# 自定义工程模板

将整个 hello 目录复制到 IDE 当前设置文件旁的 templates/ 中。例如便携版默认位置为 option/templates/hello/。重启 IDE 后打开“新建 → 工程 → 示例模板”。模板仅在首次打开新建窗口时加载，不会在浏览目录时执行 generator.xl。

欢迎文字和是否生成 README 是模板注册的字段，工程名称、位置、子目录和工作空间属于 IDE 的通用表单。创建时使用配套 xl.exe 在后台调用导出的 validate/create 函数。生成任务日志保留在设置目录的 template-jobs/ 下。

修改 generator.xl 后无需重新编译 IDE。回调返回的 content 默认视为最终文本；需要 IDE 展开 ${字段ID} 时，给相应文件显式设置 expand: true。不要在回调中另行创建 project.xlp 或工作空间文件，IDE 会根据返回的入口与文件列表生成清单。
