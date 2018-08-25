/*
 * A program to allow fuzz-testing of this implementation via
 * [afl](http://lcamtuf.coredump.cx/afl/).
 *
 * It reads a file from the path given as its single cli parameter, then tries
 * to parse this file as hsdt. If it could not be parsed, the program exits
 * with code 0. If the data was valid hsdt, it then reencodes it and crashes if
 * the produced output does not equal the original input.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/hsdt.h"

int main(int argc, char *argv[])  {
  if (argc != 2) {
    abort();
  } else {
    /* https://stackoverflow.com/a/14002993 */
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    size_t input_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *input = malloc(input_size);
    fread(input, input_size, 1, f);
    fclose(f);

    HSDT_Value decoded;
    size_t consumed;

    if (hsdt_decode(input, input_size, &decoded, &consumed) == HSDT_ERR_NONE) {
      size_t encoded_len;
      uint8_t *encoded = hsdt_encode(decoded, &encoded_len);
      assert(encoded_len == consumed);
      assert(memcmp(input, encoded, consumed) == 0);

      free(input);
      free(encoded);
      hsdt_value_free(decoded);
      return 0;
    } else {
      free(input);
      return 0;
    }
  }
}
