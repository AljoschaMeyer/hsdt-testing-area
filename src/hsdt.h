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
  HSDT_UNKNOWN_TAG, /* Encountered an unassigned tag byte value */
  HSDT_ERR_UTF8, /* An utf8 string conains invalid data */
  HSDT_ERR_INVALID_NAN, /* A float is an NaN value other than 0xf97e00 */
  HSDT_ERR_DUPLICATE_KEY, /* A map contains the same key multiple times */
  HSDT_ERR_UTF8_KEY, /* A map contains a key that is not a utf8 string */
  HSDT_ERR_CANONIC_LENGTH, /* The length of a collection or array is not given int the canonical format */
  HSDT_ERR_CANONIC_ORDER /* The keys of a map are not sorted correctly */
} HSDT_ERR;

/*
 * A stateful encoder, which emits as may bytes as possible into a given buffer,
 * and can later be given another buffer to emit more of the bytes.
 */
typedef struct HSDT_Encoder {
  bool TODO;
} HSDT_Encoder;

/* Initializes the encoder `enc` to start encoding the given `val`. */
void hsdt_encoder_init(HSDT_Encoder *enc, HSDT_Value val);

/*
 * Use an encoder `enc` to write as many bytes as possible into the given `buf`,
 * but not more than `buf_len`.
 *
 * Returns how many bytes have been written. Returning 0 signals that the value
 * has been fully encoded.
 *
 * This may only be called on an encoder on which `hsdt_encoder_init` has been
 * called before. After this has returned 0, the encoder can be reused by
 * calling `hsdt_encoder_begin` on it again.
 */
size_t hsdt_encode(HSDT_Encoder *enc, uint8_t *buf, size_t buf_len);

/*
 * A stateful decoder, which accepts partial input and can later be invoked with
 * the remaning input.
 */
typedef struct HSDT_Decoder {
  bool TODO;
} HSDT_Decoder;

/* Initializes (or resets) the decoder `dec`. Must be called once before any decoding. */
void hsdt_decoder_reset(HSDT_Decoder *dec);

/*
 * Use a decoder `dec` to read data from `buf` (at most `buf_len` many bytes),
 * and initialize `val` according to the read data.
 *
 * Returns an error code, or `HSDT_ERR_NONE` if no error occured. Sets `consumed`
 * to the number of bytes that have been read.
 *
 * A single call to this might not decode the full value, the input data might
 * have been incomplete. In these cases, `HSDT_ERR_NONE` is returned and
 * `consumed` is set to a nonzero value (unless `buf_len` has been zero).
 * Finished decoding is signalled by returning `HSDT_ERR_NONE` and setting
 * `consumed` to zero.
 */
HSDT_ERR hsdt_decode(HSDT_Decoder *dec, HSDT_Value *val, size_t *consumed, const uint8_t *buf, size_t buf_len);

#endif
