# XLang Debug VS Code Extension

This minimal extension registers the `xlang` debug type and launches the existing adapter:

```text
release\xldbg.exe dap
```

Use it from this repository with a launch configuration like:

```json
{
  "type": "xlang",
  "request": "launch",
  "name": "Debug XL Program",
  "program": "${workspaceFolder}/release/examples/full/bit_ops_basic.xl",
  "stopOnEntry": false,
  "xldbg": "${workspaceFolder}/release/xldbg.exe"
}
```

The extension does not implement debugger semantics. Breakpoints, stepping, variables, stack traces, and exceptions are provided by `xldbg dap`.
