# Windows 正式更新 0.6.2 / ver3

2026-11-09 已上线：https://xcompiler.com/download/update_win_x64 。产品版本 0.6.2，代号 0x00000602，普通更新，沿用既有 Ed25519 公钥。

本次包含 2,188 个受管文件，相对 ver2 更新 29 个；新增 27 个压缩对象，24,909,007 字节（23.76 MiB）。本次新增删除两份旧帮助源码快照；清单的累计删除项仍保留历史清退记录。

签名与全部对象校验通过。从真实 ver2 还原安装并升级 ver3，全部受管文件、删除项、安装回执、同版完整性、CLI、C/xlang JIT 和 IDE 启动检查通过；用户设置、工程、自有扩展及恢复文件保留。

上线先上传对象，再从公开 HTTPS 校验全部 1652 个对象，最后切换清单与版本标记。三个公开信息文件与本地签名包完全相同；正式客户端在线确认 ver2 检出普通更新，ver3 为最新。

原有更新范围不含 extlibs、static、examples、projects 和用户设置，MDO 继承原纳入规则且字节未变。Linux 0.6.2 为本地交付。历史命令帮助编译器绑定及其他基线限制见相邻 0.6.2 验收材料，未宣称全仓回归。

精确结果见 receipt.json、acceptance.json、online-verification.json、publish-record.json 和 publish.log。线上 update.json / update.lzma / version.txt 的原始字节已保存。不包含签名私钥及 SSH 凭据。

首次本地在线检查遇到一次 TLS 连接提前结束；失败日志保留，原客户端重试通过。对象上线前的服务器公开 HTTPS 校验已全部通过。
