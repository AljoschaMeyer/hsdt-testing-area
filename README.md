# HSDT Implementation

An implementation to experiment with different drafts of the canonicl hsdt data format, as discussed on [ssb](%y5G9E1MJ8sv4NSyQ+T8PszTTPEcf1j7vPkcHSR3AuXA=.sha256).

Running `ninja` will compile and do a few simple unit tests. It also creates a binary at `build/test/fuzz-test` that is instrumented to be run with [afl](http://lcamtuf.coredump.cx/afl/), as `afl-fuzz -i fuzzing/testcases -o fuzzing/findings build/test/fuzz-test @@`. It tests for correct round-trip behaviour of encoder and decoder.

This repo currently implements the following spec:

# HSDT Draft 3

## Logical Data Types

An mvhsdt value is one of the following:

- `null`
- a boolean (`true` or `false`)
- a utf8 encoded string (may include null bytes)
- a string of arbitrary bytes
- an IEEE 754 double precision floating point number
- an ordered sequence of values, called an array
- an unordered mapping from utf8 strings to values, called a map. An map may not contain the same key multiple times.

## Binary Encoding

Each value is preceded by a single byte, indicating its type. Conceptually, this byte is split into the *major type* and the *additional type*. The major type is stored in the first three bits, ranging from 0 to 7. The additional type is stored in the remaining five bits, ranging from 0 to 31. This scheme is stolen from [cbor](https://tools.ietf.org/html/rfc7049), which heavily inspired this spec. Choice of major and additional types tries to keep compatibility with cbor when possible. In fact, the current spec is a subset of cbor. This scheme also leaves sufficient space for adaption to a larger logical data type model, and for backwards-compatible extension should the need arise.

### Primitives

The major type `7` (`0b111`) is used for `primitive` values: `null`, `true`, `false` and floats.

- The tag `0b111_10110` (additional type 22) indicates a `null` value.
- The tag `0b111_10100` (additional type 20) indicates a `false` value.
- The tag `0b111_10101` (additional type 21) indicates a `true` value.
- The tag `0b111_11011` (additional type 27) indicates a 64 bit float. The eigth bytes following the tag represent an IEEE 743 double precision floating point number. <Imagine I precisely specified how to encode floats here. There are existing standards for that, we can pick one, move along>.

### Byte Strings

The major type `2` (`0b010`) is used for byte strings. The additional type signals how the length of the string is encoded:

- If the additional type is `n` where `n` is smaller than `24`, the `n` bytes following the tag represent the byte string.
- If the tag is `0b010_11000` (additional type 24), the byte following the tag is to be interpreted as an unsigned 8 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11001` (additional type 25), the two bytes following the tag are to be interpreted as a big-endian unsigned 16 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11010` (additional type 26), the four bytes following the tag are to be interpreted as a big-endian unsigned 32 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11011` (additional type 27), the eight bytes following the tag are to be interpreted as a big-endian unsigned 64 bit int `n`. The `n` bytes following that integer represent the byte string.

### UTF8 Strings

The major type `3` (`0b011`) is used for utf8 strings. The additional type signals how the length of the string is encoded, excatly the same way as for byte strings. A parser must check whether the decoded string is valid utf8, and indicate an error if it is not.

### Arrays

The major type `4` (`0b100`) is used for arrays. The additional type signals how the length of the array is encoded. It works exactly like the length encoding of strings. Once the length has been obtained, read encoded values until you read as many as the length specified.

### Maps

The major type `5` (`0b101`) is used for maps. Minor type and parsing work exactly like heterogenous arrays, except that the length indicates the number of key-value pairs, rather than single entries. A key-value pair is encoded by first encoding the key, directly followed by the value.

When parsing a map, an error must be emitted if a key is not a valid utf8 string, nd if the map contains duplicate keys.

### General
A parser must emit an error if it encounters a tag other than one of those listed above.

TODO this inhibits backward-compatible extensions. Give guidelines on which data to ignore instead!

## Canonical Encoding
For a canonical encoding:

- floats that indicate NaN must be encoded as `0xfb7ff8000000000000`
  - note that this makes canonical hsdt less expressive than non-canonical hsdt
- always use the smallest additional type that can contain the length of a string/array/map
- serialize maps lexicographically sorted by key (ignoring its tag). Note that this is not how cbor recommends doing it, cbor instead sorts first by length of the key, and then lexicographically.

In a setting requiring canonicity, decoders must emit errors if one of these conditions is violated, i.e. when

- reading a float that is a NaN other than `0xfb7ff8000000000000`
- a length smaller than 24 is not directly encoded in the corresponding tag's additional type
- a length smaller than 256 is not encoded with a corresponding tag's additional type of 24
- a length smaller than 65536 is not encoded with a corresponding tag's additional type of 25
- a length smaller than 4294967296 is not encoded with a corresponding tag's additional type of 26
- the keys of a map are not in ascending lexicographic order
