#ifndef __PATH_SEARCH_H__
#define __PATH_SEARCH_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined (__cplusplus)
extern "C" {
#endif
#if 0
} // ya, stupid, but I get so annoyed by that extra whitespace
#endif


  
/* This is a library to give a fast search algorithm to handle large sets of possible commands or "paths"
 * 
 * this makes use of a huffman-like tree search. It supports a sub-set of wildcards.
 *
 * As new strings are added to the list, the engine will find the location for choices within the tree allowing the
 * final search to only try paths that are valid
 *
 * This was initially developed for OSC path searching but should be useful for any CLI-like interface.
 * Optionally non-choice sections can be skipped similar to how Cisco IOS CLI works
 */

#ifndef PATHSEARCH_SKIP_NO_CHOICE
  #define PATHSEARCH_SKIP_NO_CHOICE true
#endif

#ifndef PATHSEARCH_MAX_MATCHES
  #define PATHSEARCH_MAX_MATCHES 32
#endif

#ifndef PATHSEARCH_MAX_DEPTH
  #define PATHSEARCH_MAX_DEPTH 8
#endif


struct pathsearch_ItemDescriptor_struct;
struct pathsearch_Choice_struct;

typedef void (*pathsearch_MatchHandler)(const char* path, void* args);

// by design, this can be a link to different types of structs by making use of the type field
typedef struct pathsearch_ItemDescriptor_struct {
  unsigned type : 8;
  unsigned allocated : 1;
  unsigned disabled : 1;
  unsigned heap : 1; // marker if this was malloc'd
  unsigned reserved : 22;
  const char *path;
  pathsearch_MatchHandler callback;
  struct pathsearch_ItemDescriptor_struct *next;
  struct pathsearch_ItemDescriptor_struct *prev;
} pathsearch_ItemDescriptor;




typedef struct {
  unsigned initialized : 1;
  pathsearch_ItemDescriptor* head;
  pathsearch_ItemDescriptor* tail;
} pathsearch_Object;


void pathsearch_init(pathsearch_Object* ps);
void pathsearch_destroy(pathsearch_Object* ps);

pathsearch_ItemDescriptor* addPath(pathsearch_Object* ps, const char *fullpath, pathsearch_MatchHandler callback);
// a more powerful version which allows arbitrary structs as long as the first items match the type
pathsearch_ItemDescriptor* addPath_descriptor(pathsearch_Object* ps, pathsearch_ItemDescriptor* descriptor);

void removePath(pathsearch_Object* ps, pathsearch_ItemDescriptor* descriptor);
void removePath_byCallback(pathsearch_Object* ps, pathsearch_MatchHandler callback);
void removePath_byPath(pathsearch_Object* ps, const char *path);
int pathSearch(pathsearch_Object* ps, const char *path, void* args);



#if 0
{ // ya, stupid... but might as well ... uh... just ignore please
#endif
#if defined (__cplusplus)
} // extern "C"
#endif

#endif /* __PATH_SEARCH_H__ */
