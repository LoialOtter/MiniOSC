# MiniOSC

This is a minimalistic or at least a smaller implementation of OSC over a few protocols.

The supported interfaces are
- SLP (a simple serial protocl)
- UDP (using ESPAsyncUDP)
- websockets (using WebSocketsServer)

The library also has a global "OSC" class and object which can be used to broadcast to all interfaces.


