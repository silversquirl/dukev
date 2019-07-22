var dukev = require("dukev");
var loop = dukev.default;

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

print("Starting event loop");
loop.run();
print("Event loop finished");
