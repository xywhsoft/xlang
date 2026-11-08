# 新建与自定义模板

首页“新建”和工程菜单共用模板选择器。工程、独立文件为固定分类，不存用户配置；六个内置模板由代码注册。每次打开默认工程分类、控制台模板。独立文件保留 xlang/C 两种空白未保存文档。

## 选择器和图标

左侧树使用专门按 16×16 网格设计的小图标，附带 32×32 高 DPI 版本；XUI 原生图标专用绘制回调保留原有展开、文字、选择、命中和键盘行为。右侧使用原生虚拟 ListView 的行绘制；只绘制可见行，不为每条模板创建控件。96×72 DIP（4:3）的图标在左侧，标题后紧跟作者/版本，标题行右侧显示“点击查看说明”网页链接，下面固定两行说明。行高按实际字体度量计算，默认至少 96 DIP；更换字体、宽度后才重新准备文字宽度与两行说明。初始窗口 900×600，最小 760×480，正文独立滚动、底部按钮固定。

源 SVG 在 ide/data/template-icons，make_template_icons.py 生成 384×288 透明 PNG。XL/C 使用已有文件关联 SVG，以透明边扩展到 4:3；控制台、空白、混合和 DLL 延续同一配色。用户图片等比居中，不拉伸。

## 通用创建表单

选择工程后打开通用表单：工程名称、已有存放位置、为工程创建子目录、工作空间模式、工作空间名称、为工作空间创建子目录、设为启动工程，以及模板注册的字段。最终位置实时显示。

已有显式工作空间（包括尚未保存的工作空间）默认加入当前；没有工作空间或当前只打开独立工程时默认不关联。后者只保存 .xlp，不隐式创建 .xlw。工作空间名称和工作空间子目录选项仅在“创建新的工作空间”时显示。工作空间名称初始跟随工程名称；手动修改后停止跟随。

存放位置是基础目录。新工作空间勾选子目录时先追加工作空间名，再根据工程子目录选项追加工程名。工程清单继续使用统一模型生成 project.xlp，工作空间文件命名为 <工作空间名>.xlw。目录或文件冲突不覆盖。关闭不写盘。

字段由描述注册到共享表单布局：text、multiline、boolean、choice、integer、directory、file。ID 为稳定 custom.*；通用字段占用 project.* 和 workspace.*。default 保持类型，required、integer 的 min/max、choice 的 options 可配置。options 使用 {"value":"稳定值","label":"显示文字"}。visibleWhen/enabledWhen 使用 {"field":"字段ID","equals":值}，只能引用通用字段或前面已声明的字段，避免依赖循环。隐藏/禁用字段不传给生成器。校验错误显示在表单和对应字段旁。

## 安装自定义模板

模板目录位于当前设置文件旁的 templates/；便携版默认是 option/templates/<模板名>/。发布版 examples/templates/hello 提供可复制的完整示例。复制后重启 IDE，在首次打开“新建”时发现模板；浏览目录不会执行回调。无效、重复 ID 的模板被跳过并在选择器中显示原因，不影响内置模板。

每个目录包含 template.json、可选图标、生成器及资源。格式：

```json
{
  "format": "xlang-ide-template-1",
  "id": "example.hello",
  "kind": "project",
  "title": "欢迎工程",
  "description": "两行内说明",
  "author": "作者",
  "version": "1.0.0",
  "website": "https://example.com/templates/hello",
  "category": "示例模板",
  "form": {"fields": [
    {"id":"custom.greeting","type":"text","label":"欢迎文字","default":"你好","required":true}
  ]},
  "generator": {"kind":"xlang","module":"generator.xl","validate":"validate","create":"create"}
}
```

kind 为 project 或 file；category 是对应固定分类下的额外分组。website 为可选的 HTTP/HTTPS 模板说明页；缺省或空字符串不显示链接，其他协议或无效类型拒绝载入。点击链接通过系统默认浏览器打开，保持模板选择器不变。file 可设置 language 为 IDE 支持的语言，默认 xlang，最终应生成一个文本文件，作为未保存文档打开。

## 回调合同

回调使用普通导出的 xlang 函数：

```xlang
export func validate(var context) as var
    return null
end

export func create(var context) as var
    return {"entry":"main.xl", "kind":"exe", "files":[
        {"path":"main.xl", "kind":"xlang", "content":"func main(...) as int\n    return 0\nend\n"}
    ]}
end
```

validate 可省略。返回 null 表示通过；否则返回字段 ID 到错误文字的对象。create 返回生成计划。context.project 含 name/directory，context.workspace 含 mode/name，context.fields 以完整 custom.* ID 提供类型化字段；还包括 templateId/platform。回调通过这些值取得表单输入。

IDE 在独立任务目录写入 JSON 请求和受控入口，通过后台 xl.exe run 一次调用 validate/create，结果写入独立 JSON，stdout/stderr 作为日志。关闭/取消会停止此任务；有 120 秒上限。生成器是普通本机 xlang 程序，并非受限脚本沙箱。任务资料在设置目录 template-jobs/，不污染工程源目录。

生成计划的 path 必须是工程内相对路径，拒绝 ..、绝对路径、设备名、重复输出和 project.xlp 冲突；IDE 自行生成清单。content 默认是最终文本，需显式 expand:true 才展开 ${字段ID}，避免误改回调生成的语言模板字符串。单个资源/输出上限 16 MiB，累计 64 MiB，最多 1024 个文件。文件规则也可用 source 引用模板目录的资源；binary:true 原样复制。

没有 generator 时，template.json 的 files 数组直接作为静态生成规则，默认展开字段占位符。静态与回调的创建结果经过同一组检查和模型处理。

## 提交与失败处理

所有请求及模板描述复制给后台线程，生成与写盘不操作 UI。工程采用同一父目录下暂存，再通过目录句柄发布；不允许替换已有工程目录。选择不创建工程子目录时逐文件无覆盖发布，失败只尝试撤回本次身份仍匹配的文件，不声称多文件整体原子提交。无法清理的暂存内容保留证据路径，不递归删除陌生内容。

加入当前工作空间前重新核对完整模型基线；当前工作空间引用按正常保存流程保存。切换工程/新工作空间前复用未保存修改确认。新工程已生成但工作空间保存/关联失败时，显示已生成的工程路径，禁止当前表单重复生成；可直接打开该工程处理关联。创建过程中已打开文档和输入不会被后台线程修改。

## 验证

保留原生模板服务的冲突、文件身份与原编译器四模板验证；新增 typed/inactive 字段、真实 xl run 校验/创建/取消、中文参数往返、新建/不关联工作空间、当前工作空间关联以及图标/响应式表单测试。输入、设置、共享查找和首页会话回归独立运行。具体本次结果以 delivery receipt 为准，不将专项通过等同于全仓回归。
