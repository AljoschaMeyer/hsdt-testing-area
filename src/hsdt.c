#include <string.h>
#include <stdlib.h>

#include "hsdt.h"

bool hsdt_value_eq(HSDT_Value a, HSDT_Value b) {
  if (a.tag != b.tag) {
    return false;
  } else {
    switch (a.tag) {
      case HSDT_NULL:
        return true;
      case HSDT_TRUE:
        return true;
      case HSDT_FALSE:
        return true;
      case HSDT_BYTE_STRING:
        return sdscmp(a.byte_string, b.byte_string) == 0;
      case HSDT_UTF8_STRING:
        return sdscmp(a.utf8_string, b.utf8_string) == 0;
      case HSDT_FP:
        if (isnan(a.fp) && isnan(b.fp)) {
          return true;
        } else {
          return a.fp == b.fp;
        }
      case HSDT_ARRAY:
        if (a.array.len != b.array.len) {
          return false;
        } else {
          for (size_t i = 0; i < a.array.len; i++) {
            if (!hsdt_value_eq(a.array.elems[i], b.array.elems[i])) {
              return false;
            }
          }
          return true;
        }
      case HSDT_MAP:
        if (raxSize(a.map) != raxSize(b.map)) {
          return false;
        } else {
          raxIterator ia;
          raxIterator ib;
          raxStart(&ia, a.map);
          raxStart(&ib, b.map);

          while (raxNext(&ia)) {
            raxNext(&ib);

            if (ia.key_len != ib.key_len) {
              raxStop(&ia);
              raxStop(&ib);
              return false;
            } else if (memcmp(ia.key, ib.key, ia.key_len) != 0) {
              raxStop(&ia);
              raxStop(&ib);
              return false;
            } else if (!hsdt_value_eq(*(HSDT_Value *) ia.data, *(HSDT_Value *) ib.data)) {
              raxStop(&ia);
              raxStop(&ib);
              return false;
            }
          }

          raxStop(&ia);
          raxStop(&ib);
          return true;
        }
      default:
        return false; /* unreachable if tags are valid */
    }
  }
}

void hsdt_value_free(HSDT_Value val) {
  raxIterator iter;

  switch (val.tag) {
    case HSDT_NULL:
      return;
    case HSDT_TRUE:
      return;
    case HSDT_FALSE:
      return;
    case HSDT_BYTE_STRING:
      sdsfree(val.byte_string);
      return;
    case HSDT_UTF8_STRING:
      sdsfree(val.utf8_string);
      return;
    case HSDT_FP:
      return;
    case HSDT_ARRAY:
      for (size_t i = 0; i < val.array.len; i++) {
        hsdt_value_free(val.array.elems[i]);
      }
      free(val.array.elems);
      return;
    case HSDT_MAP:
      raxStart(&iter, val.map);

      while (raxNext(&iter)) {
        hsdt_value_free(*(HSDT_Value *) iter.data);
      }

      raxStop(&iter);
      raxFree(val.map);
      return;
  }
}
