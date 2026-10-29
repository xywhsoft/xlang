# 参数化总门复用已验证的源码编译产物

`run_xlir_parameterized_guards.ps1` 新增可选的 `-ReusePublicDirectory`。
省略时仍从头构建；提供时先调用正式的源码 cohort reader，重新核对实际
源码、头文件、工具链、对象和驱动器哈希，并与选定源树逐文件比较。
只接受完整、输入未变、包含源码 XRT、启用编译器 API 的 public cohort。
Windows 还核对驱动器相邻的实际 TCC provider DLL，防止只核对 EXE 后
错误复用被替换的相邻运行库。

这只省去重复编译同一份源码。RuntimeOnly 构建、合同驱动器、全部 worker、
依赖阶段、package 分支和链接仍然重新执行，回执明确记录
`WorkerResultsReused=false`。不复用测试成功状态，也不把 public compiler
冒充 RuntimeOnly 驱动器。每轮保留独立的 cohort 核对回执。

使用本轮真实源码驱动器验证：Windows 1,295 TU，Linux 1,294 TU。
两端均接受真实 cohort；错误角色、关闭 compiler API、非源码 XRT、错误
驱动器／对象／头文件哈希、不同源树、未完成构建和变化的输入均拒绝。
Windows 还拒绝错误 provider 哈希。全程未修改原 cohort，也没有执行或
缓存任何 worker 结果。PS5/PS7 的原有 74-worker／194 项参数角色检查通过。

证据根：`E:/xlang-validation/five-groups-20261027/p7-shipping/` 与
`/home/ubuntu/xlang-validation/20261027-p7-shipping/` 下的
`source-reuse-check-*-r2`；完整参数化总门仍需实际运行，不能由这项工具
验证推导为通过。
