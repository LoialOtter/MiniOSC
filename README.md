# MiniOSC

This is a minimalistic or at least a smaller implementation of OSC over a few protocols.

The supported interfaces are
- SLP (a simple serial protocl)
- UDP (using ESPAsyncUDP)
- websockets (using WebSocketsServer)

The library also has a global "OSC" class and object which can be used to broadcast to all interfaces.

---

## Paths/Addresses

The path system used for matching OSC addresses supports wildcards in both the onMessage side as well as the incoming messages. This is by design as the intended use is as a gateway between network types as well as working locally.

All OSC wildcards are supported along with an extra one '**' which indicates to match anything past that point.

Example matches:
* "**"
..* will match any address
* "/test**"
..* "/test" will matches
..* "/test/foo" will match
* "/test/**"
..* "/test" will not match
..* "/test/foo" will match

The search system is written in reasonably tight C with minimal allocation.

---

## Send method

The send method uses va_args (printf-like) which is parsed using the typestring. This means that it's very easy and quite efficient to send messages and requires no structures to be built.


## Receive

The library is meant to be lightweight so the message is not copied at all until you need to parse the values from it.

Within the callback from onMessage, you can call "OSC.parseData" on the message to have it allocate and parse the data out. Note that the typestring is still available without calling that.
After calling parseData, you'll need to call freeData to free the allocated memory.

---

## SLP Transport

One of the transport methods is what I call 'SLP'. I made this protocol to talk to an ARM controller that normally runs beside the wifi module on my boards. SLIP was initially used but required too much overhead to handle the escaped bytes. Instead I used a simple format with some error detection.

Name     | Length(bytes) | Description
-------- | ------------- | -----------
Boundary | 1             | Delimiter byte - generally 0x5C 
Endpoint | 1             | To select endpoints, if multiple are used (also used for filtering)
Length   | 1             | Length of the packet / 4
CRC      | 2             | CRC16 of the data portion of the message
Data     | Length*4      | Payload
Boundary | 1             | same as above (optional) - to resync look for these bytes

The length field allows a maximum of 1024 bytes and relies on the fact that OSC is always transfered in 4-byte blocks.

In the accompanying microcontroller, I monitor for the boundary bytes in the ISR and receive to the end of the CRC then enable DMA to receive the rest. The last boundary byte is optional.

The library includes a special mode for debugging the interface (making it easily readable over serial). Just set OSC_DEBUG_SLP and it'll instead print an escapified version.

---

## Extra notes

At this point the library has no handling of #bundles or time-related tasks. This is on the to-do list.

