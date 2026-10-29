# 已拥有局部字符串经 try/finally 返回时仍复制

## 状态与范围

2026-10-28 Windows 发布版 JIT 运行配套最小用例退出0，输出
`owned string try-return: PASS`；这是性能机会，不是行为错误，
不新增包的 M/R 分类。编译器基线为本工作树冻结的 `d64672903`，
未修改主线或 XRT。

```powershell
./release/xl.exe run dev/extlib_blockers/owned_string_try_return_copy.xl
./release/xl.exe cgen dev/extlib_blockers/owned_string_try_return_copy.xl --emit build/test/owned_string_try_return_copy.c
```

原生 `xrtStrDup` 已通过 `@owned string` 接管。`render` 的局部
`body` 只在返回时使用，finally 只释放另一个原生资源，但生成 C
在待返回前调用 `xlir_string_copy`，随后原局部仍按作用域释放。
这是一次额外正文复制，不是移交指针时的 `strlen`。

```c
body.Data = xrtStrDup(source.Data); /* 已拥有的 XRT 分配 */
pending_return = xlir_string_copy(body); /* 当前额外副本 */
xrtFree(resource); /* finally */
drop(body);
return pending_return;
```

`xfeat_save_location` 也命中这条边界：先生成 JSON，随后检查图片
写出，再从 try 返回局部 JSON；finally 释放定位句柄。不能为了
去掉这份副本而把 JSON 序列化移到写盘之后，否则序列化 OOM 会
发生在输出文件已被替换之后，改变失败语义。本任务未采用该改法，
也没有增加每包专用内存包装器。

现有返回副本自身也可能在图片写出成功后 OOM，当前保存 API 并不
承诺任意失败下输出文件不变。消除这份副本不仅减少复制，也能
减少一处写出后的失败点；本报告不把“序列化在写出前”夸大为事务。

## 主线建议

在拥有权/最后使用分析中证明返回局部唯一拥有、没有逃逸、finally
不会观察该局部时，将它移到待返回槽并清空原槽；pending 返回值
仍须在 finally 抛错时正确释放，不能仅删除 copy 调用。借用参数、
有别名或被 finally/闭包读取的值继续遵守原有快照规则。

补测成功返回、finally 抛错、原生 OOM、多个返回分支、借用与别名、
闭包捕获、资源释放恰好一次和原错误优先级；分别验证 JIT/AOT、
指针/释放器、模块租约与槽位生命周期。应统一改善 lowering，
不为 XFeat 添加新的返回 ABI，也不能把此报告当作这些测试已通过。

本轮仅验证 Windows 最小用例及生成 C；没有单独签收 Linux、完整
冷路径开销或所有带 finally 的返回形态。
