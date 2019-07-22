var dukev = require("dukev");
var AsyncCall = require("dukev/async-call");
var loop = dukev.default;

new AsyncCall(print, "Async call was asyncly called!").schedule(loop);

print("Starting event loop");
loop.run();
print("Event loop finished");
