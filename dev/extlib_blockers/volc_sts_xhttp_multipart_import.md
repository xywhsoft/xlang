# 扩展库依赖阻断：volc.sts 导入链中的 xhttp.multipart 无法通过语义检查

- 日期：2026-10-12；使用当前 `release/xl.exe`。
- 复现：在 `release` 执行 `xl check extlibs/volc.sts/src/sts.xl`；直接检查 `extlibs/volc.core/src/volc_core.xl` 也相同。
- 期望：`volc.sts -> volc.core -> xhttp.client -> xhttp.multipart` 导入链通过检查，之后才能做本包离线/在线测试。
- 实际：`extlibs/xhttp.multipart/src/xlang/xhttp_multipart.xl:352` 报 `[sem] unknown class/union field` 和 `Buffer.appendBytes` 无匹配成员。调用点为 `stable.appendBytes(source, c_size(string.byteLen(source)))`。
- 影响：`volc.sts` 尚不能进入 XLang JIT/AOT 测试；`volc.phone` 等同一依赖链可能受影响，未单独复核。本包 `json_string_field` 目前仍用字符串拆分解析 JSON，存在转义、空格和键碰撞风险，也尚未修改。
- 建议：在独立维护 x 生态的上下文核对当前 Buffer 字节追加 API 与 `xhttp.multipart` 调用的参数类型/长度语义，修复后重跑 `xl check`，再恢复 `volc.sts` 的结构化 JSON 提取、测试和范例开发。
- 状态：blocked-dependency；本任务不改 xhttp 包或 xlang 主线。
