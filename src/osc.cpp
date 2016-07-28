#include "osc.h"

#define MAX_PATH_LENGTH 64
#define MAX_TYPESTRING_LENGTH 16


#ifdef OSC_CBUF_MEMORY
#define local_tx_malloc(x) cbuf_malloc(&_tx_buffer, x)
#define local_rx_malloc(x) cbuf_malloc(&_rx_buffer, x)
#define local_free(x)
#else
#define local_tx_malloc(x) malloc(x)
#define local_rx_malloc(x) malloc(x)
#define local_free(x) free(x)
#endif


OSC_Class OSC;


OSC_Base::OSC_Base(void) {
}



void OSC_Base::Start(void) {
#ifdef OSC_CBUF_MEMORY
  char *_tx_buffer_raw = (char*)malloc(512);
  char *_rx_buffer_raw = (char*)malloc(512);
  cbuf_static_init(&_tx_buffer, _tx_buffer_raw, 512);
  cbuf_static_init(&_rx_buffer, _rx_buffer_raw, 512);
#endif
  pathsearch_init(&ps);
  //Serial.printf("head: 0x%08X\r\n", (u32) ps.head);
  ////Serial.printf("tail: 0x%08X\r\n", (u32) ps.tail);
}
void OSC_Base::Start(char *_tx_buffer_raw, size_t _tx_buffer_len, char *_rx_buffer_raw, size_t _rx_buffer_len) {
#ifdef OSC_CBUF_MEMORY
  cbuf_static_init(&_tx_buffer, _tx_buffer_raw, _tx_buffer_len);
  cbuf_static_init(&_rx_buffer, _rx_buffer_raw, _rx_buffer_len);
#endif
  pathsearch_init(&ps);
  //Serial.printf("head: 0x%08X\r\n", (u32) ps.head);
  //Serial.printf("tail: 0x%08X\r\n", (u32) ps.tail);
}



size_t OSC_Base::_Send(const char *path, const char *typestring, va_list ap) {
  const char *typestring_beginning = typestring;
  char *buffer; 
  data_holder_type data_holder;
  uint32_t i;
  int position = 0;
  uint8_t *src, *dst;
  uint32_t total_length;
  va_list ap_size;

  for (i = 0; i < MAX_PATH_LENGTH && path[i]; i++);
  total_length = (i+4)&(~3);
  for (i = 0; i < MAX_TYPESTRING_LENGTH && typestring[i]; i++);
  total_length += (i+4)&(~3);

  // find the size of all the data members (and count the length of the typestring)
  va_copy(ap_size, ap); // we need to use va_args twice
  for (i = 0; i < MAX_TYPESTRING_LENGTH && typestring[i]; i++) {
    // get the first character out of the way
    switch (typestring[i]) {
    case 'c': // TYPE_CHARACTER; 
    case 'i': // TYPE_INTEGER;
    case 'm': // TYPE_MIDI;
    case 'r': // TYPE_RGB;
      total_length+=4; data_holder.value_uint32 = va_arg(ap_size, uint32_t); break;
    case 'f': // TYPE_FLOAT;
      total_length+=4; data_holder.value_float  = (float)va_arg(ap_size, double); break; 

    case 'd': // TYPE_DOUBLE;
    case 'h': // TYPE_LONG;
    case 't': // TYPE_TIMETAG;
      total_length+=8; data_holder.value_sint64 = va_arg(ap_size, sint64_t); break;

    case 's':
      data_holder.value_string = va_arg(ap_size, char*);
      data_holder.length = strlen(data_holder.value_string);
      total_length += (data_holder.length+3)&(~3);
      break; // TYPE_STRING;
      
    case 'b': // TYPE_BLOB
    case 'B':
      data_holder.length       = va_arg(ap_size, uint32_t);
      data_holder.value_blob   = va_arg(ap_size, uint8_t*);
      total_length += 4 + (data_holder.length+3)&(~3);
      break; // TYPE_BLOB with the advanced length field

    }
  }
  va_end(ap_size);
  //total_length += (i+3)&(~3);

  buffer = (char*)local_tx_malloc(total_length);
  if (!buffer) return 0;
  memset(buffer, 0x00, total_length);
  
  memset(&data_holder, 0x00, sizeof(data_holder_type));

  // name
  for (; position < total_length &&       *path != 0x00; position++) buffer[position] = *path++;
  buffer[position++] = 0x00;
  for (; (position & 0x03) != 0x00; position++) buffer[position] = 0x00;

  // typestring
  for (; position < total_length && *typestring != 0x00; position++) buffer[position] = *typestring++;
  buffer[position++] = 0x00;
  for (; (position & 0x03) != 0x00; position++) buffer[position] = 0x00;

  // now onto the data
  typestring = typestring_beginning;

  
  for (;position < total_length && *typestring != 0x00; typestring++) {
    // get the first character out of the way
    switch (*typestring) {
    case 'c': data_holder.value_uint32 = va_arg(ap, uint32_t);      break; // TYPE_CHARACTER; 
    case 'i': data_holder.value_sint32 = va_arg(ap, int);           break; // TYPE_INTEGER;
    case 'd': data_holder.value_double = va_arg(ap, double);        break; // TYPE_DOUBLE;
    case 'f': data_holder.value_float  = (float)va_arg(ap, double); break; // TYPE_FLOAT;
    case 'm': data_holder.value_uint32 = va_arg(ap, uint32_t);      break; // TYPE_MIDI;
    case 'r': data_holder.value_uint32 = va_arg(ap, uint32_t);      break; // TYPE_COLOR;

      // please note, these are work-arounds to an issue where va_arg requires 64-bit variables
      // to be located on 8-byte boundaries while the stack pushes 64bit variables without this requirement.
    case 'h': data_holder.value_sint64 = va_arg(ap, sint64_t); break;
    case 't': data_holder.value_sint64 = va_arg(ap, sint64_t); break;

    case 's': data_holder.value_string = va_arg(ap, char*);    break; // TYPE_STRING;
    case 'b': data_holder.length       = va_arg(ap, uint32_t);
              data_holder.value_blob   = va_arg(ap, uint8_t*); break; // TYPE_BLOB
    case 'B': data_holder.length       = va_arg(ap, uint32_t);
              data_holder.value_blob   = va_arg(ap, uint8_t*);
              data_holder.crc16        = 0;// TODO: crc16(0, data_holder.value_blob, (data_holder.shortlength << 2)); <<<<============================================== THIS NEEDS FIXING
              data_holder.marker       = 1; break; // TYPE_BLOB with the advanced length field
    case 'F': break; // TYPE_FALSE;
    case 'I': break; // TYPE_IMPULSE;
    case 'N': break; // TYPE_NONE;
    case 'T': break; // TYPE_TRUE;
    case ',': break;
    }


    // now copy the data into place
    switch (*typestring) {
    case 'c': // TYPE_CHARACTER;
    case 'f': // TYPE_FLOAT;
    case 'i': // TYPE_INTEGER;
    case 'm': // TYPE_MIDI;
    case 'r': // TYPE_RGB;
      //*(uint32_t*)(&buffer[position]) = data_holder.value_uint32;
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[3];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[2];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[1];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[0];
      //position += 4;
      break;

    case 'd': // TYPE_DOUBLE;
    case 'h': // TYPE_LONG;
    case 't': // TYPE_TIMETAG;
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[7];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[6];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[5];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[4];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[3];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[2];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[1];
      *(uint8_t*)(&buffer[position++]) = data_holder.value_bytes[0];
      //*(uint64_t*)(&buffer[position]) = data_holder.value_uint64;
      //position += 8;
      break;

    case 'b': // TYPE_BLOB;
      position += 4;
      src = (uint8_t*)data_holder.value_blob;
      dst = (uint8_t*)&buffer[position];
      for (i = 0; i < (data_holder.length+3)>>2; i++) {
        dst[0] = src[3];
        dst[1] = src[2];
        dst[2] = src[1];
        dst[3] = src[0];
        dst+=4; src+=4;
      }
      position += (data_holder.length+3) & (~3);
      break;

    case 'B': // TYPE_BLOB;
      position += 4;
      src = (uint8_t*)data_holder.value_blob;
      dst = (uint8_t*)&buffer[position];
      for (i = 0; i < data_holder.shortlength; i++) {
        dst[0] = src[3];
        dst[1] = src[2];
        dst[2] = src[1];
        dst[3] = src[0];
        dst+=4; src+=4;
      }
      position += (data_holder.length+3) & (~3);
      break;

    case 's': // TYPE_STRING;    
      data_holder.length = 0;
      for (;position < total_length && *data_holder.value_blob;) {
        buffer[position++] = *data_holder.value_blob++;
        data_holder.length++;
      }
      buffer[position++] = 0x00;
      for (; position < total_length && (position & 0x03) != 0x00; position++) buffer[position] = 0x00;
      break;

    default:
      break;
    }
  }

  _tx(buffer, total_length);
  local_free(buffer);
  return total_length;
}


size_t OSC_Base::_Send(osc_message* message) {
  if (message && message->header.path) {
    _tx(message->header.path, message->header.length);
    return message->header.length;
  }
  return 0;
}


static int stringCompareWNull(const char *buffer, const char *string) {
  int i;
  for (i = 0; string[i] != 0x00; i++) {
    if (buffer[i] != string[i]) return 0;
  }
  if (buffer[i] != 0x00) return 0;
  return 1;
}


static int stringCompare(const char *buffer, const char *string) {
  int i;
  for (i = 0; buffer[i] != 0x00 && string[i] != 0x00; i++) {
    if (buffer[i] != string[i]) return 0;
  }
  return 1;
}


static int load_32bit(const char* buffer, data_holder_type* data) {
  data->value_bytes[3] = *(uint8_t*)(&buffer[0]);
  data->value_bytes[2] = *(uint8_t*)(&buffer[1]);
  data->value_bytes[1] = *(uint8_t*)(&buffer[2]);
  data->value_bytes[0] = *(uint8_t*)(&buffer[3]);
  //data->value_uint32 = *(uint32_t*)(buffer);
  return 4;
}


static int load_64bit(const char* buffer, data_holder_type* data) {
  data->value_bytes[7] = *(uint8_t*)(&buffer[0]);
  data->value_bytes[6] = *(uint8_t*)(&buffer[1]);
  data->value_bytes[5] = *(uint8_t*)(&buffer[2]);
  data->value_bytes[4] = *(uint8_t*)(&buffer[3]);
  data->value_bytes[3] = *(uint8_t*)(&buffer[4]);
  data->value_bytes[2] = *(uint8_t*)(&buffer[5]);
  data->value_bytes[1] = *(uint8_t*)(&buffer[6]);
  data->value_bytes[0] = *(uint8_t*)(&buffer[7]);
  //data->value_uint64 = *(uint64_t*)(buffer);
  return 8;
}


static int load_string(const char* buffer, data_holder_type* data) {
  int length;
  data->value_string = (char*)buffer;
  for (length = 0; buffer[length]; length++);
  data->length = length;
  return (length+4)&(~3);
}


static int load_blob(const char* buffer, data_holder_type* data) {
  load_32bit(buffer, data);
  data->value_blob = (uint8_t*)(buffer+4);
  if (data->marker) {
    return (data->shortlength<<2)+4;
  }
  else {
    return (data->length+4 +3)&(~3);
  }
}


receive_return_type OSC_Base::_Receive(const char *buffer, size_t len, OSC_Base *origin) {
  osc_message message;
  osc_on_message_args args;
  int offset;
  data_holder_type temp_data;
  args.message = &message;
  args.interface = this;

  //Serial.println("OSC _Receive - called");
  //Serial.printf("OSC _Receive - head: 0x%08X\r\n", ps.head);

  if (len < 8) goto error_incomplete;

  message.origin = origin;

  message.header.path = buffer;
  for (message.header.path_length = 0; message.header.path[message.header.path_length] && message.header.path_length < len; message.header.path_length++);
  offset = (message.header.path_length+4) & (~3);
  if (message.header.path_length >= len || offset >= len) goto error_incomplete;

  //Serial.println("OSC _Receive - sanity checks");
  
  if (message.header.path[0] == '#') {
    //Serial.println("OSC _Receive - #bundle");
    // handle special # code messages - bundle
    if (stringCompareWNull(message.header.path, "#bundle")) {
      data_holder_type timetag;
      message.typetag = (const char*) NULL;
      message.typetag_length = 0;
      
      if (len < 20) goto error_incomplete;

      timetag.raw_header = 0;
      timetag.type = TYPE_TIMETAG;
      offset += load_64bit(&buffer[offset], &timetag);
        
      temp_data.type = TYPE_MESSAGE;
      temp_data.is_array = 1;
      offset += load_blob(&buffer[offset], &temp_data);

      if (offset >= len) goto error_incomplete;
      
      // TODO: finish this to actually handle bundles properly
      return BUNDLE_RECEIVED;
    }
    
    return ERROR_PARSE;
  }
  else {
    //Serial.println("OSC _Receive - message");
    // data handling will take place on a 'parseMessage' call, right now we just need to find the limits of the message
    message.typetag = &buffer[offset];
    for (message.typetag_length = 0; message.typetag[message.typetag_length]; message.typetag_length++);
    offset += (message.typetag_length+4) & (~3);

    //Serial.printf("OSC _Receive - path and typetag; Offset: %d, Path: %d, Typetag: %d\r\n", offset, message.header.path_length, message.typetag_length);
    
    message.header.offset_to_data = offset >> 2;
    message.data_count = message.typetag_length-1;
    
    for (int i = 0; i < message.data_count; i++) {
      switch(message.typetag[i+1]) {
      case 'c': // TYPE_CHARACTER;  offset += load_32bit(&buffer[offset], &message.data[i]); break;
      case 'f': // TYPE_FLOAT;      offset += load_32bit(&buffer[offset], &message.data[i]); break;
      case 'i': // TYPE_INTEGER;    offset += load_32bit(&buffer[offset], &message.data[i]); break;
      case 'r': // TYPE_COLOR;      offset += load_32bit(&buffer[offset], &message.data[i]); break;
        offset += 4;
        //Serial.printf("OSC _Receive - 32bit value; Offset: %d\r\n", offset);
        break;
      case 't': // TYPE_TIMETAG;    offset += load_64bit(&buffer[offset], &message.data[i]); break;
      case 'h': // TYPE_LONG;       offset += load_64bit(&buffer[offset], &message.data[i]); break;
      case 'd': // TYPE_DOUBLE;     offset += load_64bit(&buffer[offset], &message.data[i]); break;
        offset += 8;
        //Serial.printf("OSC _Receive - 64bit value; Offset: %d\r\n", offset);
        break;
      case 'F': // TYPE_FALSE;      message.data[i].value_uint32 = 0; break; 
      case 'N': // TYPE_NONE;       message.data[i].value_uint32 = 0; break;
      case 'I': // TYPE_IMPULSE;    message.data[i].value_uint32 = 1; break;
      case 'T': // TYPE_TRUE;       message.data[i].value_uint32 = 1; break;
        break;
      case 's': // TYPE_STRING;     offset += load_string(&buffer[offset], &message.data[i]); break;
        offset += load_string(&buffer[offset], &temp_data);
        //Serial.printf("OSC _Receive - string; Offset: %d\r\n", offset);
        break;
      case 'b': // TYPE_BLOB;       offset += load_blob(&buffer[offset], &message.data[i]); break;
        offset += load_blob(&buffer[offset], &temp_data);
        //Serial.printf("OSC _Receive - blob; Offset: %d\r\n", offset);
        break;
      case 'B': // TYPE_ADVBLOB;    offset += load_advblob(&buffer[offset], &message.data[i]); break;
        offset += load_blob(&buffer[offset], &temp_data);
        //Serial.printf("OSC _Receive - advblob; Offset: %d\r\n", offset);
        break;
      }
      if (offset > len) goto error_incomplete;
    }
    //Serial.println("OSC _Receive - data measured");
    message.header.length = offset;
    // -------------- handle message
    //Serial.printf("OSC _Receive - Addresses for Pathsearch: 0x%08X 0x%08X 0x%08X\r\n", &ps, message.header.path, &args);
    //Serial.printf("OSC _Receive - head: 0x%08X\r\n", ps.head);
    //Serial.printf("OSC _Receive - matches: %d\r\n", pathSearch(&ps, message.header.path, (void*)&args));
    pathSearch(&ps, message.header.path, (void*)&args);
    //Serial.println("OSC _Receive - pathsearch done");
    // -----------------------------
    //Serial.println("OSC _Receive - finished");
    return MESSAGE_RECEIVED;
  }

  // One of the only times I'll ever use gotos
  // This makes sure the allocated space, if any, is freed before exiting
 error_incomplete:
    Serial.printf("OSC _Receive - Error: Incomplete - offset: %d  len: %d\r\n", offset, len);
    return ERROR_INCOMPLETE;
 error_parse:
    Serial.println("OSC _Receive - Error: Parse");
    return ERROR_PARSE;
}


receive_return_type OSC_Base::Receive(const char *buffer, size_t len, OSC_Base *origin) {
  return _Receive(buffer, len, origin);
}


size_t OSC_Base::Send(const char *path, const char *typestring, ...) {
  va_list ap;
  sint32_t retValue;

  va_start(ap, typestring);
  retValue = this->_Send(path, typestring, ap);
  va_end(ap);
  return retValue;
}
size_t OSC_Base::Send(osc_message* message) {
  return this->_Send(message);
}
size_t OSC_Base::Send(const char* buffer, size_t len) {
  osc_message message;
  message.header.path = buffer;
  message.header.length = len;
  return this->_Send(&message);
}

void OSC_Base::onMessage(const char *path, pathsearch_MatchHandler callback) {
  addPath(&ps, path, callback);
  //Serial.printf("Address: 0x%08X\r\n", &ps);
  //Serial.printf("Head/Tail: 0x%08X, 0x%08X\r\n", (u32)ps.head, (u32)ps.tail);
  //Serial.printf("Path: 0x%08X, %s\r\n", (u32)ps.head->path, ps.head->path);
  //Serial.printf("Callback: 0x%08X\r\n", (u32)ps.head->callback, ps.head->callback);
  //Serial.printf("Next/Prev: 0x%08X, 0x%08X\r\n", (u32)ps.head->next, (u32)ps.head->prev);
}
void OSC_Base::onBundle(pathsearch_MatchHandler callback) {
  addPath(&ps, "#bundle", callback);
}

void OSC_Base::remove_onMessage(pathsearch_MatchHandler callback) {
  removePath_byCallback(&ps, callback);
}
void OSC_Base::remove_onMessage(const char* path) {
  removePath_byPath(&ps, path);
}
void OSC_Base::remove_onBundle(void) {
  removePath_byPath(&ps, "#bundle");
}

 


int OSC_Base::parseData(osc_message* message) {
  int offset = message->header.offset_to_data<<2;
  const char* buffer = message->header.path;
  
  message->data_count = message->typetag_length-1;
  message->data = (data_holder_type*)local_rx_malloc(sizeof(data_holder_type)*message->data_count);
  
  // make sure the holder is null
  memset(message->data, 0x00, sizeof(data_holder_type)*message->data_count);
  
  for (int i = 0; i < message->data_count; i++) {
    switch(message->typetag[i+1]) {
    case 'c': message->data[i].type = TYPE_CHARACTER;  offset += load_32bit(&buffer[offset], &message->data[i]); break;
    case 'f': message->data[i].type = TYPE_FLOAT;      offset += load_32bit(&buffer[offset], &message->data[i]); break;
    case 'i': message->data[i].type = TYPE_INTEGER;    offset += load_32bit(&buffer[offset], &message->data[i]); break;
    case 'r': message->data[i].type = TYPE_COLOR;      offset += load_32bit(&buffer[offset], &message->data[i]); break;
    case 't': message->data[i].type = TYPE_TIMETAG;    offset += load_64bit(&buffer[offset], &message->data[i]); break;
    case 'h': message->data[i].type = TYPE_LONG;       offset += load_64bit(&buffer[offset], &message->data[i]); break;
    case 'd': message->data[i].type = TYPE_DOUBLE;     offset += load_64bit(&buffer[offset], &message->data[i]); break;
    case 'F': message->data[i].type = TYPE_FALSE;      message->data[i].value_uint32 = 0; break; 
    case 'N': message->data[i].type = TYPE_NONE;       message->data[i].value_uint32 = 0; break;
    case 'I': message->data[i].type = TYPE_IMPULSE;    message->data[i].value_uint32 = 1; break;
    case 'T': message->data[i].type = TYPE_TRUE;       message->data[i].value_uint32 = 1; break;
    case 's': message->data[i].type = TYPE_STRING;     offset += load_string(&buffer[offset], &message->data[i]); break;
    case 'b': message->data[i].type = TYPE_BLOB;       offset += load_blob(&buffer[offset], &message->data[i]); break;
    case 'B': message->data[i].type = TYPE_ADVBLOB;    offset += load_blob(&buffer[offset], &message->data[i]); break;
    }
  }
}


void OSC_Base::freeData(osc_message* message) {
  if (message->data) local_free(message->data);
}


/*---------------------------------------------------------------------------------------------------------------------------------*/

void OSC_Class::_tx(const char *buffer, size_t len) {
  interface_list_struct* item;
  for (item = this->_head; item; item = item->next) {
    item->interface->Send(buffer, len);
  }
}


OSC_Class::OSC_Class(void) {
}


void OSC_Class::Start(void) {
  OSC_Base::Start();
  _head = NULL;
}


void OSC_Class::Start(char *_tx_buffer, size_t _tx_buffer_len, char *_rx_buffer, size_t _rx_buffer_len) {
  OSC_Base::Start(_tx_buffer, _tx_buffer_len, _rx_buffer, _rx_buffer_len);
  _head = NULL;
}


void OSC_Class::addInterface(OSC_Base *interface) {
  interface_list_struct* item;
  interface_list_struct* newItem;

  for (item = _head; item; item = item->next) {
    if (item->interface == interface) {
      return; // already added
    }
  }

  newItem = (interface_list_struct*)malloc(sizeof(interface_list_struct));
  newItem->interface = interface;

  for (item = _head; item && item->next; item = item->next);
  if (!item) {
    newItem->next = NULL;
    newItem->prev = NULL;
    _head = newItem;
  }
  else {
    newItem->next = NULL;
    newItem->prev = item;
    item->next = newItem;
  }
}


void OSC_Class::removeInterface(OSC_Base *interface) {
  interface_list_struct* item;
  for (item = _head; item; item = item->next) {
    if (item->interface == interface) {
      item->prev->next = item->next;
      item->next->prev = item->prev;
      free(item);
      return;
    }
  }
}


void OSC_Class::Repeat(const char* path, void* args) {
  osc_on_message_args* on_message_args = (osc_on_message_args*) args;
  interface_list_struct* item;
  if (!on_message_args || !on_message_args->message) return;
  for (item = this->_head; item; item = item->next) {
    if (item->interface != on_message_args->message->origin) item->interface->Send(on_message_args->message);
  }
}


void OSC_Class::onMessage(const char *path, pathsearch_MatchHandler callback) {
  interface_list_struct* item;
  for (item = this->_head; item; item = item->next) {
    item->interface->onMessage(path, callback);
  }
}
void OSC_Class::remove_onMessage(pathsearch_MatchHandler callback) {
  interface_list_struct* item;
  for (item = this->_head; item; item = item->next) {
    item->interface->remove_onMessage(callback);
  }
}


/*---------------------------------------------------------------------------------------------------------------------------------*/

void osc_repeat(const char* path, void* args) {
  OSC.Repeat(path, args);
}
