#ifndef HSDT_H
#define HSDT_H

#include <stddef.h>
#include <stdbool.h>

#include <math.h>
#ifndef __STDC_IEC_559__
Do not compile if doubles are not guaranteed to behave according to IEEE 754
#endif

/* Dynamic string library, documentation at https://github.com/antirez/sds */
#include "../deps/sds.h"
/* Radix tree map, documentation at https://github.com/antirez/rax */
#include "../deps/rax.h"

/* We begin by giving types for the logical data model of hsdt. */
typedef enum {
  HSDT_NULL,
  HSDT_TRUE,
  HSDT_FALSE,
  HSDT_BYTE_STRING,
  HSDT_UTF8_STRING,
  HSDT_FP,
  HSDT_ARRAY,
  HSDT_MAP
} HSDT_TYPE_TAG;

typedef struct HSDT_Value HSDT_Value;

typedef struct HSDT_Array {
  size_t len;
  HSDT_Value *elems;
} HSDT_Array;

typedef struct HSDT_Value {
  HSDT_TYPE_TAG tag;
  union {
    sds byte_string;
    sds utf8_string;
    double fp;
    HSDT_Array array;
    rax *map;
  };
} HSDT_Value;

/* Return whether the two given values are equal. */
bool hsdt_value_eq(HSDT_Value a, HSDT_Value b);

/* Free all heap-allocated data associated with the given value. */
void hsdt_value_free(HSDT_Value val);

/* Errors that can occur during decoding of an encoded value. */
typedef enum {
  HSDT_ERR_NONE, /* No error occured */
  HSDT_ERR_EOF, /* Input ended even though parsing has not been completed */
  HSDT_ERR_TAG, /* Encountered an invalid tag byte value */
  HSDT_ERR_UTF8, /* An utf8 string conains invalid data */
  HSDT_ERR_INVALID_NAN, /* A float is an NaN value other than 0xf97e00 */
  HSDT_ERR_UTF8_KEY, /* A map contains a key that is not a utf8 string */
  HSDT_ERR_CANONIC_ORDER, /* The keys of a map are not sorted correctly */
  HSDT_ERR_CANONIC_LENGTH /* The length of a collection or array is not given in the canonical format */
} HSDT_ERR;

/* Parse an encoded value into an HSDT_Value.
 *
 * Reads at most `in_len` many bytes from `in` and decodes them. If this returns
 * `HSDT_ERR_NONE`, `out` now contains all necessary data. Else, the content of
 * `out` is unspecified, but it is not necessary to call `hsdt_value_free` on it.
 *
 *  In any case, `consumed` is set to the number of bytes that were read.
 */
HSDT_ERR hsdt_decode(uint8_t *in, size_t in_len, HSDT_Value *out, size_t *consumed);

/*
 * Allocates and returns a string holding the canonical encoding of the given value.
 * `out_len` is set to the length of the returned string.
 */
uint8_t *hsdt_encode(HSDT_Value in, size_t *out_len);

/* Return how many bytes the value `val` would take in encoded form */
size_t hsdt_encoding_len(HSDT_Value val);
#endif
