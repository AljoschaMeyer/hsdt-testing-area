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
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *string = malloc(fsize);
    fread(string, fsize, 1, f);
    fclose(f);

    HSDT_Decoder dec;
    HSDT_Value decoded;
    size_t consumed;
    size_t total_consumed = 0;

    hsdt_decoder_reset(&dec);

    while (hsdt_decode(&dec, &decoded, &consumed, string + total_consumed, fsize - total_consumed) == HSDT_ERR_NONE) {
      total_consumed += consumed;

      if (total_consumed >= fsize) {
        return 0; /* Input is a prefix of a valid hsdt value */
      }

      if (consumed == 0) { /* done decoding */
        HSDT_Encoder enc;
        uint8_t *reencoded = malloc(total_consumed);
        size_t total_encoded = 0;

        hsdt_encoder_init(&enc, decoded);

        while (true) {
          assert(total_consumed - total_encoded > 0);
          size_t encoded = hsdt_encode(&enc, reencoded + total_encoded, total_consumed - total_encoded);
          total_encoded += encoded;

          if (encoded == 0) { /* done encoding */
            assert(memcpy(string, reencoded, total_consumed) == 0);
          }
        }

      }
    }

    return 0; /* Input file did not contain valid hsdt */

    // TODO parse, do stuff
  }
}
