# DEMO6 复合应用范例

这个目录放置更接近真实使用场景的小工具范例。它们不是单点语法展示，而是把文件、路径、字符串、JSON、URL、hash、class、f-string、命令行参数等能力组合起来使用。

所有范例都自带输入数据、自行断言结果，并把临时文件写入 `build/test/examples_apps`，适合直接进入回归测试。

## 范例列表

- `asset_manifest.xl`：扫描一组本地资源，生成包含文件名、大小和 hash 的 JSON 清单。
- `cli_args.xl`：读取 `main(...)` 参数，解析一个小型命令行。
- `config_loader.xl`：从 JSON 配置加载到 class，并为缺失字段提供默认值。
- `csv_sales_report.xl`：解析 CSV 文本，按区域汇总销售额。
- `file_backup.xl`：复制配置文件到备份目录，并生成备份元数据。
- `log_summary.xl`：解析日志文本，统计 INFO/WARN/ERROR 数量。
- `static_site_builder.xl`：把页面模型渲染为一个简单 HTML 文件。
- `template_mail_merge.xl`：用 f-string 生成多封通知文本。
- `todo_json_store.xl`：把待办事项 class 持久化为 JSON 并读回。
- `url_batch_builder.xl`：批量构造带 query string 的 API URL。

## 运行

在 `demo6` 目录下：

```bat
test_apps.bat
```

或直接运行单个范例：

```bat
release\xl.exe run release\examples\apps\log_summary.xl
```
