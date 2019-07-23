var File = require("io").File;

var path = "examples/testfile.txt";

function printBuf(buf) {
	print(new TextDecoder().decode(buf));
}

// Low level IO primitives
var f = File.open(path);
var buf = new Uint8Array(1024);
buf = buf.subarray(0, f.read(buf));
f.close();
printBuf(buf);
delete buf, f;

// Higher level wrapper
var f = File.open(path);
try {
	var data = f.readAll();
	printBuf(data);
} finally {
	f.close();
}
delete data, f;

// Even more high level wrapper
var data = File.read(path);
printBuf(data);
delete data;

// Explicit flags with openFile
var f = File.openFile(path, File.O_RDONLY);
try {
	var data = f.readAll();
	printBuf(data)
} finally {
	f.close();
}

// This should fail due to invalid open flags
var f = File.openFile(path, File.O_WRONLY);
try {
	f.readAll();
} catch (e) {
	print("Got an error reading data, as expected!");
} finally {
	f.close();
}
