var dukev = require("dukev");
var AsyncCall = require("dukev/async-call");

// Promise states
var pending = "pending";
var fulfilled = "fulfilled";
var rejected = "rejected";

// Hidden properties
var valueK = Symbol();
var onResolveK = Symbol();
var onRejectK = Symbol();

var scheduleCallback = function scheduleCallback(promise) {
	var cb;
	if (promise.state === fulfilled) {
		cb = promise[onResolveK];
	} else if (promise.state === rejected) {
		cb = promise[onRejectK];
	}

	// If there's no callback for this state of if the promise is still pending, do nothing
	if (!cb) return;

	// Otherwise, schedule the callback with the default event loop
	new AsyncCall(cb, promise[valueK]).schedule(dukev.default);
}

var Promise = function Promise(executor) {
	this.state = pending;
	this[valueK] = undefined;
	this[onResolveK] = undefined;
	this[onRejectK] = undefined;

	var promise = this;
	var resolve = function resolve(value) {
		promise[valueK] = value;
		if (promise.state == fulfilled) return;
		promise.state = fulfilled;
		scheduleCallback(promise);
	};

	var reject = function reject(value) {
		promise[valueK] = value;
		if (promise.state == rejected) return;
		promise.state = rejected;
		scheduleCallback(promise);
	};

	try {
		executor(resolve, reject);
	} catch (err) {
		reject(err);
	}
}

Promise.reject = function Promise_reject(reason) {
	return new Promise(function (resolve, reject) { reject(reason); });
};
Promise.resolve = function Promise_resolve(reason) {
	return new Promise(function (resolve, reject) { resolve(reason); });
};

// all, allSelected and race are not thread-safe, but that's fine because Promises are tied to the default dukev loop anyway
// Also, duktape doesn't support for...of, so iterators aren't a thing. Because of this, the three aforementioned functions expect arrays

var clearPromiseCallbacks = function clearPromiseCallbacks(promise) {
	promise[onResolveK] = undefined;
	promise[onRejectK] = undefined;
}

Promise.all = function Promise_all(promises) {
	return new Promise(function (resolve, reject) {
		if (promises.length == 0) return resolve(promises);

		var numPending = promises.length;
		var rejected = false;

		var completePendingPromise = function completePendingPromise() {
			numPending--;
			if (numPending <= 0) resolve(promises);
		};

		for (var i = 0; i < promises.length; i++) {
			if (promises[i] instanceof Promise) {
				(function (i) {
					promises[i].then(function (value) {
						promises[i] = value;
						completePendingPromise();
					}, function (value) {
						reject(value);
						promises.forEach(clearPromiseCallbacks);
					});
				})(i);
			} else {
				completePendingPromise();
			}
		}
	});
}

// TODO: allSelected

Promise.race = function Promise_race(promises) {
	return new Promise(function (resolve, reject) {
		var onResolve = function (value) {
			promises.forEach(clearPromiseCallbacks);
			resolve(value);
		};

		var onReject = function (value) {
			promises.forEach(clearPromiseCallbacks);
			reject(value);
		};

		for (var i = 0; i < promises.length; i++) {
			promises[i].then(onResolve, onReject);
		}
	});
}

Promise.prototype.catch = function Promise_catch(onReject) {
	return this.then(undefined, onReject);
};

Promise.prototype.finally = function Promise_finally(onFinally) {
	return this.then(function (value) {
		onFinally();
		return value;
	}, function (value) {
		onFinally();
		throw(value);
	});
};

Promise.prototype.then = function Promise_then(onResolve, onReject) {
	var promise = this;
	return new Promise(function (resolve, reject) {
		if (onResolve) {
			promise[onResolveK] = function (value) {
				try {
					resolve(onResolve(value));
				} catch (err) {
					reject(err);
				}
			};
		} else {
			promise[onResolveK] = resolve;
		}

		if (onReject) {
			promise[onRejectK] = function (value) {
				try {
					reject(onReject(value));
				} catch (err) {
					reject(err);
				}
			}
		} else {
			promise[onRejectK] = reject;
		}

		// This will only call the callbacks if they need triggering
		scheduleCallback(promise);
	});
};

module.exports = Promise;
