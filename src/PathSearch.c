#include "PathSearch.h"
#include <stdlib.h>
#include <string.h>




void pathsearch_init(pathsearch_Object* ps) {
  if (!ps->initialized) {
    ps->initialized = true;
    ps->head = 0;
    ps->tail = 0;
  }
}

pathsearch_ItemDescriptor* addPath(pathsearch_Object* ps, const char *path, pathsearch_MatchHandler callback) {
  int i;
  pathsearch_ItemDescriptor* descriptor;

  descriptor = malloc(sizeof(pathsearch_ItemDescriptor));
  if (!descriptor) {
    return NULL;
  }

  descriptor->allocated = true;
  descriptor->heap = true;
  descriptor->path = path;
  descriptor->callback = callback;

  return addPath_descriptor(ps, descriptor);
}


pathsearch_ItemDescriptor* addPath_descriptor(pathsearch_Object* ps, pathsearch_ItemDescriptor* descriptor) {
  pathsearch_ItemDescriptor* old_tail = ps->tail;
  if (old_tail) {
    old_tail->next = descriptor;
    descriptor->prev = old_tail;
    descriptor->next = 0;
    ps->tail = descriptor;
  }
  else {
    if (!ps->head) {
      ps->head = descriptor;
      ps->tail = descriptor;
      descriptor->prev = 0;
      descriptor->next = 0;
    }
    else {
      for (old_tail = ps->head; old_tail->next; old_tail = old_tail->next);
      old_tail->next = descriptor;
      descriptor->prev = old_tail;
      descriptor->next = 0;
      ps->tail = descriptor;
    }
  }
  return descriptor;
}

void removePath(pathsearch_Object* ps, pathsearch_ItemDescriptor* descriptor) {
  pathsearch_ItemDescriptor* item;
  for (item = ps->head; item && item != descriptor; item = item->next); // this is not technically necessary - but it's a good sanity check
  if (item) {
    if (item->next) {
      item->next->prev = item->prev;
      item->prev->next = item->next;
    }
    else {
      item->prev->next = 0;
      ps->tail = item->prev;
    }
    item->allocated = false;

    if (item->heap) {
      free(item);
    }
  }
}

void removePath_byCallback(pathsearch_Object* ps, pathsearch_MatchHandler callback) {
  pathsearch_ItemDescriptor* item;
  for (item = ps->head; item && item->callback != callback; item = item->next);
  if (item) removePath(ps, item);
}

void removePath_byPath(pathsearch_Object* ps, const char *path) {
  pathsearch_ItemDescriptor* item;
  for (item = ps->head; item && strcmp(item->path, path) != 0; item = item->next);
  if (item) removePath(ps, item);
}







typedef enum { NO_MATCH = 0, MATCH, MATCH_COMPLETE, CONTINUE } match_return;

match_return match_asterisk(const char **a, const char **b) {
  if (**a != '*') return NO_MATCH; // error - must have an asterisk
  (*a)++;
  if (**a == '*') return MATCH_COMPLETE;
  if (**a == '/') {
    for (; **b && **b != '/'; (*b)++);
    return MATCH;
  }
  for (; **b && **b != '/' && **b != **a; (*b)++); // ERROR - not hungry
  return CONTINUE;
}


match_return match_question_mark(const char **a, const char **b) {
  if (**a != '?') return NO_MATCH;
  if (**b == '/') return NO_MATCH;
  (*a)++;
  (*b)++;
  return CONTINUE;
}



match_return match_char_set(const char **a, const char **b) {
  int invert = false;
  if (**a != '[') return NO_MATCH; // error - must have a square bracket
  (*a)++;
  if (**a == '!') { invert = true; (*a)++; }
  for (; **a && **a != ']'; (*a)++) {
    char seriesChar = **a;
    if (**a == '-') {
      seriesChar = *((*a)-1);
      for (seriesChar = *((*a) - 1); seriesChar <= *((*a) + 1) && seriesChar != **b; seriesChar++);
      //if (seriesChar == **b) {
      //  Serial.printf("%c-%c", *((*a) - 1), *((*a) + 1));
      //}
    }
    if (invert) {
      if (seriesChar == **b) {
        return NO_MATCH;
      }
    }
    else {
      if (seriesChar == **b) {
        //Serial.printf("%c", seriesChar);
        break;
      }
    }
  }
  if (!invert && **a == ']') {
    return NO_MATCH;
  }
  for (; **a && **a != ']'; (*a)++);
  //Serial.print("]");
  (*a)++;
  (*b)++;
  return CONTINUE;
}


match_return match_selection(const char **a, const char **b) {
  const char* start = (*b);

  if (**a != '{') return NO_MATCH;
  (*a)++;

  while (**a && **b) {
    //Serial.printf("%c", **a);
    if (**a == ',' || **a == '}') { // you got here, must be right
      break;
    }
    if (**b == **a) {
      (*b)++; (*a)++;
      continue;
    }
    // does not match, move to the next possible
    for (; **a && **a != ',' && **a != '}'; (*a)++);
    if (**a == '}' || !**a || !**b) {
      return NO_MATCH;
    }
    //Serial.print(" nope ");
    (*b) = start;
    (*a)++; // to go past comma
  }
  for (; **a && **a != '}'; (*a)++);
  if (!**a) {
    //Serial.print(" } missing\n\r"); 
    return NO_MATCH;
  }
  (*a)++;
  return CONTINUE;
}


match_return _segmentMatch(const char *path, const char *segment) {
  while (*path && *segment) {
    //Serial.printf("%c", *segment);

    // multi-character wildcard
    if (*segment == '*') {
      switch(match_asterisk(&segment, &path)) {
      case NO_MATCH: // error - no asterisk after sending to match part
        return NO_MATCH;
      case MATCH_COMPLETE:
        //Serial.print(" MATCH\n\r");
        return MATCH_COMPLETE;
      default:
        continue;
      }
    }
    if (*path == '*') {
      switch(match_asterisk(&path, &segment)) {
      case NO_MATCH: // error - no asterisk after sending to match part
        return NO_MATCH;
      case MATCH_COMPLETE:
        //Serial.print(" MATCH\n\r");
        return MATCH_COMPLETE;
      default:
        continue;
      }
    }
    

    // group of possible characters
    if (*segment == '[') {
      if (match_char_set(&segment, &path) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }
    if (*path == '[') {
      if (match_char_set(&path, &segment) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }
    

    // set of possible values comma delimited
    if (*segment == '{') {
      if (match_selection(&segment, &path) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }
    if (*path == '{') {
      if (match_selection(&path, &segment) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }

    // single character wildcard
    if (*segment == '?') {
      if (match_question_mark(&segment, &path) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }
    if (*path == '?') {
      if (match_question_mark(&path, &segment) == NO_MATCH) {
        //Serial.print(" NOMATCH\n\r");
        return NO_MATCH;
      }
      continue;
    }
    
    // character matches or single characer wildcard
    if (*segment == *path) {
      segment++; path++;
      continue;
    }

    //Serial.print(" NOMATCH\n\r"); 
    return NO_MATCH;
  }
  if (*segment == '*') for (; *segment == '*'; segment++); // special case - '*' can match zero or more characters
  if (*path == *segment) {
    //Serial.print(" MATCH\n\r");
    return MATCH;
  }

  //Serial.printf(" 0x%02X=0x%02X ", *segment, *path);
  //Serial.print(" NOMATCH\n\r"); 
  return NO_MATCH;
}



int pathSearch(pathsearch_Object* ps, const char *path, void* args) {
  pathsearch_ItemDescriptor *item;
  int matchedCount = 0;

  for (item = ps->head; item; item = item->next) {
    if (item && !item->disabled && _segmentMatch(path, item->path)) {
      matchedCount++;
      if (item->callback) item->callback(path, args);
    }
  }
  return matchedCount;
}


