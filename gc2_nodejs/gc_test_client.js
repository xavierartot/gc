var net = require('net');

var client = new net.Socket();
client.connect(7001, '127.0.0.1', function() {
    console.log('connected to server');
    // client.write('Hello, server! Love, Client.');
    
    // create buffer
    var buffer = new Buffer(200);
    // write device id
    buffer.writeInt32BE(42,0);
    // write protocol version
    buffer.writeInt32BE(5,4);
    // write realtime mode
    buffer.writeInt32BE(1, 8);
    
    client.write(buffer);
    console.log("wrote buffer");
});