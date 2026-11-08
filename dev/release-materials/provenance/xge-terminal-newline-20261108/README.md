# XGE Terminal 换行模式同步

默认 TEXT：LF 下移并回到行首；VT 模式保留原列号。CRLF、单独 CR、自动折行、ANSI 和中文保持各自语义。使用独立 Set/Get 接口，公开创建描述的布局没有改变。Clear、RIS、resize 和备用屏切换保留配置。切换前解析旧队列；回调关闭控件或重入解析都有 checked 状态保护。

XGE 实现提交 3919bdf9，ConPTY 范例提交 c8b3aadc，重入保护提交 67d12450。上游与配套引擎均通过 79 项换行专项，以及解析、输入、主题、查找和大输出相关验证。Windows / Linux 均通过声明生成 C、JIT、静态编译和脱离外部 XGE / XRT DLL 的独立运行。新增方法直接调用原生符号。

IDE 全部 108 个对象按匹配 SDK 重编；最后只变更 XGE 内部实现时，确认公开头和对象字节不变后重新链接。实际 release/win_x64 的真实程序构建与管道运行验证：原始输出仍是 Hello, World!xlang 6 true
程序启动，中文从第 0 列开始，结束光标为 (8,1)。输入、Unicode/IME、C/xlang、查找、设置、弹窗回归及实际 IDE 启动通过。

SDK、两平台动态与静态扩展包及 Windows IDE 已在本地同步。帮助目录新增 setNewlineMode / getNewlineMode 与两个模式常量，共 4 项，其余条目仅源行号允许变化。没有更新在线 ver2，也没有纳入语义／语言服务器工作区开发。release 保持 Git 忽略，仅提交声明、源码、SDK 配对材料和验证记录。

精确文件哈希、受保护编译器／语言服务器、SDK ABI、测试与源码恢复链见 receipt.json 和 evidence.zip；64 份历史 SDK 报告与补丁在原路径保持原字节，本次导入记录使用独立路径；导入工具也已据此更新。配套源码增量位于 dev/release-materials/sources/xge-terminal-newline-20261108/。本轮为专项验证，不代表全仓回归。
