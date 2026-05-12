# xldbg VS Code DAP 接入

## 当前状态

截至 2026-05-12，VS Code 最小扩展骨架位于 `demo5/tools/vscode-xlang-debug`，使用普通 JavaScript 实现，无 npm 构建依赖。当前 adapter 只走 `release\xldbg.exe dap` 的 stdio 模式。

本文档记录 VS Code 侧最小接入约定。调试语义全部由 `xldbg dap` 提供，VS Code 扩展只负责启动 adapter 并转发 Debug Adapter Protocol。

## Debug Type

V1 使用 debug type：

```json
"type": "xlang"
```

推荐 launch configuration：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "xlang",
      "request": "launch",
      "name": "Debug XL Program",
      "program": "${workspaceFolder}/release/examples/full/bit_ops_basic.xl",
      "stopOnEntry": false,
      "xldbg": "${workspaceFolder}/release/xldbg.exe"
    }
  ]
}
```

## Adapter Command

扩展应以 stdio 模式启动 adapter：

```text
release\xldbg.exe dap
```

不要通过 shell 包装 adapter stdout。DAP stdout 必须只包含 `Content-Length` framed messages；日志和错误信息由 `xldbg` 写入 stderr 或 DAP `output` event。

## Configuration Mapping

VS Code configuration 到 DAP `launch` request 的映射：

| VS Code 字段 | DAP 字段 | 说明 |
| --- | --- | --- |
| `program` | `arguments.program` | 必填，XL 源文件路径 |
| `stopOnEntry` | `arguments.stopOnEntry` | 可选，默认 `false` |
| `xldbg` | adapter executable | 可选，默认可解析为 `${workspaceFolder}/release/xldbg.exe` |

V1 扩展不实现表达式求值、变量修改、条件断点执行、数据断点或自定义 stepping 语义。这些能力必须先在 `xldbg dap` 中实现，再由扩展声明。

`--listen` TCP 模式当前是 `xldbg` 的保留接口，不作为 VS Code 扩展默认接入方式。

## Minimal Extension Shape

VS Code 扩展只需要注册 debug configuration provider 和 debug adapter descriptor factory。当前 `tools/vscode-xlang-debug/extension.js` 已实现：

```ts
vscode.debug.registerDebugConfigurationProvider('xlang', provider);
vscode.debug.registerDebugAdapterDescriptorFactory('xlang', {
  createDebugAdapterDescriptor(session) {
    const xldbg = session.configuration.xldbg ?? '${workspaceFolder}/release/xldbg.exe';
    return new vscode.DebugAdapterExecutable(xldbg, ['dap']);
  }
});
```

路径解析、`${workspaceFolder}` 展开和 Windows path quoting 应在扩展侧完成，传给 `DebugAdapterExecutable` 时使用参数数组，不拼接 shell 命令行。

## Validation Checklist

- `initialize` 返回成功并发送 `initialized` event。
- `configurationDone` 返回成功。
- `setBreakpoints` 在 launch 前返回 pending unverified breakpoints，launch 后收到 `breakpoint(reason=changed)`。
- `launch` 后断点命中发送 `stopped(reason=breakpoint)`。
- `stackTrace` 返回 XL source path、line、column。
- `scopes` / `variables` 返回 Arguments / Locals。
- 未捕获异常发送 `stopped(reason=exception)`，继续后发送 `terminated`。
- Debug Console 不出现 raw DAP frame 或 debuggee stdout 污染。
