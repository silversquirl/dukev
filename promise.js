var Promise = function (executor) {
	executor(this._resolve, this._reject);
}

Promise.all = function () {}
Promise.allSettled = function () {}
Promise.race = function () {}
Promise.reject = function () {}
Promise.resolve = function () {}

Promise.prototype.catch = function () {}
Promise.prototype.finally = function () {}
Promise.prototype.then = function () {}

exports = Promise;
