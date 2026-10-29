# ZIP 安全提取：锚定改名 API 尚未同步

## 当前边界

`release/extlibs/zip/xrt_zip_extract.c` 的 `xrtZipArchiveExtractAll` 会检查
目标根及目录分量，随后通过 `xrtFileTemp` 创建临时文件，并以
`xrtPathRename` 按路径提交。检查与提交之间，其他进程若替换目录分量，
提交可能重新解析到另一个目录。已有符号链接负例只验证检查时已经存在
的链接，不能证明并发替换安全。不要将当前接口宣称为不可信目录的
安全解压沙箱。

独立 XRT 仓库 `D:\GIT\xrt\include\xrt\file.h` 已声明
`xrtRootFileRename(xroot, source, target, replace)`，并有 Windows/Linux
定向测试；DEMO6 当前 `lib/xrt/xrt.h` 尚无该声明。直接让 ZIP 包调用
新 API 会使当前发布物不能构建，不能在包内复制 XRT 实现或添加弱符号
回退。

## 最小复现

1. 对照 `D:\GIT\xrt\include\xrt\file.h` 中的 `xrtRootFileRename`
   与 `lib/xrt/xrt.h` 的 `xrtRootFileOpen`/`xrtRootRemove` 声明段：
   后者没有锚定改名入口。
2. 查看 `release/extlibs/zip/xrt_zip_extract.c` 的
   `__xrtZipPrepareOutput` 与 `xrtZipArchiveExtractAll`：前者按路径
   `xrtPathStat`，后者按路径 `xrtFileTemp`、`xrtPathRename`。
3. `bash release/extlibs/zip/test/check_symlink.sh` 仅检查预先放好的
   符号链接；通过不消除步骤 2 的检查后替换窗口。

确定性竞争复现可在 Linux 测试构建中包装 `xrtPathRename`：准备归档
`sub/a.txt`、目标根 `dest/sub` 与根外目录 `outside`。包装器在原调用前
把已创建临时文件的 `dest/sub` 移到 `outside/sub`，再把 `dest/sub`
替换成指向 `outside/sub` 的链接，然后调用原始 `xrtPathRename`。
旧的临时文件和目标名称都会沿该链接在根外重新解析；断言根外不能出现
`a.txt`。这套注入尚未执行，不能把静态调用链当成已测得的越界写入结果。

## 收口路线

1. 由维护 XRT 同步的线程更新 DEMO6 单头文件并验证新 API 的实际链接，
   同时确认运行版/编译版资源选择均使用同一版本。
2. ZIP 包把提取目标固定为 `xroot`，相对条目路径逐级通过根句柄验证与
   创建；临时文件也必须通过同一锚定根排他创建，不能只把最后的 rename
   换成新 API。
3. 用锚定改名提交、根内删除失败临时文件；增加在检查与提交之间替换
   目录分量的确定性竞争测试，分别覆盖覆盖/不覆盖模式、Windows 重解析点
   与 Linux 符号链接。完成后复跑 C/XLang、sanitizer、互操作及 AOT 门禁。

本任务不修改 xlang 主线或独立 XRT 仓库；API 同步前暂停 ZIP 的发布级
安全签收，继续处理其他普通扩展包。
