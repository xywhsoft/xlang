# Windows 正式更新 0.6.1 / ver2

2026-11-08 已发布到 `https://xcompiler.com/download/update_win_x64`。产品版本 0.6.1、版本代号 0x00000601，更新迭代 2，普通更新（非强制）。沿用正式通道与客户端已信任的 Ed25519 公钥。

本次包含 2,173 个受管文件，更新 139 个文件；新增 134 个压缩对象，合计 25,209,046 字节（24.04 MiB）。122 个过时受管文件被清退，其中 121 个是旧命令帮助源码快照，另一个是旧字体文件。文件删除范围在上线前已逐项检查。

已从真实 ver1 清单还原安装，使用正式更新器执行 ver1 → ver2，再逐项验证全部新文件、删除项、安装回执、同版完整性、CLI 版本、C / xlang JIT 与 IDE 启动。设置、用户项目、扩展目录和恢复文件哨兵均保留。上线按先对象、后清单、最后版本标记的顺序执行；服务器使用复用连接的 curl 逐项从公开 HTTPS 验证对象，并复核三个公开信息文件。前两次独立请求校验遇到 TLS／连接中断，均未激活版本；失败日志已保留，最终通过复用连接的校验后才切换版本。正式客户端在线确认 ver1 检出普通更新、ver2 为最新。

范围沿用既有更新器合同：不更新 extlibs、static、examples、projects 及用户设置；MDO 继承 ver1 的纳入选项，文件未变。Linux 没有在线发布。旧的非扩展命令帮助绑定仍是历史编译器指纹，本次已验证资料配对与安装后 GUI 启动，不将其表述为完整编译器／帮助兼容验收。

详细记录见 receipt.json、acceptance.json、online-verification.json、publish.log。update.json / update.lzma / version.txt 是线上精确字节；源码材料位于 dev/release-materials/sources/ide-update-v2-publish-20261108/。不包含签名私钥和 SSH 凭据。
