#include "cbuf_malloc.h"
#include "stdlib.h"


size_t cbuf_dynamic_init(cbuf_malloc_type *buffer, size_t length) {
  buffer->buf = malloc(length);
  if (buffer->buf) {
    buffer->length = length;
    buffer->write_point = 0;
    return length;
  }
  return 0;
}

size_t cbuf_static_init(cbuf_malloc_type *buffer, char *buf, size_t length) {
  buffer->length = length;
  buffer->buf = buf;
  buffer->write_point = 0;
  return length;
}

void* cbuf_malloc(cbuf_malloc_type *buffer, size_t length) {
  void* return_ptr;
  if (buffer->write_point + length > buffer->length) {
    buffer->write_point = length;
    return (void*)&buffer->buf[0];
  }
  else {
    return_ptr = (void*)&buffer->buf[buffer->write_point];
    buffer->write_point += length;
    return return_ptr;
  }
}


