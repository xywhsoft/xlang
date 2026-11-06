# xlang 更新制作与推送

客户端只需要 `update.exe`（Linux 为 `update`）。本目录是发布机工具，使用 Python 3.11+ 和 OpenSSH；不把 Python、SSH 或签名私钥安装到终端用户目录。

双击 `update_manager.bat` 或执行 `./update_manager.sh` 可进入简单菜单。自动化使用下面的命令；所有目录均可用绝对路径指定。

## 初始化与差分

在仓库根目录执行，例如：

```powershell
python ide/tools/update/update_manager.py init --platform win_x64 --source release/win_x64 --store E:/xlang-updates/win_x64 --version 1 --notes "首次发布"
python ide/tools/update/update_manager.py prepare --platform win_x64 --store E:/xlang-updates/win_x64 --version 2 --notes-file update-notes.txt
python ide/tools/update/update_manager.py list --platform win_x64 --store E:/xlang-updates/win_x64
python ide/tools/update/update_manager.py verify --platform win_x64 --store E:/xlang-updates/win_x64
```

`update/` 保存最新版本需要的全部对象；`verN/` 保存本次新对象。三个根信息文件为 `version.txt`、`update.json` 和 `update.lzma`。`.state/history/` 只保存很小的版本清单与记录，不保存全量客户端。支持时主线和差分对象使用硬链接，因此不要直接编辑对象文件。

## 程序版本与更新迭代

版本信息有两个独立字段，`version.txt` 的格式为：

```json
{"version": 4, "displayVersion": "0.6.0", "forceUpdate": false}
```

`displayVersion` 是显示用的程序版本，例如 `0.6.0`。IDE 构建从 `ide/data/version.json` 生成程序内版本与 Windows 文件版本资源，并在发行目录保存 `data/version.json`。关于窗口显示正在运行的程序版本，以及 外部状态目录 `installed.json` 中已安装的更新迭代号；没有安装回执时先读取随包 `data/release.json`，均缺少时显示迭代 0，不查询网络，也不会把线上最新版本误当成本机版本。

`version` 是每次上线递增的整数迭代号，更新检测只比较这个字段。相同 `0.6.0` 可以连续发布多个更新；改变显示版本号本身不会触发升级。`forceUpdate` 决定发现更高迭代时是否必须更新。

制作更新默认从发行目录 `data/version.json` 获取显示版本。其他来源可显式传入 `--display-version 0.6.0`；如果发行目录已有该文件，参数必须与文件一致。要升级程序版本，先修改 `ide/data/version.json` 并重新构建 IDE。签名清单、版本标记、历史记录及安装回执均包含显示版本；客户端核对版本标记与签名清单一致。已发布迭代不可原地修改，所有修改均需通过新迭代制作、签名和推送。

沿用 AutoTestIDE schema 4、SHA-256 对象名、LZMA-Alone、无压缩回退及 50 MiB 分片。清单新增 `platform`、传输大小/摘要及 Unix 权限。清单 JSON 使用 `xlang-update-signed-1` 封套：`payload` 是精确的 UTF-8 清单字符串，`signature` 对该字符串原始字节签名，避免不同 JSON 序列化器产生歧义；`update.lzma` 压缩同一 JSON。封套的 `publicKey` 只供审阅，客户端只信任本地固定公钥。

受管文件为 IDE、独立更新器、xlang 工具链、语言服务、公开头文件、官方资源/字体/帮助、必要说明及 `option/defaults/`。不遍历和上传 `extlibs/`、用户项目、示例、用户设置、会话、恢复数据、mdo-home、缓存或开发材料。`mdo` 默认自行更新，只有明确传入 `--include-mdo` 时纳入。完整 IDE 制包检查最小运行文件和命令帮助/来源成对；编译器帮助绑定状态另记入版本记录，保留旧资料的 GUI 交付应沿用已有验收边界。打包完整性校验不代替编译器/帮助兼容验收。

## 签名密钥

首次本机初始化的 32 字节 Ed25519 私钥位于 Windows `%LOCALAPPDATA%/xlang/update-signing.key`；Linux 工具默认查找 `$HOME/.local/share/xlang/update-signing.key`。通过 `--key` 指定其他私钥文件，通过 `--crypto` 指定签名助手。**私钥应单独备份，不提交、不上传、不放进 release。** 丢失密钥不能仅靠重新生成一把密钥恢复旧客户端的信任。

仓库的 `ide/update/publisher-public-key.txt` 固定发行公钥；正常重建客户端不需要私钥。更换公钥时，先发布携带新公钥的更新器，并使用 `--co-sign-key NEXT_SEED` 对同一清单追加新公钥签名；保留旧签名，使尚未更新的客户端仍能取得更新器。密钥轮换需保留足够迁移时间。

## 普通更新与强制更新

版本号文件仍名为 `version.txt`，内容为 JSON：

```json
{"version": 3, "displayVersion": "0.6.0", "forceUpdate": true}
```

普通发布默认 `forceUpdate: false`；制包时加 `--force-update` 开启强更，或在菜单中选择“强制旧版本更新”。例如：

```powershell
python ide/tools/update/update_manager.py prepare --platform win_x64 --store release/update_info/win_x64 --version 4 --force-update --notes "必须更新的修复"
```

标记同时写入签名清单。更新器核对两次版本信息及签名策略一致后，才通知 IDE；未验证的信息不能触发强更。已发布版本不可原地修改，改变强更策略应制作更大的版本号。新更新器也可读取旧数字版本号，按非强更处理；本项目首版不维护旧文件名的自动迁移链。

IDE 启动时先异步读取已验证的强更缓存，再后台查询服务器。普通更新可稍后处理；强更提供“立即更新／退出程序”，Esc、关闭提示或取消保存不能恢复 IDE 的正常使用。没有已验证强更策略时，网络故障允许离线使用；已有强更缓存时，断网仍需更新。新签名版本可以解除旧强更策略。`update.enabled: false` 仅关闭普通更新的自动提示，不能屏蔽强更检测。

## SSH 推送

先在发布机的 OpenSSH 配置中设置主机别名、用户、端口和密钥，并确认主机指纹。工具使用 BatchMode，不保存密码或私钥内容。

```powershell
python ide/tools/update/update_manager.py configure --platform win_x64 --store E:/xlang-updates/win_x64 --host xcompiler-publish --remote-root /实际网站目录/download/update_win_x64 --public-url https://xcompiler.com/download/update_win_x64
python ide/tools/update/update_manager.py push --platform win_x64 --store E:/xlang-updates/win_x64 --dry-run
python ide/tools/update/update_manager.py push --platform win_x64 --store E:/xlang-updates/win_x64
python ide/tools/update/update_manager.py status --platform win_x64 --store E:/xlang-updates/win_x64 --remote
```

可通过 `--ssh` 指定 OpenSSH 可执行文件，`--ssh-config` 指定独立 SSH 配置。`remoteRoot` 是服务器文件系统路径，必须以 `/update_<platform>` 结尾；`publicUrl` 是对应网页 URL，两者不要混淆。服务器需要 POSIX shell、tar 和 GNU coreutils（包括 `sha256sum`、`mv -T`）；无需运行 Python 发布服务。发布用户需要写入目标目录及相邻的私有发布暂存目录。公开目录使用 755、公开文件使用 644，私有暂存保持 700；网站目录应仅允许发布账号写入。端点应直接返回文件，不使用 HTTP 跳转。

公开下载校验采用直连，与 XRT 更新器保持一致，不使用发布机的系统或环境代理。

推送会验证所有引用对象，而不是只假设差分之外的线上文件存在。小文件按最多 128 个对象、64 MiB 分批通过一个 SSH 流上传到独立的私有暂存目录，整批 SHA-256 校验通过后才逐个改名；中断后继续复用已完成对象，并先通过公开 HTTP(S) 地址核对全部对象。随后切换 JSON、LZMA 并核对公开清单，最后才切换版本号。激活前失败会恢复旧信息文件；激活后若最终 HTTP 复核失败，则保留新版本和待复核状态，重跑 push 完成验证，绝不静默降低已激活的版本号。重跑继续复用已完成的对象。同一平台有远端发布锁，本地保留未完成任务的令牌，进程中断后仍可继续。

`--dry-run` 只展示目标、版本、对象数量和大小，不上传。版本相同时只接受字节相同的清单，并可补齐被删除的远端对象；已发布版本内容不能覆盖。网络中断按完整对象恢复，分片对象最多重新传输一个 50 MiB 分片。

服务器缓存建议：`version.txt` 使用 `no-store`，`update.json/update.lzma` 使用 `no-cache`，哈希对象使用长期不可变缓存。发布中不删除旧对象，保护已经取得旧清单的客户端。

## 本地恢复与范围

`rebuild` 使用历史清单和现存对象恢复最新主线及三个信息文件；`discard` 只允许撤销最新、未发布且没有未完成推送的版本。已经发布的坏版本应以更大的版本号重新发布修复内容。

首版不提供自动清理远端对象、任意历史版本自动回退或版本数据库。历史差分会积累，清理前必须保留当前版本和需要保留的历史对象；不要手动删除 `.state/push.json` 来绕过未完成的推送。

各平台分别使用独立工作目录和 URL：`update_win_x64`、`update_linux_x64`、`update_linux_arm`、`update_linux_arm64`。ARM32 与 ARM64 不共用目录。

当发布机的大文件网络链路不稳定时，可以加 `--verify-public-on-server`。该方式要求服务器有 curl 7.71+，从服务器访问公开 HTTPS URL，验证 TLS、每个对象的大小和 SHA-256；本机仍核对线上信息文件。它不跳过公开下载校验，也不关闭证书验证。

## 本仓库的日常维护流程

维护源码入口为 `ide/tools/update/`，签名助手构建在 `ide/build/Release/update/`，可直接运行的维护工具副本在 `release/update_info/tools/`。这些开发工具不进入客户端目录。

目录布局（`release/update_info/` 已加入 `.gitignore`）：

```text
release/
├─ win_x64/                    用户客户端
├─ linux_x64/                  用户客户端
└─ update_info/                本机维护资料，不打包给用户
   ├─ win_x64/                 Windows 主线、差分、信息文件与 .state 配置
   ├─ linux_x64/               Linux 独立更新库
   ├─ tools/                   维护工具与签名助手
   ├─ notes/                   更新说明
   └─ test-history/            此前测试通道归档
```

目录分工：

- `release/win_x64/`：用户可直接运行的发行内容。
- `release/update_info/win_x64/`：正式更新的主线对象、版本差分、清单、部署配置和小型历史记录；不要随编译目录清理。
- `release/update_info/test-history/win_x64/`：此前的测试版本 1–3，保留作测试历史。
- `dev/local-state/release-clean/`：清理时归档的个人设置、项目、mdo-home、构建和临时输出；清理不永久删除这些文件。
- `ide/build/update-runtime/`：仓库中实际运行客户端的更新缓存、日志、安装回执、强更策略和恢复事务。
- `dev/releases/`：面向用户的便携 ZIP。

先关闭使用此目录的 IDE、更新器、引擎和 mdo，然后执行：

```powershell
cd D:/GIT/x-lang
python ide/tools/update/release_maintenance.py inspect
python ide/tools/update/release_maintenance.py clean --apply
```

`clean` 不加 `--apply` 时仅预览。它保留程序、资源、字体、命令帮助、头文件、官方示例、许可证、`static/`、扩展库和 `option/defaults/`，其他顶层文件及个人配置归档到发布目录外。SDK/扩展包内部的来源与许可元数据会完整保留。旧更新状态由新更新器持锁迁移；发现未完成安装会要求先恢复，不会丢弃事务。

日常开发启动 IDE 使用 `powershell -File ide/tools/run_ide.ps1`，个人设置、会话和诊断写到 `dev/local-state/ide/`，仍运行真实 `release/win_x64/xlang_ide.exe`。

首次正式版本为迭代 1、显示版本 0.6.0：

```powershell
python ide/tools/update/update_manager.py init --platform win_x64 --source release/win_x64 --version 1 --display-version 0.6.0 --include-mdo --notes-file release/update_info/notes/0.6.0-v1.txt
python ide/tools/update/update_manager.py verify --platform win_x64
python ide/tools/update/release_maintenance.py bundle --platform win_x64
```

制作版本会生成并签名 `data/release.json`，便携 ZIP 解压后即能显示迭代 1，无需附带发布机的安装回执。ZIP 含运行依赖及扩展包；在线差分仍排除 `extlibs/`。`--include-mdo` 的选择保存在版本库配置中，后续继承，也可显式用 `--no-include-mdo` 取消。

**当前此前的线上测试通道已经到迭代 3，直接推送 1 会被正常的防降级保护拒绝。** 第一次正式上线前，需单独归档测试通道。以下命令只用于本项目尚未正式发行的这个测试通道；不用于正常已发行版本的降级：

```powershell
python ide/tools/update/retire_test_channel.py --platform win_x64 --expect-version 3
python ide/tools/update/retire_test_channel.py --platform win_x64 --expect-version 3 --apply
python ide/tools/update/update_manager.py push --platform win_x64 --verify-public-on-server
```

先按前面的 `configure` 步骤配置正式工作目录的 SSH。测试通道归档工具默认仅预览，执行时持发布锁并再次核对版本标记，保留旧通道全部字节；哈希对象用硬链接复用。若归档操作失败或中断，查看 `.state/test-channel-retirement.json` 中的归档路径并确认远端状态，不能盲目重置。正式发布后只允许递增迭代。

后续相同显示版本的修复仍递增迭代：

```powershell
python ide/tools/update/update_manager.py prepare --platform win_x64 --version 2 --notes-file release/update_info/notes/0.6.0-v2.txt
python ide/tools/update/update_manager.py push --platform win_x64 --verify-public-on-server
```

需要强更时，在 `prepare` 中加 `--force-update`。修改程序显示版本时，先修改 `ide/data/version.json` 并重新构建 IDE。未发布版本需要重新采集源目录时，可先 `discard` 再 `init`（撤销首次版本后）或 `prepare`；已发布版本不能撤销覆盖。`rebuild` 只恢复已有清单/对象，不重新采集源码或运行目录。

发布前还应核对编译器、IDE、语言服务、SDK、命令目录的验收记录。工具会记录命令目录与编译器摘要是否匹配；保留旧帮助资料的 GUI 验收不代表完整编译器/命令目录已经完成正式签收。
