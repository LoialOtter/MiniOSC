#ifndef __WEBSOCKET_OSC_H__
#define __WEBSOCKET_OSC_H__


#include "osc.h"

class WebsocketOSC_Class: public OSC_Base {
 protected:
  WebSocketsServer* webSocket;
  virtual void _tx(const char *buffer, size_t len);
 public:
  WebsocketOSC_Class(WebSocketsServer* socket);
  void rx(uint8_t *payload, size_t len);
};







#endif /* __WEBSOCKET_OSC_H__ */
