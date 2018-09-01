#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "hsdt.h"

/* https://stackoverflow.com/a/28592202 */
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

// TODO make everything iterative rather than recursive
// TODO handle OOM
// TODO introduce COLLECTION_SIZE_IN_BYTES

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
            if (!hsdt_value_eq(a.array.elems[i], b.array.elems[i])) { // XXX recursion
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
          raxSeek(&ia, "^", (unsigned char*) "", 0); // XXX OOM
          raxSeek(&ib, "^", (unsigned char*) "", 0); // XXX OOM

          while (raxNext(&ia)) { // XXX OOM
            raxNext(&ib); // XXX OOM

            if (ia.key_len != ib.key_len) {
              raxStop(&ia);
              raxStop(&ib);
              return false;
            } else if (memcmp(ia.key, ib.key, ia.key_len) != 0) {
              raxStop(&ia);
              raxStop(&ib);
              return false;
            } else if (!hsdt_value_eq(*(HSDT_Value *) ia.data, *(HSDT_Value *) ib.data)) { // XXX recursion
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
        hsdt_value_free(val.array.elems[i]); // XXX recursion
      }
      free(val.array.elems);
      return;
    case HSDT_MAP:
      raxStart(&iter, val.map);
      raxSeek(&iter, "^", (unsigned char*) "", 0); // XXX OOM

      while (raxNext(&iter)) { // XXX OOM
        hsdt_value_free(*(HSDT_Value *) iter.data); // XXX recursion
        free(iter.data);
      }

      raxStop(&iter);
      raxFree(val.map);
      return;
  }
}

#ifdef COLLECTION_SIZE_IN_BYTES
// TODO memoize results?
size_t hsdt_decode_len(uint8_t *in, size_t in_len) {
  if (in_len == 0) {
    return SIZE_MAX;
  } else if (in[0] == 0xF4 || in[0] == 0xF5 || in[0] == 0xF6) { /* false, true, or null */
    return 1;
  } else if (in[0] == 0xFB) { /* 64 bit float */
    return 9;
  } else {
    uint8_t major = in[0] >> 5;
    if (major < 2 || major > 5) { /* invalid major type */
      return 0;
    }

    uint8_t additional = in[0] & 0x1F;
    size_t len;

    switch (additional) {
      case 24:
        if (in_len < 2) {
          return SIZE_MAX;
        } else {
          len = in[1];
          if (len <= 23) { /* Length is not canonic */
            return 0;
          } else {
            return 2 + len;
          }
        }
      case 25:
        if (in_len < 3) {
          return SIZE_MAX;
        } else {
          uint16_t tmp16;
          memcpy(&tmp16, in + 1, 2);
          len = ntohs(tmp16);
          if (len <= 255) { /* Length is not canonic */
            return 0;
          } else {
            return 3 + len;
          }
        }
      case 26:
        if (in_len < 5) {
          return SIZE_MAX;
        } else {
          uint32_t tmp32;
          memcpy(&tmp32, in + 1, 4);
          len = ntohl(tmp32);
          if (len <= 65535) { /* Length is not canonic */
            return 0;
          } else {
            return 5 + len;
          }
        }
      case 27:
        if (in_len < 9) {
          return SIZE_MAX;
        } else {
          uint64_t tmp64;
          memcpy(&tmp64, in + 1, 8);
          len = ntohll(tmp64);
          if (len <= 4294967295) { /* Length is not canonic */
            return 0;
          } else {
            return 9 + len;
          }
        }
      case 28:
      case 29:
      case 30:
      case 31:
        /* Invalid additional type */
        return 0;
      default:
        /* 1 byte for the tag, plus the length which equals the additional type */
        return 1 + additional;
    }
  }
}
#endif

typedef union DoubleAsInt {
  double d;
  uint64_t i;
} DoubleAsInt;

/* Begin utf8 checking code, taken from https://stackoverflow.com/a/22135005 */
#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

/* Call with UTF8_ACCEPT as the initial state. */
static uint32_t validate_utf8(uint32_t *state, uint8_t *str, size_t len) {
   size_t i;
   uint32_t type;

    for (i = 0; i < len; i++) {
        // We don't care about the codepoint, so this is
        // a simplified version of the decode function.
        type = utf8d[str[i]];
        *state = utf8d[256 + (*state) * 16 + type];

        if (*state == UTF8_REJECT)
            break;
    }

    return *state;
}
/* End utf8 checking code */

/* Return how many bytes are needed to encode the length of a collection of the given size. */
/* Works independent of whether `size` is in bytes or in entries. */
static size_t len_enc(size_t size) {
  if (size <= 23) {
    return 0;
  } else if (size <= 255) {
    return 1;
  } else if (size <= 65535) {
    return 2;
  } else if (size <= 4294967295) {
    return 4;
  } else {
    return 8;
  }
}

#ifndef COLLECTION_SIZE_IN_BYTES
/*
 * With COLLECTION_SIZE_IN_BYTES, this would need to do memoization of entry
 * sizes to keep linear running time, else it would be quadratic in the nesting
 * depth.
 */
size_t hsdt_encoding_len(HSDT_Value val) {
  size_t size; /* The size of a collection, counted in contained items. */
  size_t inner_size; /* Summend size in bytes of the encodings of all contained items. */

  raxIterator iter;

  switch (val.tag) {
    case HSDT_NULL:
      return 1;
    case HSDT_TRUE:
      return 1;
    case HSDT_FALSE:
      return 1;
    case HSDT_BYTE_STRING:
      size = sdslen(val.byte_string);
      inner_size = size;
      return 1 + len_enc(size) + inner_size;
    case HSDT_UTF8_STRING:
      size = sdslen(val.utf8_string);
      inner_size = size;
      return 1 + len_enc(size) + inner_size;
    case HSDT_FP:
      return 1 + 8;
    case HSDT_ARRAY:
      size = val.array.len;

      inner_size = 0;
      for (size_t i = 0; i < val.array.len; i++) {
        inner_size += hsdt_encoding_len(val.array.elems[i]); // XXX recursion
      }

      return 1 + len_enc(size) + inner_size;
    case HSDT_MAP:
      size = raxSize(val.map);

      inner_size = 0;
      raxStart(&iter, val.map);
      raxSeek(&iter, "^", (unsigned char*) "", 0); // XXX OOM

      while (raxNext(&iter)) { // OOM
        inner_size += len_enc(iter.key_len) + 1;
        inner_size += iter.key_len;
        inner_size += hsdt_encoding_len(*(HSDT_Value *) iter.data); // XXX recursion
      }

      raxStop(&iter);
      return 1 + len_enc(size) + inner_size;
    default:
      return 0; /* unreachable if tags are valid */
  }
}
#endif

/* Encode `in` into the buffer `buf`, which must be large enough. Return how many bytes were written. */
static size_t do_encode(HSDT_Value in, uint8_t *buf);

uint8_t *hsdt_encode(HSDT_Value in, size_t *out_len) {
  size_t enc_len = hsdt_encoding_len(in);
  *out_len = enc_len;
  uint8_t *enc = malloc(enc_len); // XXX OOM

  do_encode(in, enc);
  return enc;
}

static size_t encode_len(size_t size, uint8_t major, uint8_t *buf) {
  if (size <= 23) {
    buf[0] = major | size;
    return 1;
  } else if (size <= 255) {
    buf[0] = major | 24;
    buf[1] = (uint8_t) size;
    return 2;
  } else if (size <= 65535) {
    uint16_t size_16 = htons((uint16_t) size);
    buf[0] = major | 25;
    buf[1] = size_16 >>  0 & 0xff;
    buf[2] = size_16 >>  8 & 0xff;
    return 3;
  } else if (size <= 4294967295) {
    uint32_t size_32 = htonl((uint32_t) size);
    buf[0] = major | 26;
    buf[1] = size_32 >>  0 & 0xff;
    buf[2] = size_32 >>  8 & 0xff;
    buf[3] = size_32 >> 16 & 0xff;
    buf[4] = size_32 >> 24 & 0xff;
    return 5;
  } else {
    uint64_t size_64 = htonll((uint64_t) size);
    buf[0] = major | 27;
    buf[1] = size_64 >>  0 & 0xff;
    buf[2] = size_64 >>  8 & 0xff;
    buf[3] = size_64 >> 16 & 0xff;
    buf[4] = size_64 >> 24 & 0xff;
    buf[5] = size_64 >> 32 & 0xff;
    buf[6] = size_64 >> 40 & 0xff;
    buf[7] = size_64 >> 48 & 0xff;
    buf[8] = size_64 >> 56 & 0xff;
    return 9;
  }
}

static size_t do_encode(HSDT_Value val, uint8_t *buf) {
  size_t col_len;
  size_t len_len;
  size_t total_written;
  raxIterator iter;
  switch (val.tag) {
    case HSDT_NULL:
      buf[0] = 0xF6;
      return 1;
    case HSDT_TRUE:
      buf[0] = 0xF5;
      return 1;
    case HSDT_FALSE:
      buf[0] = 0xF4;
      return 1;
    case HSDT_BYTE_STRING:
      col_len = sdslen(val.byte_string);
      len_len = encode_len(col_len, 0x40, buf);
      memcpy(buf + len_len, val.byte_string, col_len);
      return len_len + col_len;
    case HSDT_UTF8_STRING:
      col_len = sdslen(val.utf8_string);
      len_len = encode_len(col_len, 0x60, buf);
      memcpy(buf + len_len, val.utf8_string, col_len);
      return len_len + col_len;
    case HSDT_FP:
      buf[0] = 0xfb;
      if (isnan(val.fp)) {
        buf[1] = 0x7f;
        buf[2] = 0xf8;
        buf[3] = 0x00;
        buf[4] = 0x00;
        buf[5] = 0x00;
        buf[6] = 0x00;
        buf[7] = 0x00;
        buf[8] = 0x00;
        return 9;
      } else {
        DoubleAsInt convert;
        convert.d = val.fp;
        convert.i = htonll(convert.i);

        for (size_t i = 0; i < 8; i++) {
          buf[1 + i] = ((uint8_t *)&convert.i)[i];
        }
        return 9;
      }
    case HSDT_ARRAY:
      col_len = val.array.len;
      len_len = encode_len(col_len, 0x80, buf);
      total_written = len_len;

      for (size_t i = 0; i < col_len; i++) {
        total_written += do_encode(val.array.elems[i], buf + total_written); // XXX recursion
      }

      return total_written;
    case HSDT_MAP:
      col_len = raxSize(val.map);
      len_len = encode_len(col_len, 0xA0, buf);
      size_t total_written = len_len;

      raxStart(&iter, val.map);
      raxSeek(&iter, "^", (unsigned char*) "", 0); // XXX OOM
      while (raxNext(&iter)) { // XXX OOM
        /* handle key */
        total_written += encode_len(iter.key_len, 0x60, buf + total_written);
        memcpy(buf + total_written, iter.key, iter.key_len);
        total_written += iter.key_len;
        /* handle value */
        total_written += do_encode(*(HSDT_Value *) iter.data, buf + total_written); // XXX recursion
      }

      raxStop(&iter);
      return total_written;
    default:
      return 0; /* unreachable if tags are valid */
  }
}

static HSDT_ERR do_decode(uint8_t *in, size_t in_len, HSDT_Value *out, size_t *consumed);

HSDT_ERR hsdt_decode(uint8_t *in, size_t in_len, HSDT_Value *out, size_t *consumed) {
  *consumed = 0;
  return do_decode(in, in_len, out, consumed);
}

/*
 * Helper function. Reads a tag and all following length data from `in`, errors
 * if not enough data is available. Increases `consumed` by the amount of bytes
 * it read. Sets `major`, `additional` to the tag values, and `val` to the length
 * data.
 */
static HSDT_ERR tag_and_val(uint8_t *in, size_t in_len, size_t *consumed, uint8_t *major, uint8_t *additional, uint64_t *val) {
  *major = in[0] >> 5;
  *additional = in[0] & 0x1F;

  switch (*additional) {
    case 24:
      if (in_len < 2) {
        return HSDT_ERR_EOF;
      } else {
        *val = in[1];
        *consumed += 2;
        if (*val <= 23) {
          return HSDT_ERR_CANONIC_LENGTH;
        } else {
          return HSDT_ERR_NONE;
        }
      }
    case 25:
      if (in_len < 3) {
        return HSDT_ERR_EOF;
      } else {
        uint16_t tmp16;
        memcpy(&tmp16, in + 1, 2);
        *val = ntohs(tmp16);
        *consumed += 3;
        if (*val <= 255) {
          return HSDT_ERR_CANONIC_LENGTH;
        } else {
          return HSDT_ERR_NONE;
        }
      }
    case 26:
      if (in_len < 5) {
        return HSDT_ERR_EOF;
      } else {
        uint32_t tmp32;
        memcpy(&tmp32, in + 1, 4);
        *val = ntohl(tmp32);
        *consumed += 5;
        if (*val <= 65535) {
          return HSDT_ERR_CANONIC_LENGTH;
        } else {
          return HSDT_ERR_NONE;
        }
      }
    case 27:
      if (in_len < 9) {
        return HSDT_ERR_EOF;
      } else {
        uint64_t tmp64;
        memcpy(&tmp64, in + 1, 8);
        *val = ntohll(tmp64);
        *consumed += 9;
        if (*val <= 4294967295) {
          return HSDT_ERR_CANONIC_LENGTH;
        } else {
          return HSDT_ERR_NONE;
        }
      }
    case 28:
    case 29:
    case 30:
    case 31:
      return HSDT_ERR_TAG;
    default:
      *val = *additional;
      *consumed += 1;
      return HSDT_ERR_NONE;
  }
}

/* Return `true` iff if the first string is lexicogrpahically strictly greater than the second string */
static bool is_lexicographically_greater(uint8_t *s1, size_t len1, uint8_t *s2, size_t len2) {
  if (len1 > 0 && len2 > 0) {
    if (s1[0] < s2[0]) {
      return false;
    } else if (s1[0] > s2[0]) {
      return true;
    } else {
      return is_lexicographically_greater(s1 + 1, len1 - 1, s2 + 1, len2 - 1); // XXX recursion
    }
  } else {
    return len1 > 0;
  }
}

static HSDT_ERR do_decode(uint8_t *in, size_t in_len, HSDT_Value *out, size_t *consumed) {
  if (in_len == 0) {
    return HSDT_ERR_EOF;
  } else {
    if (in[0] == 0xF6) {
      out->tag = HSDT_NULL;
      *consumed += 1;
      return HSDT_ERR_NONE;
    } else if (in[0] == 0xF5) {
      out->tag = HSDT_TRUE;
      *consumed += 1;
      return HSDT_ERR_NONE;
    } else if (in[0] == 0xF4) {
      out->tag = HSDT_FALSE;
      *consumed += 1;
      return HSDT_ERR_NONE;
    } else  if (in[0] == 0xFB) { /* 64 bit float */
      if (in_len < 9) {
        return HSDT_ERR_EOF;
      } else {
        DoubleAsInt convert;
        for (size_t i = 0; i < 8; i++) {
          ((uint8_t *)&convert.i)[i] = in[1 + i];
        }
        *consumed += 9;
        convert.i = ntohll(convert.i);

        if (isnan(convert.d) && (convert.i != 0x7ff8000000000000)) {
          return HSDT_ERR_INVALID_NAN;
        } else {
          out->tag = HSDT_FP;
          out->fp = convert.d;
        }

        return HSDT_ERR_NONE;
      }
    } else {
      uint8_t major;
      uint8_t additional;
      uint64_t val;
      HSDT_ERR err;
      size_t tag_and_val_consumed = 0;
      err = tag_and_val(in, in_len, &tag_and_val_consumed, &major, &additional, &val);
      *consumed += tag_and_val_consumed;
      if (err != HSDT_ERR_NONE) {
        return err;
      }

      uint32_t utf8_state;
      uint8_t *last_key = NULL;
      size_t last_key_len = 0;
      switch (major) {
        case 2:
          if ((SIZE_MAX - in_len < *consumed) || (in_len - *consumed < val)) {
            return HSDT_ERR_EOF;
          } else {
            *consumed += val;
            out->tag = HSDT_BYTE_STRING;
            out->byte_string = sdsnewlen(in + tag_and_val_consumed, val); // XXX OOM
            return HSDT_ERR_NONE;
          }
        case 3:
          if ((SIZE_MAX - in_len < *consumed) || (in_len - *consumed < val)) {
            return HSDT_ERR_EOF;
          } else {
            *consumed += val;
            out->tag = HSDT_UTF8_STRING;
            out->utf8_string = sdsnewlen(in + tag_and_val_consumed, val); // XXX OOM

            utf8_state = UTF8_ACCEPT;
            if (validate_utf8(&utf8_state, in + tag_and_val_consumed, val) == UTF8_ACCEPT) {
              return HSDT_ERR_NONE;
            } else {
              return HSDT_ERR_UTF8;
            }
          }
        case 4:
          if ((SIZE_MAX - in_len < *consumed) || (in_len - *consumed < val)) {
            /*
             * This is not a precise check to ensure that there is enough data.
             * But it protects against malicious payloads causing large memory
             * allocations.
             */
            return HSDT_ERR_EOF;
          }

          out->tag = HSDT_ARRAY;
          out->array.len = val;
          out->array.elems = malloc(val * sizeof(HSDT_Value)); // XXX oom

          for (size_t i = 0; i < val; i++) {
            size_t inner_consumed = 0;
            HSDT_ERR e = do_decode(in + *consumed, in_len - *consumed, out->array.elems + i, &inner_consumed); // XXX recursion
            *consumed += inner_consumed;
            if (e != HSDT_ERR_NONE) {
              out->array.len = i;
              return e;
            }
          }

          return HSDT_ERR_NONE;
        case 5:
          out->map = raxNew(); // XXX OOM
          out->tag = HSDT_MAP;

          for (size_t i = 0; i < val; i++) {
            /* handle the key */
            uint8_t key_major;
            uint8_t key_additional;
            uint64_t key_val;
            size_t inner_consumed = 0;

            if (SIZE_MAX - in_len < *consumed) {
              return HSDT_ERR_EOF;
            }

            HSDT_ERR err;
            err = tag_and_val(in + *consumed, in_len - *consumed, &inner_consumed, &key_major, &key_additional, &key_val);
            if (err != HSDT_ERR_NONE) {
              return err;
            }
            *consumed += inner_consumed;

            if (key_major != 3) {
              return HSDT_ERR_UTF8_KEY;
            }
            if ((SIZE_MAX - in_len < *consumed) || (in_len - *consumed < key_val)) {
              return HSDT_ERR_EOF;
            }
            utf8_state = UTF8_ACCEPT;
            if (validate_utf8(&utf8_state, in + *consumed, key_val) != UTF8_ACCEPT) {
              return HSDT_ERR_UTF8;
            }
            if (!is_lexicographically_greater(in + *consumed, key_val, last_key, last_key_len)) {
              return HSDT_ERR_CANONIC_ORDER;
            }

            HSDT_Value *map_val = malloc(sizeof(HSDT_Value)); // XXX OOM
            size_t key_consumed = *consumed;
            last_key = in + *consumed;
            last_key_len = key_val;
            *consumed += key_val;

            /* handle the value */
            inner_consumed = 0;
            HSDT_ERR e = do_decode(in + *consumed, in_len - *consumed, map_val, &inner_consumed); // XXX recursion
            *consumed += inner_consumed;
            if (e != HSDT_ERR_NONE) {
              return e;
            }

            raxInsert(out->map, in + key_consumed, key_val, (void *) map_val, NULL); // XXX OOM
          }

          return HSDT_ERR_NONE;
        default:
          return HSDT_ERR_TAG;
      }
    }
  }
}

// HSDT_ERR hsdt_decode(uint8_t *in, size_t in_len, HSDT_Value *out, size_t *read) {
//   size_t consumed = 0; /* How many bytes have we read already? */
//   HSDT_Value *current = out; /* Where to put the decoded data */
//
//   while (consumed < in_len) {
//     /* Start decoding the next item at `in[consumed]` into `current` */
//     switch (in[consumed]) {
//       case 0xF6: /* null value */
//         current->tag = HSDT_NULL;
//         consumed += 1;
//         goto done;
//       case 0xF5: /* true value */
//         current->tag = HSDT_TRUE;
//         consumed += 1;
//         goto done;
//       case 0xF4: /* false value */
//         current->tag = HSDT_FALSE;
//         consumed += 1;
//         goto done;
//     }
//   }
//
//   /* Ran out of input before finishing to decode. */
//   return HSDT_ERR_EOF;
//
//   /*
//    * Succeeded in decoding, `consumed` holds the total number of decoded bytes,
//    * `out` has been fully initialized.
//    */
//   done:
//     *read = consumed;
//     return HSDT_ERR_NONE;
// }
//
// /* State for a stateful decryptor that can work with partial input. */
// typedef struct HSDT_Dec_State {
//
// } HSDT_Dec_State;
