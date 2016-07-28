#ifndef __OSC_H__
#define __OSC_H__

#include <Arduino.h>
#include <cbuf.h>
#include <WebSocketsServer.h>
#include "PathSearch.h"
//#include <list.h>

//#define OSC_CBUF_MEMORY true

#ifdef OSC_CBUF_MEMORY
#include "cbuf_malloc.h"

#ifndef OSC_TX_BUF_SIZE
  #define OSC_TX_BUF_SIZE 512
#endif
#ifndef OSC_RX_BUF_SIZE
  #define OSC_RX_BUF_SIZE 512
#endif
#endif

//// this defines the maximum number of data slots in a message
//// Please note that data beyond this will be ignored
//#ifndef OSC_DATA_POOL_SIZE
//  #define OSC_DATA_POOL_SIZE  8
//#endif


class OSC;

struct osc_message_struct;

typedef enum {
  TYPE_UNKNOWN = -1,
  TYPE_UNUSED = 0,
  TYPE_MESSAGE,
  TYPE_STRING,
  TYPE_BLOB,
  TYPE_INTEGER,
  TYPE_FLOAT,
  TYPE_NONE,
  TYPE_TRUE,
  TYPE_FALSE,
  TYPE_IMPULSE,
  TYPE_TIMETAG,
  TYPE_LONG,
  TYPE_DOUBLE,
  TYPE_ADVBLOB,
  TYPE_CHARACTER,
  TYPE_COLOR,
  TYPE_MIDI
} osc_data_type;

typedef struct data_holder_type {
  union { // this is kidna convoluted but it allows different types to have different headers
    // it also shows some of the usage;
    struct {
      osc_data_type type : 8;
      unsigned is_array : 1;
      unsigned array_length : 7;
    };
    struct { // TYPE_MESSAGE
      unsigned reserved_type_message1 : 16;
      unsigned path_length : 8;
      unsigned offset_to_data : 8;
    };
    uint32_t raw_header;
  };
  union {
    uint32_t value_uint32;
    uint64_t value_uint64;
    sint32_t value_sint32;
    sint64_t value_sint64;
    float    value_float;
    double   value_double;
    uint8_t  value_bytes[8];
    uint32_t value_timestamp[2];
    uint32_t value_words[2];
    struct {
      uint8_t value_red, value_green, value_blue, value_alpha;
    };
    struct {
      union {
        u32 length;
        struct {
          unsigned crc16       : 16;
          unsigned shortlength : 8;
          unsigned endpoint    : 6;
          unsigned valid       : 1; // for marking if CRC is correct
          unsigned marker      : 1;
        };
      };
      union {
        const char *path;
        char *value_string;
        uint8_t *value_blob;
      };
    };
  };
} data_holder_type;

class OSC_Base;
typedef struct osc_message_struct {
  data_holder_type header;

  const char *typetag;
  size_t typetag_length;

  data_holder_type *data;
  unsigned data_count : 8;
  unsigned local : 1;

  OSC_Base *origin;
} osc_message;

typedef struct {
  OSC_Base *interface;
  osc_message *message;
} osc_on_message_args;

//typedef enum {
//  NONE = 0,
//  UART = 1,
//  UDP = 2,
//  TCP = 3,
//  WebSocket = 4
//} InterfaceTypeEnum;



//typedef void (*OSC_MessageHandler)(const char* path, osc_message *message);

typedef enum {
  MESSAGE_RECEIVED = 0,
  BUNDLE_RECEIVED = 1,
  
  ERROR_INCOMPLETE = -1,
  ERROR_PARSE      = -2
} receive_return_type;

class OSC_Base {
 protected:
  // virtual functions - must be implemented
  virtual void _tx(const char *buffer, size_t len) = 0;

  // functions implemented in base class
  size_t _Send(const char *path, const char *typestring, va_list ap);
  size_t _Send(osc_message* message);
  receive_return_type _Receive(const char *buffer, size_t len, OSC_Base *origin);


#ifdef OSC_CBUF_MEMORY
  cbuf_malloc_type _tx_buffer;
  cbuf_malloc_type _rx_buffer;
#endif
 public:

  pathsearch_Object ps;

  OSC_Base(void);
  
  virtual void Start(void);
  virtual void Start(char *_tx_buffer, size_t _tx_buffer_len, char *_rx_buffer, size_t _rx_buffer_len);

  receive_return_type Receive(const char *buffer, size_t len, OSC_Base *origin = NULL); // this is for injection into the system
  //receive_return_type Receive(const char *buffer, size_t len, bool local);
  size_t Send(const char *path, const char *typestring, ...);
  size_t Send(osc_message* message);
  size_t Send(const char *buffer, size_t len);
  
  virtual void onMessage(const char *path, pathsearch_MatchHandler callback);
  virtual void onBundle(pathsearch_MatchHandler callback);

  virtual void remove_onMessage(pathsearch_MatchHandler callback);
  void remove_onMessage(const char *path);
  void remove_onBundle(void);
  
  int parseData(osc_message* message);
  void freeData(osc_message* message);
};




struct interface_list_struct;
typedef struct interface_list_struct {
  interface_list_struct* next;
  interface_list_struct* prev;
  OSC_Base *interface;
} interface_list_type;


class OSC_Class: public OSC_Base {
 protected:
  interface_list_struct* _head;
  
  void _tx(const char *buffer, size_t len);

 public:
  OSC_Class(void);

  void Start(void);
  void Start(char *_tx_buffer, size_t _tx_buffer_len, char *_rx_buffer, size_t _rx_buffer_len);

  void addInterface(OSC_Base *interface);
  void removeInterface(OSC_Base *interface);

  void Repeat(const char* path, void* args);

  // these only will work on the currently added interfaces, If you want
  // handlers on all interfaces, make sure you call these after all interfaces
  // are added
  virtual void onMessage(const char *path, pathsearch_MatchHandler callback);
  virtual void remove_onMessage(pathsearch_MatchHandler callback);
};


void osc_repeat(const char* path, void* args);

extern OSC_Class OSC;



#endif /* __OSC_H__ */
