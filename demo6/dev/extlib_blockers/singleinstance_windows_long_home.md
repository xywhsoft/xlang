# singleinstance: Windows 深主目录下的锁文件打开失败

## 现象

2026-10-19 的 Windows 独立 GCC 包门禁中，将 `USERPROFILE` 临时指向
`demo6/build/test/singleinstance_guard_<32 hex>/home` 后，第一条合法的
128 字节锁名在 `xsi_acquire` 返回 -1。XRT 错误为
`failed to open the file`。同一程序、同一锁名和相同 XRT 构建，改用
`demo6/build/test/si_<12 hex>/home` 后通过。前者最终锁文件路径约 283
字符，后者约 245 字符；这强烈指向 Windows 传统路径长度边界，尚未
采集 Win32 system code，不把推断写成已证实的唯一根因。

包将最多 128 字节 UTF-8 名称精确 Base64URL 编码，最长文件名约 171
字符，再接到 `USERPROFILE/.xlang/singleinstance/`。因此普通 API 输入在
较深的合法用户主目录下可能失败，不是超长名称应被拒绝的情况。

## 复现与修复边界

原生用例为 `release/extlibs/singleinstance/test/test_singleinstance.c`；
构建方式见同目录 `check.ps1`。把门禁中临时设置的 `USERPROFILE` 换成
上述长目录，测试在首次 `xsi_acquire(boundary_name)` 失败；恢复短目录
即通过。Linux 同用例的 ASan/UBSan 门禁通过。

根因应在 XRT 的 Windows 路径转换与原生文件/目录 API 一起处理，
确保绝对路径超过传统长度时仍可打开文件。不能只给该扩展库加临时
`\\?\` 前缀，或把 128 字节公开名称限制悄悄降低；XRT 其他文件
操作也会遇到同类边界。修复后用深主目录、128 字节名称、中文名称、
跨进程锁争用及释放后重获复跑。当前扩展库任务不修改 XRT 主线，
本项保持发布阻断。
