// Asynchronous function calls using dukev
var dukev = require("dukev");

var AsyncCall = function (func/*, ...args */) {
	this.func = func;
	this.args = Array.prototype.slice.call(arguments, 1);
}

AsyncCall.prototype.schedule = function (loop) {
	var acall = this;
	var async = new dukev.Async(function () {
		async.stop();
		acall.func.apply(undefined, acall.args);
	});
	async.start(loop).send();
	return this;
}

module.exports = AsyncCall;
