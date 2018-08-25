/*
 * Checks that the implementation correctly works with some of the sample data
 * from https://tools.ietf.org/html/rfc7049#appendix-A
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/hsdt.h"

static void check(char *hex_input, HSDT_Value expected) {
  /* Convert input string (hex encoded, null-delimited) into binary data. */
  size_t hex_len = strlen(hex_input);
  size_t valid_bytes_len = hex_len / 2;
  uint8_t *valid_bytes = malloc(valid_bytes_len);
  /* https://gist.github.com/xsleonard/7341172 */
  for (size_t i=0, j=0; j< valid_bytes_len; i+=2, j++)
    valid_bytes[j] = (hex_input[i] % 32 + 9) % 25 * 16 + (hex_input[i+1] % 32 + 9) % 25;

  /* Perform the checks */

  HSDT_Value actual;
  size_t consumed;

  assert(hsdt_decode(valid_bytes, valid_bytes_len, &actual, &consumed) == HSDT_ERR_NONE);
  assert(consumed == valid_bytes_len);
  assert(hsdt_value_eq(actual, expected));

  size_t reencoded_len;
  uint8_t *reencoded = hsdt_encode(actual, &reencoded_len);
  // print_buf(reencoded, reencoded_len);
  assert(memcmp(reencoded, valid_bytes, reencoded_len) == 0);

  assert(reencoded_len == hsdt_encoding_len(actual));

  hsdt_value_free(expected);
  hsdt_value_free(actual);
  free(reencoded);
  free(valid_bytes);
}

static void reject(char *hex_input, HSDT_ERR expected_err) {
  assert(expected_err != HSDT_ERR_NONE);

  /* Convert input string (hex encoded, null-delimited) into binary data. */
  size_t hex_len = strlen(hex_input);
  size_t valid_bytes_len = hex_len / 2;
  uint8_t *valid_bytes = malloc(valid_bytes_len);
  /* https://gist.github.com/xsleonard/7341172 */
  for (size_t i=0, j=0; j< valid_bytes_len; i+=2, j++)
    valid_bytes[j] = (hex_input[i] % 32 + 9) % 25 * 16 + (hex_input[i+1] % 32 + 9) % 25;

  /* Perform the checks */
  HSDT_Value val;
  val.tag = HSDT_NULL; /* Must be initialized so hsdt_value_free does not read uninitialized data */
  size_t consumed;
  assert(hsdt_decode(valid_bytes, valid_bytes_len, &val, &consumed) == expected_err);

  hsdt_value_free(val);
  free(valid_bytes);
}

int main(void) {
  HSDT_Value expected;

  expected.tag = HSDT_FP;
  expected.fp = 1.1;
  check("fb3ff199999999999a", expected);

  expected.tag = HSDT_FP;
  expected.fp = 1.0e+300;
  check("fb7e37e43c8800759c", expected);

  expected.tag = HSDT_FP;
  expected.fp = -4.1;
  check("fbc010666666666666", expected);

  expected.tag = HSDT_FP;
  expected.fp = INFINITY;
  check("fb7ff0000000000000", expected);

  expected.tag = HSDT_FP;
  expected.fp = NAN;
  check("fb7ff8000000000000", expected);

  expected.tag = HSDT_FP;
  expected.fp = -INFINITY;
  check("fbfff0000000000000", expected);

  expected.tag = HSDT_FALSE;
  check("f4", expected);

  expected.tag = HSDT_TRUE;
  check("f5", expected);

  expected.tag = HSDT_NULL;
  check("f6", expected);

  expected.tag = HSDT_BYTE_STRING;
  expected.byte_string = sdsnew("");
  check("40", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("");
  check("60", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("a");
  check("6161", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("IETF");
  check("6449455446", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("\"\\");
  check("62225c", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("\u00fc");
  check("62c3bc", expected);

  expected.tag = HSDT_UTF8_STRING;
  expected.utf8_string = sdsnew("\u6c34");
  check("63e6b0b4", expected);

  expected.tag = HSDT_ARRAY;
  expected.array.len = 0;
  expected.array.elems = NULL;
  check("80", expected);

  expected.tag = HSDT_MAP;
  expected.map = raxNew();
  check("a0", expected);

  expected.tag = HSDT_MAP;
  expected.map = raxNew();
  HSDT_Value *inner_ = malloc(sizeof(HSDT_Value));
  inner_->tag = HSDT_UTF8_STRING;
  inner_->utf8_string = sdsnew("c");
  raxInsert(expected.map, (unsigned char*) "b", 1, (void *) inner_, NULL);
  check("a161626163", expected);

  expected.tag = HSDT_ARRAY;
  expected.array.len = 2;
  HSDT_Value *elems = malloc(2 * sizeof(HSDT_Value));
  expected.array.elems = elems;
  elems[0].tag = HSDT_UTF8_STRING;
  elems[0].utf8_string = sdsnew("a");
  elems[1].tag = HSDT_MAP;
  elems[1].map = raxNew();
  HSDT_Value *inner = malloc(sizeof(HSDT_Value));
  inner->tag = HSDT_UTF8_STRING;
  inner->utf8_string = sdsnew("c");
  raxInsert(elems[1].map, (unsigned char*) "b", 1, (void *) inner, NULL);
  check("826161a161626163", expected);

  /* Stuff that must be rejected */
  reject("81", HSDT_ERR_EOF); /* Not enough data */
  reject("9a80003f6581", HSDT_ERR_EOF); /* Not enough data */

  return 0;
}

/* TODO add tests for data that must be rejected */
