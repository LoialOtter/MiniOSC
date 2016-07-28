#ifndef __UDP_OSC_H__
#define __UDP_OSC_H__


#include "osc.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncUDP.h>

class UDPOSC_Class: public OSC_Base {
 protected:
  AsyncUDP udp;
  virtual void _tx(const char *buffer, size_t len);
  void rx(AsyncUDPPacket& packet);
 public:
  int rx_port, tx_port;

  UDPOSC_Class(int rx_port, int tx_port);
  //void rx(uint8_t *payload, size_t len);
};







#endif /* __WEBSOCKET_OSC_H__ */
