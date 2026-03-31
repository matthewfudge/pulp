# evaluateExpression crashes on objects with circular references (infinite recursion in toChocValue)

## Description

`choc::javascript::Context::evaluateExpression()` crashes with a SIGSEGV (stack overflow) when the returned JS value contains circular references. The `toChocValue()` method in `choc_javascript_QuickJS.h` recursively walks all object properties without any cycle detection, so mutually referencing objects cause infinite recursion.

This comes up naturally when working with DOM-like structures where elements have parent/child pointers — any `evaluateExpression` that returns such an object crashes the process.

## Minimal reproduction

```cpp
#include "choc_javascript.h"

int main()
{
    auto ctx = choc::javascript::createQuickJSContext();

    // Create a simple circular reference: a.child.parent === a
    ctx.evaluateExpression(
        "var a = {}; var b = {}; a.child = b; b.parent = a; a"
    );
    // ^ crashes here — toChocValue recurses infinitely on a→b→a→b→...
}
```

Even simpler:

```cpp
ctx.evaluateExpression("var x = {}; x.self = x; x");
```

## Stack trace (ASan, abbreviated)

```
ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
Hint: address points to the zero page.
    #0 operator new(unsigned long)
    #1-#7 std::vector / choc::value::Value construction
    #8 choc::value::Value::Value(choc::value::Type&&) choc_Value.h:2614
    #10 choc::value::createString(...) choc_Value.h:2702
    #12 QuickJSContext::ValuePtr::toChocValue() choc_javascript_QuickJS.h:64202
    #13 QuickJSContext::ValuePtr::toChocValue() choc_javascript_QuickJS.h:64263  ← recursive call
    #14 QuickJSContext::ValuePtr::toChocValue() choc_javascript_QuickJS.h:64263
    ... (repeats 50+ times until stack overflow)
```

## Where the problem is

In `choc_javascript_QuickJS.h`, `QuickJSContext::ValuePtr::toChocValue()` around line 64263:

```cpp
for (auto& propName : propNames)
    o.setMember (propName, (*this)[propName.c_str()].toChocValue());
```

There's no tracking of which objects have already been visited, so any cycle in the object graph becomes infinite recursion.

## Workaround

We're currently appending `";void 0"` to eval strings so the return value is `undefined` instead of the circular object. Works but obviously limits what you can get back from JS.

Important nuance: this only works when the suffix is appended at the very end of the evaluated string, so the final expression result is `undefined` rather than the last user value. If it appears earlier in the script, it doesn't prevent CHOC from trying to convert a later circular object.

## Upstream status

Tracktion fixed this upstream in CHOC commit `13fe8685d3d9d0458c8361fcea29ecb471d5582f`.

As of March 30, 2026, Pulp's fresh `FetchContent` configure already picks up a newer CHOC `main` revision that contains this fix. That means a clean build should no longer need the workaround for circular return values.

However, the repo still uses `GIT_TAG main` for CHOC, so this is not yet a stable, reproducible guarantee for all contributors. The right follow-up is:

1. Pin CHOC to a known-good commit at or after `13fe8685d3d9d0458c8361fcea29ecb471d5582f`.
2. Add a regression test that evaluates a circular JS object and proves Pulp no longer crashes.
3. Remove only the specific `";void 0"` workaround sites that were guarding this CHOC bug.

Not every `void 0` in the repo is part of this workaround. Some uses intentionally discard a script result and may still be valid after the CHOC fix.

## Possible fix direction

A visited-object set (e.g. tracking `JSValue` pointers already seen) would be the thorough solution. A simpler approach that would catch 99% of cases would be a recursion depth limit — something like 32 levels deep is more than enough for any realistic object graph, and anything deeper is almost certainly a cycle. Either way, hitting the limit could return a `choc::value::Value()` or a placeholder string like `"[circular]"` rather than crashing.

## Environment

- CHOC: latest main as of March 2026
- Compiler: Apple Clang 16 (arm64), ASan enabled
- OS: macOS 26.3.1
