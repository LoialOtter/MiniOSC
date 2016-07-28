#include "WebSocket_OSC.h"


void WebsocketOSC_Class::_tx(const char *buffer, size_t len) {
  webSocket->broadcastBIN((uint8_t*)buffer, len);
}

WebsocketOSC_Class::WebsocketOSC_Class(WebSocketsServer *socket)
{
  webSocket = socket;
}

void WebsocketOSC_Class::rx(uint8_t *payload, size_t len) {
  _Receive((char*)payload, len, this);
}
