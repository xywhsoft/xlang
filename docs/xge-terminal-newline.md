# Terminal 换行模式

XGE 默认使用 `XUI_TERMINAL_NEWLINE_TEXT`。普通程序管道、日志或直接写入文本中的 LF (`\n`) 会下移一行并回到行首；CRLF 同样只换一行。单独 CR (`\r`) 仍回到当前行首，可用于进度覆盖。

原始 VT 模式可显式选择，适用于 PTY、ConPTY、SSH 和全屏 TUI：

```text
terminal.setNewlineMode(xui.XUI_TERMINAL_NEWLINE_VT)
terminal.setNewlineMode(xui.XUI_TERMINAL_NEWLINE_TEXT)
auto mode = terminal.getNewlineMode()
```

这些是 `xui.Terminal` 的直接原生声明，映射到 `xuiTerminalSetNewlineMode` / `xuiTerminalGetNewlineMode`。无效模式会 checked 报错。切换前会处理已排队输出，原有输出按原模式解析；如果在输出回调中重入切换或关闭控件，会返回 checked 状态，不递归解析队列或访问已退役对象。

模式在 Clear、RIS、resize 和主备用屏切换后保留。它只改变 LF 的光标动作，输入字节、原始日志、CR、自动折行及 `ESC D` 保持各自语义。IDE 的运行面板使用普通管道，显式选择 TEXT，不修改程序的 `print` / `println` 或原始日志。
