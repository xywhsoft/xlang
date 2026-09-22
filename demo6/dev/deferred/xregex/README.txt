xregex xlang deferred assets

This directory preserves the current xregex ABI adapter, language tests,
and C/XLang parity examples removed from the xlang core product.
They must return as a source package backed by the public xregex extension,
with explicit import and package/link metadata. They must not be restored by
adding XRT_MODULE_REGEX or xrtRegex symbols to the trimmed XRT core.

Preserved dependent projects:

- projects/grep
- projects/logparse
- projects/rxgrep
- projects/xedit
- projects/xlint
- projects/xlsrv

Project-local commands still use their former release/projects/dev paths and
must be rewritten when the xregex package import and link contract is finalized.
