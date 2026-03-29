// DOM manipulation methods — loaded separately to avoid QuickJS bytecode
// stack issues when compiled as part of a large file.

Element.prototype.appendChild = function(child) {
    if (!(child instanceof Element)) return child;
    if (child._parentElement) child._parentElement.removeChild(child);
    child._parentElement = this;
    this._children.push(child);
    this._ensureNative();
    if (!child._nativeCreated) {
        _reparentNative(child, this._id);
    } else {
        removeWidget(child._id);
        child._nativeCreated = false;
        _reparentNative(child, this._id);
    }
    if (child._textContent) setText(child._id, child._textContent);
    child.style._flushAll();
    child._reapplyStylesheets();
    return child;
};

Element.prototype.insertBefore = function(newChild, refChild) {
    if (!refChild) return this.appendChild(newChild);
    if (newChild._parentElement) newChild._parentElement.removeChild(newChild);
    var idx = this._children.indexOf(refChild);
    if (idx < 0) return this.appendChild(newChild);
    newChild._parentElement = this;
    this._children.splice(idx, 0, newChild);
    this._ensureNative();
    if (!newChild._nativeCreated) {
        _reparentNative(newChild, this._id);
    } else {
        removeWidget(newChild._id);
        newChild._nativeCreated = false;
        _reparentNative(newChild, this._id);
    }
    if (newChild._textContent) setText(newChild._id, newChild._textContent);
    newChild.style._flushAll();
    newChild._reapplyStylesheets();
    return newChild;
};

Element.prototype.removeChild = function(child) {
    var idx = this._children.indexOf(child);
    if (idx < 0) return child;
    this._children.splice(idx, 1);
    child._parentElement = null;
    if (child._nativeCreated) removeWidget(child._id);
    child._nativeCreated = false;
    return child;
};

Element.prototype.remove = function() {
    if (this._parentElement) this._parentElement.removeChild(this);
};

Element.prototype.replaceChild = function(newChild, oldChild) {
    var idx = this._children.indexOf(oldChild);
    if (idx < 0) return oldChild;
    this.removeChild(oldChild);
    if (idx < this._children.length) {
        this.insertBefore(newChild, this._children[idx]);
    } else {
        this.appendChild(newChild);
    }
    return oldChild;
};
