var dukev = require("dukev");
var loop = dukev.default;

var async = new dukev.Async(function (w) {
	print("I was asynced!");
	w.stop();
}).start(loop);

new dukev.Timer(function (w) {
	print("Timer was timed");
	print("Asyncing the asyncer");
	async.send();
}, 1).start(loop);

print("Starting event loop");
loop.run();
print("Event loop finished");
