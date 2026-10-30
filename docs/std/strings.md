# Strings: `str` and `std.strings`

`str` methods are available everywhere; `std.strings` adds a few free
functions. All byte offsets are `int`; slicing panics unless offsets fall on
UTF-8 character boundaries.

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn str.new() str`, `fn str.withCapacity(n: int) str` | | ✓ |
| `fn (s: str) len() int` | length in bytes | ✓ |
| `fn (s: str) isEmpty() bool` | | ✓ |
| `fn (s: mut str) push(t: str)` | append; may allocate | ✓ |
| `fn (s: mut str) pushChar(c: u32)` | append a Unicode scalar value as UTF-8; **panics** if `c` is not one | ✓ |
| `fn (s: str) toOwned() str` | copy a borrowed string into a new owned `str`; allocates | ✓ |
| `fn (s: mut str) clear()` | | ✓ |
| `fn (s: str) bytes() StrBytes` | iterator over `u8` | ✓ |
| `fn (s: str) chars() StrChars` | iterator over Unicode scalar values (`u32`) | |
| `fn (s: str) startsWith(p: str) bool`, `endsWith` | | ✓ |
| `fn (s: str) find(p: str) Option<int>` | byte offset | ✓ |
| `fn (s: str) contains(p: str) bool` | | ✓ |
| `fn (s: str) trim() &str`, `trimStart`, `trimEnd` | borrow of `s` | ✓ |
| `fn (s: str) split(sep: str) StrSplit` | borrowing iterator of `&str` | ✓ |
| `fn (s: str) splitWhitespace() StrSplit` | | ✓ |
| `fn (s: str) lines() StrSplit` | splits on `\n`, removing a trailing `\r` | ✓ |
| `fn (s: str) replace(from: str, to: str) str` | allocates | ✓ |
| `fn (s: str) toLower() str`, `toUpper()` | ASCII only; allocates | ✓ |
| `fn (s: str) repeat(n: int) str` | allocates | ✓ |
| `fn (s: str) toCString() CString` | null-terminated copy for FFI | ✓ |

Iterating a `str` directly (`for b in s`) yields bytes (`u8`); use
`s.chars()` for Unicode scalar values.

## `std.strings`

| Function | Description | S0 |
|----------|-------------|:--:|
| `fn join(parts: [str], sep: str) str` | | ✓ |
| `fn fromUtf8(bytes: move List<u8>) Result<str, Utf8Error>` | validates | ✓ |
| `struct Builder` with `push`, `pushChar`, `finish() str` | efficient incremental construction | ✓ |
