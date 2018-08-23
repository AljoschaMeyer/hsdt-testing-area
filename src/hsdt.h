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

#ifdef HSDT_HOMOGENOUS_ARRAYS
  ,HSDT_ARRAY_HOM
#endif

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

/* Frees all heap-allocated data associated with the given value. */
void hsdt_value_free(HSDT_Value val);

/* Errors that can occur during decoding of an encoded value. */
typedef enum {
  HSDT_ERR_NONE, /* No decoding error occured */
  HSDT_ERR_EOF, /* Input ended even though parsing has not been completed */
  HSDT_UNKNOWN_TAG, /* Encountered an unassigned tag byte value */
  HSDT_ERR_UTF8, /* An utf8 string conains invalid data */
  HSDT_ERR_INVALID_NAN, /* A float is an NaN value other than 0xf97e00 */
  HSDT_ERR_DUPLICATE_KEY, /* A map contains the same key multiple times */
  HSDT_ERR_CANONIC_LENGTH, /* The length of a collection or array is not given int the canoical format */
  HSDT_ERR_CANONIC_ORDER /* The keys of a map are not sorted correctly */
} HSDT_ERR;

/* Parse an encoded value into an HSDT_Value.
 *
 * Reads at most `in_len` many bytes from `in` nd decodes them. If this returns
 * `HSDT_ERR_NONE`, `out` now contains all necessary data. Else, the content of
 * `out` is unspecified, but it is not necessary to call `hsdt_value_free` on it.
 *
 *  In any case, `consumed` is set to the number of bytes that were read.
 */
HSDT_ERR decode(unsigned char *in, size_t in_len, HSDT_Value *out, size_t *consumed);

/* Creates and returns an sds string holding the canonical encoding of the given value. */
sds encode(HSDT_Value in);

#endif
