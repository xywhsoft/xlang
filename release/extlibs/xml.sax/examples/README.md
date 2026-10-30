# SAX 双语流式目录范例

从仓库根目录运行，无需网络、磁盘 XML 输入或其他服务：

```powershell
./release/xl.exe run release/extlibs/xml.sax/examples/catalog_stream.xl
./release/xl.exe run release/extlibs/xml.sax/examples/catalog_stream_c.c
```

Linux 将 `./release/xl.exe` 换为 `./release/xl`，其余路径不变。
两份程序解析相同的中文图书目录，只累计数量和文本，不为统计构造
完整文档树；随后验证输入限额拒绝、下一次合法调用恢复及提前关闭。
独立断言结果后，输出同一个 UTF-8 黄金文件：

```text
books=2
names=语言运行库
limit rejected=true; recovery=true
XML catalog stream: OK
```

C 的事件指针及事件文本仅借用到下次 Next/Close/Destroy；累计结果
必须在此之前复制或消费。本例使用有界栈缓冲累计少量目录文本，
检查容量，并在所有已创建 Reader 路径释放资源。XLang 事件是可拥有
的动态值对象，解析器由类生命周期管理；两种表层不是相同成本，
本例不宣称动态事件装箱零开销。它也不是任意大文件的全量文本聚合器。

C 范例包含包内 SAX/Expat 实现以支持单文件 `xl run`；独立 GCC 构建
时只再链接一份 XRT，不能重复链接这些已包含的包实现。正式 import
仍由包清单分别编译。共用 `test/check_trees.ps1` / `check_trees.sh`
验证严格 C、两种语言的 JIT/AOT 和黄金输出；C 版本并不经过 XLang。

完整 API 教学例仍在 `release/examples/extlibs/xml_sax.xl` 和
`xml_sax_c.c`。公开 Reader 类型与安全构造的阻断见[包 README](../README.md)，
不能把本例工厂返回路径的通过当作整个包验收。
