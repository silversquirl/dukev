var dukev = require('dukev');
print("Hello, world!");
var loop = new dukev.Loop();

var timer = new dukev.Timer(function () {
	print("Callback has been called back!");
}, 3);
timer.start(loop)

var i = 0;
var timer2 = new dukev.Timer(function (w) {
	if (i >= 5) {
		print("That's enough pinging for me!");
		w.stop();
	} else {
		print("ping");
	}
	i++;
		
}, 1, 1);
timer2.start(loop);

loop.run();
print("First event loop finished");

var async = new dukev.Async(function (w) {
	print("I was asynced!");
	w.stop();
}).start(loop);

new dukev.Timer(function (w) {
	print("Timer was timed");
	print("Asyncing the asyncer");
	async.send();
}, 2).start(loop);


loop.run();
print("Second event loop done");
print("All done");
