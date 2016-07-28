#ifndef __SLP_OSC_H__
#define __SLP_OSC_H__


#include "osc.h"

class SLP_OSC_Class: public OSC_Base {
 protected:
  virtual void _tx(const char *buffer, size_t len);
 public:
  SLP_OSC_Class(void);
  void rx(uint8_t *payload, size_t len);
};



#endif /* __SLP_OSC_H__ */
