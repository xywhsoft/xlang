# volc.phone: dependency import and response handling

From `demo6`, run:

```powershell
.\release\xl.exe check release\extlibs\volc.phone\src\phone.xl
```

On 2026-10-21 this stops at `xhttp.multipart/src/xlang/xhttp_multipart.xl:352`:
`Buffer.appendBytes` is not a callable member. The dependency chain is
`volc.phone -> volc.core -> xhttp.client -> xhttp.multipart`. This is the
same independently maintained dependency issue recorded in
`dev/extlib_blockers/volc_core_import_chain.md`. Do not claim XLang/JIT/AOT
acceptance for `volc.phone` until that import is repaired and retested.

Package-local follow-up after the dependency is restored:

- `src/phone.xl` uses `string.indexOf` / `string.len` (UTF-8 scalar offsets)
  with `x_volc_phone_substr` (byte offsets). A response containing non-ASCII
  text before a searched field can cut inside a UTF-8 sequence. Use the
  standard `string.slice` scalar operation and remove the native substring
  helper rather than maintaining two offset conventions.
- `x_volc_phone_native.c` keeps eight return strings in a process-global
  unsynchronized ring; the allocated mutex is never locked. Concurrent
  callers can race, and the ring has no shutdown cleanup. Removing the
  helper also removes this lifetime problem.
- `json_string_field`, `json_int_field`, `json_section`, and
  `normalize_response` search raw JSON text. Escaped quotes, nested fields,
  and field-name text inside values can produce wrong data. Replace these
  with structured `json.parse` access and add offline fixture tests before
  using untrusted service responses.
- Typed request helpers serialize credentials and the JSON body into a
  newline-delimited spec. Embedded newlines can alter spec fields; route
  typed calls through `volc.core.request_acep` / `sign_preview_acep` and
  reserve `*_spec` for callers who intentionally supply that format.
- `phone_spec_dry_run` searches the entire spec for `dry_run=true`,
  `dry_run=1`, or `timeout_ms=0`; the body can contain these bytes. A live
  typed call with body `{"note":"dry_run=true"}` therefore selects signing
  preview instead of sending a request. Choose preview from a typed option,
  not by searching serialized credentials or body text. An offline mock must
  verify that body content cannot change the request/preview decision.
- `task_state` returns `"error"` for a response containing `"Error"`, while
  `wait_task` stops only for `"success"` or `"failed"`. A failed task therefore
  keeps polling until its timeout. Use one terminal-state vocabulary and
  test success, failure, still-running, and timeout with a deterministic
  mock clock/response sequence.

None of these package-local items is marked fixed by the dependency report.
The next acceptance pass needs C/XLang equivalent offline signing previews,
JSON response fixtures, one mock HTTP success/failure exchange, and ownership
checks. Real cloud credentials are not required for the mock gate.
