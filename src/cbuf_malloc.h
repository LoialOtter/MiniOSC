#ifndef __CBUF_MALLOC_H__
#define __CBUF_MALLOC_H__

#if defined (__cplusplus)
extern "C" {
#endif
#if 0
} // ya, stupid, but I get so annoyed by that extra whitespace
#endif

#include <stdlib.h>

/* Please note!
 * This memory system has no provisions for detecting overwriting old data. Things allocated using this need to be used immediately
 * and no assumptions should be made about the validity of the data after a subsequent call of 'cbuf_malloc'
 *
 * This is a very simple system that wraps if it's unable to get a block past the last point allocated.
 * So if a buffer of 256 bytes is made and you ask for 130 bytes twice in a row, both will be given the same address and the
 * last 126 bytes of the buffer will not be used. 
 *
 * It's best to make sure the buffer size is at least twice the size of your data.
 */



typedef struct {
  char *buf;
  size_t length;
  unsigned write_point : 12;
} cbuf_malloc_type;

void* cbuf_malloc(cbuf_malloc_type *buffer, size_t length);

// char test_buffer[256];
// cbuf_malloc_type test = _CBUF_MALLOC_DATA(test_buffer);
#define _CBUF_MALLOC_DATA(name) {(char *)(&name),sizeof(name),0}

size_t cbuf_dynamic_init(cbuf_malloc_type *buffer, size_t length);
size_t cbuf_static_init(cbuf_malloc_type *buffer, char *buf, size_t length);



#if 0
{ // ya, stupid... but might as well ... uh... just ignore please
#endif
#if defined (__cplusplus)
} // extern "C"
#endif


#endif /* __CBUF_MALLOC_H__ */
