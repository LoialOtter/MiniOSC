#include "UDP_OSC.h"

static void udp_osc_rx(void* args, AsyncUDPPacket& packet) {
  OSC_Base* _this = (OSC_Base*)args;
  _this->Receive((char*)packet.data(), packet.length(), _this);
}


UDPOSC_Class::UDPOSC_Class(int rx_port, int tx_port) {
  this->rx_port = rx_port;
  this->tx_port = tx_port;
  if (udp.listen(rx_port)) {
    udp.onPacket(&udp_osc_rx, (void*)this);
  }
}


void UDPOSC_Class::_tx(const char *buffer, size_t len) {
  udp.broadcastTo((uint8_t*)buffer, len, tx_port);
}

void UDPOSC_Class::rx(AsyncUDPPacket& packet) {
}
