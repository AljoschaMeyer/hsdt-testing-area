# HSDT Implementation

An implementation to experiment with and benchmark different drafts of the hsdt data format, as discussed on [ssb](%y5G9E1MJ8sv4NSyQ+T8PszTTPEcf1j7vPkcHSR3AuXA=.sha256).

Defining `HSDT_SIZE_IN_BYTES` turns on specifying collection sizes in the absolute size in bytes, rather than the number of items. This is a deviation from CBOR, but might be more efficient, and will thus need to be benchmarked.

This repo currently implements the following spec:

## Logical Data Types

An mvhsdt value is one of the following:

- `null` (the single value of the unit type, representing absence of information)
- a boolean (`true` or `false`)
- a utf8 encoded string (may include null bytes)
- a string of arbitrary bytes
- an IEEE 754 double precision floating point number, excluding all but one representation of `NaN`
- an ordered sequence of values, called an array, not necessarily homogenous
- an unordered mapping from strings to values, called an object. An object may not contain the same key multiple times. Values are not necessarily homogenous.

## Binary Encoding

Each value is preceded by a single byte, indicating its type. Conceptually, this byte is split into the *major type* and the *additional type*. The major type is stored in the first three bits, ranging from 0 to 7. The additional type is stored in the remaining five bits, ranging from 0 to 31. This scheme is stolen from [cbor](https://tools.ietf.org/html/rfc7049), which heavily inspired this spec. choice of major and additional types tries to keep compatibility with cbor when possible. This scheme also leaves sufficient space for adaption to a larger logical data type model, and for backwards-compatible extension should the need arise.

### Primitives

The major type `7` (`0b111`) is used for `primitive` values: `null`, `true`, `false` and floats.

- The tag `0b111_10110` (additional type 22) indicates a `null` value.
- The tag `0b111_10100` (additional type 20) indicates a `false` value.
- The tag `0b111_10101` (additional type 21) indicates a `true` value.
- The tag `0b111_11011` (additional type 27) indicates a 64 bit float. The eigth bytes following the tag represent an IEEE 743 double precision floating point number. <Imagine I precisely specified how to encode floats here. There are existing standards for that, we can pick one, move along>. A special case are floats that indicate NaN, these must be encoded as `0xf97e00`, and reading any NaN other than `0xf97e00` must be signaled as an error.

### Byte Strings

The major type `2` (`0b010`) is used for byte strings. The additional type signals how the length of the string is encoded:

- If the additional type is `n` where `n` is smaller than `24`, the `n` bytes following the tag represent the byte string.
- If the tag is `0b010_11000` (additional type 24), the byte following the tag is to be interpreted as an unsigned 8 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11001` (additional type 25), the two bytes following the tag are to be interpreted as a big-endian unsigned 16 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11010` (additional type 26), the four bytes following the tag are to be interpreted as a big-endian unsigned 32 bit int `n`. The `n` bytes following that integer represent the byte string.
- If the tag is `0b010_11011` (additional type 27), the eight bytes following the tag are to be interpreted as a big-endian unsigned 64 bit int `n`. The `n` bytes following that integer represent the byte string.

### UTF8 Strings

The major type `3` (`0b011`) is used for utf8 strings. The additional type signals how the length of the string is encoded (exactly the same as for byte strings):

- If the additional type is `n` where `n` is smaller than `24`, the `n` bytes following the tag represent the utf8 string.
- If the tag is `0b011_11000` (additional type 24), the byte following the tag is to be interpreted as an unsigned 8 bit int `n`. The `n` bytes following that integer represent the utf8 string.
- If the tag is `0b011_11001` (additional type 25), the two bytes following the tag are to be interpreted as a big-endian unsigned 16 bit int `n`. The `n` bytes following that integer represent the utf8 string.
- If the tag is `0b011_11010` (additional type 26), the four bytes following the tag are to be interpreted as a big-endian unsigned 32 bit int `n`. The `n` bytes following that integer represent the utf8 string.
- If the tag is `0b011_11011` (additional type 27), the eight bytes following the tag are to be interpreted as a big-endian unsigned 64 bit int `n`. The `n` bytes following that integer represent the utf8 string.

A parser must check whether the decoded string is valid utf8, and indicate an error if it is not.

### Arrays

The major type `4` (`0b100`) is used for arrays. The additional type signals how the length of the array is encoded. It works exactly like the length encoding of strings. Once the length has been obtained, read encoded values until you read as many as the length specified.

### Maps

The major type `5` (`0b101`) is used for maps. Minor type and parsing work exactly like heterogenous arrays, except that the length indicates the number of key-value pairs, rather than single entries. A key-value pair is encoded by first encoding the key (always a string, but you must still use the correct tag), directly followed by the value.

## Canonical Encoding
For a canonical encoding:

- always use the smallest additional type that can contain the length of a string/array/map
- serialize maps lexicographically sorted by key (ignoring its tag). Note that this is not how cbor recommends doing it, cbor instead sorts first by length of the key, and then lexicographically. Either makes sense, no need to decide this right now. I lean toward lexicographic sorting, since that's the natural iteration order of most tree map implementations
