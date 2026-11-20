# Single-match code and text searches

`CodeDocument.findPlain()` and `CodeDocument.findRegex()` search an independent
document. `CodeEditDocument` supplies the same methods for an editor's observed
document view. `xui.findText()` uses the native find engine directly on counted
`string`, `bytes` or `Buffer` input. Text can contain embedded NUL. Patterns
are C strings and reject embedded NUL.

Pass `start` to choose where to start scanning, `rangeStart` and `rangeEnd` to
limit the search, and `flags` to select case sensitivity, whole word, backward
or wrapping search. The default searches the whole document forward. A negative
range end means the text end. Offsets are UTF-8 byte positions. The standalone
entry uses `XUI_FIND_*` flags, including `XUI_FIND_REGEX`. Document searches
use `XUI_CODE_SEARCH_*` flags.

Each call returns `(found, result)`. A hit returns `1` with an independent
record. A miss returns `0` and a zero-initialized record. A zero-width regex
match still reports a hit. Regular-expression captures use the native fixed
array, up to `iCaptureCount`. Capture 0 represents the entire match and an
unmatched group has start and end `-1`.

Allocation, invalid arguments and unavailable features raise checked errors.
Wrapping proceeds only after the first search leg misses. The optional regex
diagnostic Buffer is writable for the native call and retains its byte length.
A miss clears its first byte if the capacity is nonzero. Failed calls leave
previous language result variables intact. A closed editor view rejects later
queries through its existing observer.

Each language method directly declares its native entry. Plain document search
uses a 1 KiB local buffer when the range copy fits. Longer ranges retain the
native chunk scanner and heap-backed scratch. Document regex still copies the
selected range and compiles the pattern. The standalone find engine prepares
its pattern per invocation. The language layer checks the receiver, pattern,
span and status and delivers numeric/record results by value.

The [window-free example](../lib/bindings/xui/examples/code_search.xl) finds the
first item after a Chinese prefix and reads the next item's numeric capture.

## Standalone match snapshots and replacement expansion

`xui.findAllText(text, pattern, rangeStart=0, rangeEnd=-1, flags=0, limit=-1)`
scans the given range once for non-overlapping matches in ascending byte order.
Like the native collector, it ignores backward and wrap flags. `totalCount()`
counts the entire range. A positive limit keeps only that many early matches
in memory, zero retains no records, and `-1` retains all matches.

`FindMatches.results()` returns an immutable `NativeSequence<FindResult>`.
The sequence retains the native snapshot independently and needs no copy of
the entire match array. It remains readable after closing the snapshot or
changing the original input buffer. Publication occurs after the search and
record storage have succeeded. While collecting, geometric growth may move
the native allocation. The result records own only byte offsets and captures.

`xui.buildTextReplacement(text, pattern, replacement, flags, result)` expands
one match to `bytes`. Pass the same contents, pattern and flags used to produce
the match. Regex capture references use the existing native syntax, such as
`$1`. The input may be string, bytes or Buffer and is passed with its actual
byte length. Pattern and replacement are C strings. The output payload uses
the resident XRT allocator and is adopted without a payload copy. The bytes
value still allocates its normal owner. Use `encoding.decodeUtf8` when text is
needed.

The [batch example](../lib/bindings/xui/examples/find_matches.xl) limits retained
results, displays the full count, and expands a template from each capture.
