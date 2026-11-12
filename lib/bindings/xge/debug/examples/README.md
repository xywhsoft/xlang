# Debug capabilities

`capabilities.xl` prints the engine version and platform/graphics capability
report without starting a window or GPU. Use the matched debug SDK, built with
`XGE_DEBUGMODE=1`; the ordinary SDK deliberately does not publish this module.

```text
xl run capabilities.xl
xl build capabilities.xl -o capabilities.exe
```

`debugDumpCaps(Buffer)` writes into the existing buffer and returns the native
status. `XGE_ERROR_BUFFER_TOO_SMALL` means its NUL-terminated report was
truncated; enlarge the buffer and call again. It does not grow the buffer or
silently convert a partial report into a successful string. The example uses
8192 bytes, checks success, and explicitly removes the unused NUL tail before
printing. An initialized engine's debug calls belong on its creator thread.

The buffer is managed by xlang. No window, file, network service or manual
native pointer cleanup is needed.
