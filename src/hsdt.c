#include "hsdt.h"

bool hsdt_value_eq(HSDT_Value a, HSDT_Value b) {
  if (a.tag != b.tag) {
    return false;
  } else {
    return false; // TODO
  }
}

void hsdt_value_free(HSDT_Value val) {
  switch (val.tag) {
    // TODO
    default:
      return;
  }
}
