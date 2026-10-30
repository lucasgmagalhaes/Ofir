# Strings: `str` and `std.strings`

`str` methods are available everywhere; `std.strings` adds a few free
functions. All byte offsets are `int`; slicing panics unless offsets fall on
UTF-8 character boundaries.

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn str.new() str`, `fn str.with_capacity(n: int) str` | | ✓ |
| `fn (s: &str) len() int` | length in bytes | ✓ |
| `fn (s: &str) is_empty() bool` | | ✓ |
| `fn (s: &mut str) push(t: &str)` | append; may allocate | ✓ |
| `fn (s: &mut str) push_byte(b: u8)` | **panics** if it would produce invalid UTF-8 | ✓ |
| `fn (s: &mut str) clear()` | | ✓ |
| `fn (s: &str) bytes() StrBytes` | iterator over `u8` | ✓ |
| `fn (s: &str) chars() StrChars` | iterator over Unicode scalar values (`u32`) | |
| `fn (s: &str) starts_with(p: &str) bool`, `ends_with` | | ✓ |
| `fn (s: &str) find(p: &str) Option<int>` | byte offset | ✓ |
| `fn (s: &str) contains(p: &str) bool` | | ✓ |
| `fn (s: &str) trim() &str`, `trim_start`, `trim_end` | borrow of `s` | ✓ |
| `fn (s: &str) split(sep: &str) StrSplit` | borrowing iterator of `&str` | ✓ |
| `fn (s: &str) split_whitespace() StrSplit` | | ✓ |
| `fn (s: &str) lines() StrSplit` | splits on `\n`, removing a trailing `\r` | ✓ |
| `fn (s: &str) replace(from: &str, to: &str) str` | allocates | ✓ |
| `fn (s: &str) to_lower() str`, `to_upper()` | ASCII only; allocates | ✓ |
| `fn (s: &str) repeat(n: int) str` | allocates | ✓ |
| `fn (s: &str) to_c_string() CString` | null-terminated copy for FFI | ✓ |

Iterating a `str` directly (`for b in s`) yields bytes (`u8`); use
`s.chars()` for Unicode scalar values.

## `std.strings`

| Function | Description | S0 |
|----------|-------------|:--:|
| `fn join(parts: &[&str], sep: &str) str` | | ✓ |
| `fn from_utf8(bytes: List<u8>) Result<str, Utf8Error>` | validates | ✓ |
| `struct Builder` with `push`, `push_byte`, `finish() str` | efficient incremental construction | ✓ |
