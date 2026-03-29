// DOM manipulation methods

Element.prototype.appendChild = function(child) {
    console.log("appendChild ENTER");
    if (!(child instanceof Element)) { console.log("not Element"); return child; }
    console.log("appendChild: instanceof OK");
    if (child._parentElement) child._parentElement.removeChild(child);
    console.log("appendChild: removeChild OK");
    child._parentElement = this;
    this._children.push(child);
    console.log("appendChild: push OK, calling _ensureNative");
    this._ensureNative();
    console.log("appendChild: _ensureNative OK, calling __domAppend");
    __domAppend(this._id, child._id, child.tagName.toLowerCase());
    console.log("appendChild: __domAppend OK");
    child._nativeCreated = true;
    return child;
};

Element.prototype.removeChild = function(child) {
    var idx = this._children.indexOf(child);
    if (idx < 0) return child;
    this._children.splice(idx, 1);
    child._parentElement = null;
    if (child._nativeCreated) __domRemove(child._id);
    child._nativeCreated = false;
    return child;
};

Element.prototype.remove = function() {
    if (this._parentElement) this._parentElement.removeChild(this);
};

Element.prototype.insertBefore = function(newChild, refChild) {
    if (!refChild) return this.appendChild(newChild);
    var idx = this._children.indexOf(refChild);
    if (idx < 0) return this.appendChild(newChild);
    if (newChild._parentElement) newChild._parentElement.removeChild(newChild);
    newChild._parentElement = this;
    this._children.splice(idx, 0, newChild);
    this._ensureNative();
    __domAppend(this._id, newChild._id, newChild.tagName.toLowerCase());
    newChild._nativeCreated = true;
    return newChild;
};

Element.prototype.replaceChild = function(newChild, oldChild) {
    var idx = this._children.indexOf(oldChild);
    if (idx < 0) return oldChild;
    this.removeChild(oldChild);
    this.appendChild(newChild);
    return oldChild;
};
