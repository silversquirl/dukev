var dukev = require("dukev");
var AsyncCall = require("dukev/async-call");
var Promise = require("dukev/promise");

var printIt = function (value) {
	return function () { print(value); };
}

// An AsyncCall is required because everything related to promises has to be run within the event loop to work
new AsyncCall(function () {
	Promise.resolve("resolve->then").then(print);
	Promise.reject("reject->catch").catch(print);
	Promise.resolve().finally(printIt("resolve->finally"));
	Promise.reject().finally(printIt("reject->finally"));
}).schedule(dukev.default);
dukev.default.run();

print("");

new AsyncCall(function () {
	Promise.resolve("resolve->throw->catch")
		.then(function (value) {
			throw value;
		})
		.catch(print);
	Promise.reject("reject->throw->catch")
		.catch(function (value) {
			throw value;
		})
		.catch(print);
}).schedule(dukev.default);
dukev.default.run();

print("");

new AsyncCall(function () {
	var res1 = Promise.resolve(1);
	var res2 = Promise.resolve(2);
	var rej1 = Promise.reject(1);
	var rej2 = Promise.reject(2);

	Promise.all([]).then(print_repr);
	Promise.all([1, 2]).then(print_repr);
	Promise.all([res1, res2]).then(print_repr);
	Promise.all([res1, rej2]).catch(print_repr);
	Promise.all([rej1, rej2]).catch(print_repr);
}).schedule(dukev.default);
dukev.default.run();

print("");

new AsyncCall(function () {
	var res = Promise.resolve("resolved");
	var rej = Promise.reject("rejected");
	var wait = new Promise(function () {});

	Promise.race([res, wait]).then(print).catch(print_repr);
	Promise.race([wait, res]).then(print);
	Promise.race([rej, wait]).catch(print);
	Promise.race([wait, rej]).catch(print);
}).schedule(dukev.default);
dukev.default.run();
