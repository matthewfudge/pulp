// Minimal runtime shim for Pulp's QuickJS environment, which lacks
// setTimeout / clearTimeout / queueMicrotask. React-reconciler in
// LegacyRoot mode references setTimeout for fallback scheduling but
// doesn't actually depend on it firing for synchronous renders. The
// stubs here let the bundle evaluate without crashing.
(function () {
    var g = globalThis;
    if (typeof g.setTimeout !== 'function') {
        g.setTimeout = function (fn) { return -1; };
    }
    if (typeof g.clearTimeout !== 'function') {
        g.clearTimeout = function () {};
    }
    if (typeof g.queueMicrotask !== 'function') {
        // QuickJS exposes Promise.resolve().then for microtask scheduling.
        // We don't need to await — the React reconciler only schedules
        // microtasks for post-commit work that the synchronous render
        // path doesn't depend on.
        g.queueMicrotask = function (fn) {
            try { fn(); } catch (e) { /* swallow — matches browser semantics */ }
        };
    }
    if (typeof g.Promise === 'undefined') {
        // Defensive: QuickJS has Promise. If it ever doesn't, everything
        // else will already be on fire — this is just a guard rail.
        g.Promise = { resolve: function () { return { then: function (fn) { fn(); } }; } };
    }
    if (typeof g.console === 'undefined') {
        g.console = { log: function () {}, error: function () {}, warn: function () {} };
    }
})();
