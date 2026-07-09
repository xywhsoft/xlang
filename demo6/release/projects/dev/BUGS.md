# dev 工具群 BUG 与手感问题记录

本文件只记录**当前未修复**的 BUG 和设计限制。已修复的 BUG 已从此文件移除。

---

## 未修复 BUG

### BUG-S1: string.splitLines 把末尾换行当分隔符，多出尾部空串
- 工具：sorter, diff, mdtoc 等
- 复现：`array a = string.splitLines("a\nb\n")`
- 现象：`a.length == 3`，元素为 `["a", "b", ""]`
- 期望：末尾换行作为行终结符，应得到 2 行
- 绕过：若末元素为 `""` 则 remove 一个
- 严重度：中
- 状态：设计行为，暂不修复

### BUG-M1: int() 对非数字字符串静默返回 0 而不抛异常
- 工具：mdtoc 等
- 复现：`int n = int("abc")` → n == 0，不抛异常
- 期望：应抛异常或有 tryParse
- 绕过：自行逐字符校验
- 严重度：低

### BUG-GS2: `hashable` 非法泛型约束名
- 工具：gstack
- 复现：`func f<T is hashable>(T x)` → `[sem] unknown generic constraint`
- 现象：`hashable` 不是合法约束名；合法名集合未文档化
- 绕过：使用 `comparable`/`cloneable`/`json` 或 class 继承约束
- 严重度：低

### BUG-V1: 复合赋值 `+=` `-=` 等不派发到 class 运算符协议
- 工具：vops
- 复现：`a += b`（a/b 为自定义 class）→ `[sem] numeric compound assignment operands must be numeric`
- 期望：应派发到 `__add__` 协议
- 绕过：展开为 `a = a + b`
- 严重度：中

### BUG-P1: 复合赋值以 class 属性为左值被拒
- 工具：props
- 复现：`m.hp += 10` → `[sem] class property assignment only supports =`
- 期望：应支持复合赋值
- 绕过：`m.hp = m.hp + 10`
- 严重度：低（与 BUG-V1 同根因）

### BUG-O1: `var` 实参无法传给重载函数
- 工具：overload
- 复现：重载 `func f(int)` / `func f(float)`，传 var 值 → `[sem] 没有匹配的函数重载`
- 绕过：先 `int(v)` 或 `float(v)` 显式转换
- 严重度：中

### BUG-O2: `__item__` 重载返回 `array<int>` 运行期崩溃
- 工具：overload
- 复现：`func __item__(int row) as array<int>` → 0xC0000005 硬崩溃
- 绕过：避免协议方法返回 typed 容器
- 严重度：高

### BUG-O3: get 属性零维+一维重载不工作
- 工具：props, overload
- 复现：`get hp() as int` + `get hp(int) as int`，访问 `m.hp` → `[checked] 找不到 class 方法`
- 绕过：使用不同属性名
- 严重度：中

### BUG-D2: `derive string/hash/equals` 未实现
- 工具：deepclone
- 复现：`class X { derive string; }` → `[sem] reserved but not implemented in demo6`
- 现象：编译期明确拒绝（诊断已改进，不再静默通过）
- 绕过：手写 `__eq__`/`__ne__`/`__toString__`/`hashByFields`
- 严重度：中（功能缺失，但诊断清晰）

### BUG-U1: union 实例 `string(type(u))` 返回 "unknown"
- 工具：unionbits
- 复现：`string(type(unionInstance))` → `"unknown"`（class 实例正常返回类名）
- 绕过：用 `type(u) == T` 或 `u is T`，不依赖字符串化
- 严重度：低

### BUG-E1: `encoding.toBytes` 加 BOM flag 返回空 Buffer
- 工具：encconv
- 复现：`encoding.toBytes("text", encoding.utf8 + encoding.bom)` → `buf.len() == 0`
- 绕过：不加 BOM flag，需要 BOM 时手工拼接
- 严重度：中

### BUG-HD1: native-backed class 不能从已有具名值 return
- 工具：hexdump
- 复现：`Buffer b = ...; return b` → `[sem] 不可复制类型不能从已有值返回`
- 注意：作函数参数传入 OK；`return Buffer()` 临时构造 OK；仅 return 已有具名值被拒
- 绕过：操作 Buffer 的函数接收参数而非返回
- 严重度：中

### BUG-HD2: `string.formatInt(n, "c")` 不按字符格式输出
- 工具：hexdump
- 复现：`string.formatInt(65, "c")` → `"65"`（期望 `"A"`）
- 绕过：手写 ASCII 映射
- 严重度：低

### BUG-K1: json pretty 模式键序可能 hash 重排
- 工具：kvdb 等
- 复现：`json.stringify(json.parse('{"a":1,"b":2}'), pretty=true)` → 键序可能变化
- 注意：compact 模式保留插入序；pretty 模式依赖 hash 表实现
- 绕过：需要保序时手工拼接 JSON 文本
- 严重度：低

### BUG-RT1: `array<Base>` 在 `append(Derived)` 时静默丢弃派生类实例
- 工具：raytrace
- 复现：
  ```xlang
  class Shape { virtual func area() as float ... end }
  class Sphere extends Shape { override func area() as float ... end }
  array<Shape> arr = array[]
  arr.append(Sphere(...))      // 静默失败
  println(arr.length)          // 0（不是 1）
  ```
- 现象：编译通过、运行不抛异常，但 `arr.length` 仍为 0；append 的派生类实例被静默丢弃
- 期望：要么完整存入（保留 vtable），要么编译期拒绝
- 绕过：用 `array<var>` 代替 `array<Base>`，取用时 `as Base` 还原
- 严重度：高（静默错误，极难发现）

### BUG-RT2: typed `Base` 字段 + `derive value` 把派生类按值切到 Base
- 工具：raytrace
- 复现：
  ```xlang
  class Mat  { derive value; virtual func sc() as int ... end }
  class Lam extends Mat { derive value; int bonus=0; override func sc() as int return self.v+self.bonus end }
  class Holder { derive value; Mat m = Mat(0); create(Mat m) { self.m = m } }
  Lam l = Lam(10, 5)        // sc() -> 15
  Holder h = Holder(l)      // l 存入 Mat m 字段
  println(h.test())         // 11（不是 16）：bonus 与 override 都被切掉
  ```
- 现象：派生类实例存入带 derive value 的基类字段时发生 **slicing**，丢失派生数据 + vtable
- 绕过：用 `var` 字段承载多态实例，取用时 `as Base` 还原（实测 vtable 保留）
- 严重度：高（语言层面的多态陷阱）

### BUG-RT3: 从函数返回带 virtual 方法的 class 实例丢失 vtable
- 工具：raytrace
- 复现：
  ```xlang
  func makeSphere(...) as Sphere
      Sphere s = Sphere(...)
      return s                 // 返回时丢失 vtable
  end
  World w = World()
  w.add(makeSphere(...))       // 加入 array<var>
  Shape s = item as Shape
  s.hit(...)                   // 0xC0000005 崩溃，virtual 调用打到错误槽位
  ```
- 现象：返回有 virtual 方法 + var 字段的 class 实例时，vtable 指针没有正确迁移到调用方；
  后续 dynamic dispatch（override 方法）触发访问违例
- 绕过：在调用方直接 `new` 实例（不要从函数返回带 virtual 的 class），传给接纳函数（如 `w.add(s)`）
- 严重度：高（致命崩溃，无明显警告）

### BUG-RT4: 「class 构造嵌套 class 构造」触发 statement-level lowering 拒绝
- 工具：raytrace
- 复现：
  ```xlang
  Ray r = Ray(Vec3(0,0,0), Vec3(1,0,0))   // [checked] class constructor expression requires statement-level lowering
  return false, HitRecord()               // 同样错误
  ```
- 现象：把 class 构造表达式直接作为另一 class 构造的实参，或直接出现在 return 多值表达式里，
  编译器拒绝；要求先把内层构造降到语句
- 绕过：先 `Vec3 v = Vec3(...)` 再 `Ray r = Ray(v, v)`；return 时先 `HitRecord m = HitRecord(); return false, m`
- 严重度：中（语法限制，工作量大但不致命）

### BUG-RT5: 局部变量名与标准库名空间冲突时被误判为命名空间访问
- 工具：raytrace
- 复现：
  ```xlang
  Vec3 dir = ...      // 「dir」与 stdlib `dir` 命名空间冲突
  if dir.nearZero()   // [stdlib] standard library namespace has no such symbol
  ```
- 现象：当局部变量名与标准库名（`dir`/`path`/`time`/`math`/...）重名时，编译器把 `dir.xxx` 解释为
  stdlib 命名空间访问而非局部变量方法调用；非二义场景下应优先局部变量
- 绕过：避免使用 stdlib 命名空间同名作为局部变量（用 `direction`/`d` 等）
- 严重度：中（容易踩，错误信息不直观）

### BUG-RT6: 多返回值类型不匹配时缺少候选信息
- 工具：raytrace
- 复现：
  ```xlang
  bool scattered; Ray scatteredRay; Color attenuation
  scattered, attenuation, scatteredRay = mat.scatter(r, rec)  // scatter 返回 (bool, Ray, Color)
  // [sem] 多返回值赋值类型和目标变量类型不匹配: target Color, value Ray
  ```
- 现象：错误信息只指出「目标位置 i 期望 Color 收到 Ray」，没说清楚整体解构顺序，
  但功能上正确（编译期捕获），调试时需要核对函数返回签名
- 严重度：低（功能正确，只是诊断信息可改进）


### BUG-JP1: 普通 class 作为另一 class 的成员字段需要 `derive value`
- 工具：jsonparse, calc（回归）
- 复现：
  ```xlang
  class Inner
      int x = 0
  end
  class Outer
      Inner f                     // <- 触发
      create(Inner f)
          self.f = f
      end
  end
  ```
- 现象：`[checked] class persistence requires __toValue__ or derive value/json/xson`，定位在 `class Inner` 声明处
- 边界：同一 class 按值传给**普通函数**（非成员字段）OK；仅作为另一 class 的**成员字段类型**时被拒
- 影响：`calc` 工具（Tokenizer/Token 作为 Parser 字段）当前已无法 `xl run`，属回归
- 绕过：给被嵌入的 class 加 `derive value`
- 严重度：中

### BUG-WINUI1: `extern` 函数返回 `string` 时 xlang 不拷贝 C 的 `const char*`（别名借用）
- 工具：winui（probe_string_copy.xl / probe_string_copy2.xl）
- 场景：C 混编 `extern "c_fn" func f(...) as string`，C 侧返回 `const char*`
- 复现：
  ```xlang
  // C: const char* winui_get_text(void* h) 写入并返回一个 static buffer
  string sa = a.getText()   // C 返回指向 static buffer "first" 的指针
  string sb = b.getText()   // C 重写同一个 static buffer 为 "second"
  println(sa)               // 实际打印 "second"（期望 "first"）
  ```
- 现象：
  - `string s = externFn()` 把 C 返回的 `const char*` 直接当作 xlang string 的内部指针存储（**借用/别名**），不拷贝到 xlang 拥有的存储。
  - 第二次调用同一（或任何复用该内存的）C 函数会静默改变第一次返回的 xlang string。
  - 不崩溃（说明 xlang 不会对返回的 `const char*` 调用 free，否则 static buffer 会崩），因此返回 malloc 内存会泄漏、返回 static buffer 会别名——两种常见 C 模式都踩坑。
- 边界：
  - 读取后立即使用（中间无其它覆盖调用）结果正确。
  - `string s = "" + externFn()`（拼接）、`arr.append(externFn())`（装入 typed container）会强制拷贝，是可靠的绕过。
  - 同时影响 free `extern func` 和 native-backed class 成员 `extern func`（同一条 string 返回 ABI）。
- 根因推测：xlang 运行时把 C 返回的 `const char*` 包装成 string view 而非 owned string。等价于 Python ctypes `restype = c_char_p` 的「不拷贝」行为。
- 期望：`extern ... as string` 应当把 C 返回的 `const char*` **深拷贝**到 xlang 拥有的、引用计数管理的 string 存储（与所有其它 xlang string 赋值一致），让 C 侧可以安全返回 static/malloc 任意来源的字符串。
- 绕过：`string s = "" + externFn()` 或 `string s = string(externFn())` 或先装入 `array<string>`。
- 严重度：高（静默错误数据；C interop 极常见模式；无明显警告）
- 状态：未修复

### BUG-XB1: 文件作用域 var 数组在 HTTP 回调里元素引用丢失
- 工具：xblog
- 复现：
  ```xlang
  var gPosts = []          // 文件作用域 var 数组

  func fill()
      gPosts.push({"slug":"a","title":"A"})
  end

  func route(point reqHandle, point respHandle) as bool
      // gPosts.length 报告正确值（这里 == 1）
      // 但 gPosts[0] 返回空；string(gPosts[0]) 是 ""，
      // gPosts[0]["slug"] 也是 ""，for k,v in gPosts[0] 直接崩溃
  end

  func main(...) as int
      fill()
      println(gPosts[0]["slug"])   // main 里访问完全正常，打印 "a"
      http.server.getRequest(server, "/", requestHandler)
      http.server.start(server)    // route 回调里访问失效
  end
  ```
- 现象：
  1. 文件作用域的 `var gPosts = []` 在被 `push(dict)` 填充后，**从 `main` 直接访问 `gPosts[0]["slug"]` 完全正常**。
  2. 同一进程里 HTTP server 的 C-callback 回调（`func(point, point) as bool`）里：
     - 字符串/整数字面量全局变量（如 `gPostsPath`、`gListTpl`）访问**完全正常**；
     - `gPosts.length` 返回**正确**值（数组容器壳还在）；
     - 但 `gPosts[i]` 返回空，`string(gPosts[i]) == ""`，`gPosts[i]["key"]` 也是 `""`，`for k,v in gPosts[i]` 触发崩溃；
     - 也就是说：**容器元信息（length）保留，但元素引用全部丢失**。
  3. 在 route 回调里**新建的**局部 var dict（`var fresh = {}; fresh["x"]="y"`）访问完全正常；只有跨回调边界的全局 var 容器元素有问题。
  4. 与 worker 数量无关（workerCount=1 同样复现）。
  5. 同样试用 `class Store { var posts = [] }; Store gStore` 包装也无效（class 实例全局在回调里其 `var` 数组字段元素同样丢失）。
- 影响：需要在 HTTP 回调里共享可变 var 数据的 xlang server 应用。
- 绕过（xblog 已采用）：每次请求**重新从磁盘 `loadPosts()` 拿到一个局部 var**，局部 var 在回调里访问完全正常。代价是每次请求一次文件读，但小型博客可接受。
- 推测根因：native C-callback 跨边界时，var 数组元素的 marshalling 把 length 字段成功复制但元素指针数组没有正确传递（或被深拷贝成空）。需要 XRT 侧排查 `var` 容器跨 native callback 边界的传值/传引用语义。
- 严重度：高（影响 server 应用架构，但存在低成本绕过）

### BUG-C1: class 方法不能安全接收 `array<int>` / `Buffer` 参数
- 工具：chip8
- 复现：
  ```xlang
  class Foo
      array<int> mem
      create()
          self.mem = array[0, 0, 0]
      end
      func loadX(array<int> src)              // array<int> 形参
          int n = src.length                   // <- [checked] class field receiver is not a class value
          self.mem[0] = src[0]                 // <- [checked] 当前 C 生成暂不支持将该表达式打包为 var
      end
      func loadBuf(Buffer buf)                 // Buffer 形参
          int n = buf.len()                    // 前端通过，C 生成阶段：tcc: __xlvfs__/module.c: invalid type
      end
  end
  ```
- 现象：
  1. **`array<int>` 形参**：在 class 方法内对形参 `src` 调用 `.length` 或 `src[i]`（读），编译器一律报 `[checked] class field receiver is not a class value`，似乎把形参当成了隐式 `self.` 字段访问来解析；同一形参在**普通全局函数**里完全正常。
  2. **`Buffer` 形参**：xlang 前端（check）通过，但下游 tcc C 代码生成报 `invalid type`，链接失败。
  3. 对照：class 方法内访问**自身字段** `self.mem[i]`、`self.mem.length`（赋给 int 局部后 return）完全正常；仅**形参容器**有问题。
- 绕过（chip8 已采用）：把所有需要 `array<int>` / `Buffer` 形参的逻辑从 class 方法移到**普通全局函数**，签名改为 `func loadBytes(Chip8 cpu, array<int> prog)`（`Chip8` 实例 + 容器都作形参，普通函数支持良好）。
- 严重度：中（限制 OOP 封装手感，但全局函数 + 实例首参的绕过清晰可用）

### BUG-C2: class 方法 `return self.field.length` 不能直接返回容器 length
- 工具：chip8
- 复现：
  ```xlang
  class Foo
      array<int> mem
      create() self.mem = array[0,0,0] end
      func showLen() as int
          return self.mem.length        // <- [checked] 当前 C 生成暂不支持将该表达式打包为 var
      end
  end
  ```
- 现象：直接 `return self.mem.length`（或其它把容器 length 表达式作为 return 值）触发 C 生成拒绝；似乎 length 表达式被推断为某种不可直接打包为 var 的中间类型。
- 绕过：先落到局部 `int n = self.mem.length; return n`（chip8 全程采用此写法）。
- 严重度：低（一行绕过）

### BUG-XL1: typed 容器（array<T>/set<T>）作为函数实参按值拷贝，修改不回传
- 工具：xlint
- 复现：
  ```xlang
  func fill(set<string> s)
      s.add("x")
  end
  set<string> d = set<string>{}
  fill(d)
  println(d.contains("x"))   // false（期望 true）
  ```
- 现象：`array<T>` / `set<T>` / `array<Local>` 等静态容器作为函数参数时被**整体按值拷贝**，函数内的 `push`/`add`/`remove` 不会影响调用方的容器。
- 注意：这与「变量是值语义」一致，但对习惯了引用容器（Python list / JS array）的用户极反直觉；`array<var>` / `var` 容器同样是值拷贝。
- 绕过：用全局容器（函数直接访问全局，xlint 已采用），或让函数 `return` 新容器由调用方赋值。
- 严重度：中（设计语义，但容易踩）

### BUG-XL2: 长时间运行的单进程持续泄漏/变慢（json.parse 大树 + 容器累积 + 递归遍历）
- 工具：xlint（递归 lint 整个 dev 目录）
- 复现：在一个进程里循环对 ~90 个 .xl 源文件做 `json.parse(largeAST)` + AST 遍历 + 累积 issue 到 `array<Issue>`，前几个文件正常，随后**每文件耗时指数级增长**（0.3s → 13s），92 文件版本 >15 分钟未完成。
- 子定位（均用最小复现探针确认）：
  1. **递归函数遍历 var 树泄漏**：`func walk(n){ for c in n["children"] walk(c) }` 反复调用 `json.parse` + 递归 walk，20 轮内从稳定变到卡死；改写成**显式 `var` 栈的迭代遍历**后稳定（25 轮 106s 不变）。
  2. **typed `array<int>`/`array<bool>` 的 push/remove 泄漏**：迭代遍历用 `array<int>`/`array<bool>` 作辅助栈时同样快速泄漏；改用 `var` 数组承载栈状态后改善。
  3. **即便全用 var 栈，跨文件累积 `array<Issue>` + 反复大 `json.parse` 仍会逐步变慢**：单文件都 <1.6s，30 份同文件在单进程内 >120s 未完成。
- 根因推测：XRT 的引用计数/GC 在长生命周期进程里无法及时回收大 var 树与累积容器，堆膨胀导致后续分配/遍历变慢。
- 绕过（xlint 已采用）：**多文件时 fan-out**——主进程对每个文件 `process.run` 起一个短命子进程（子进程做单文件 lint 输出 JSON），主进程聚合结果。每个子进程短命、退出即释放全部内存。92 文件从 >15min 降到 **~21s**。
- 严重度：高（任何「在单进程内处理大量数据/文件」的 xlang 程序都会撞上；fan-out 是通用绕过范式）

### BUG-XL3: `is` 是保留字，不能作标识符/循环变量
- 工具：xlint
- 复现：`for is in items` 或 `Issue is = Issue(...)` → `[parse] 语法错误`
- 绕过：改用其它名字（如 `iss`）
- 严重度：低（与 `and/or/not/in` 同类保留字，但易被忽略）

### BUG-XL4: `math.abs(int)` 重载解析失败
- 工具：xlint
- 复现：`int v = 5; if math.abs(v) >= 1 then ... end` → `[sem] 函数调用实参类型和形参类型不匹配 target float, value int`
- 现象：`math.abs` 既有 int 重载也有 float 重载，但传 `int` 实参时被解析到 float 重载并拒绝
- 绕过：字面量 token 无符号时无需 abs；确实需要时手写 `v >= 0 ? v : -v`
- 严重度：低

### BUG-XL5: `array<T> x = stdlibFunc()` 被拒
- 工具：xlint
- 复现：`array<string> ls = string.splitLines(s)` 或 `array<string> fs = dir.files(p, true)` → `[checked] 静态泛型容器目前只支持同类容器字面量初始化`
- 现象：用标准库返回的动态数组初始化静态 `array<T>` 被拒；只能用字面量或 `array[]` + `push` 初始化
- 绕过：先 `var tmp = stdlibFunc()`，再 `for x in tmp { ls.push(string(x)) }` 构造 typed 数组
- 严重度：低（有清晰绕过）

### BUG-XL6: `process.run` 把命令串中的引号字面化
- 工具：xlint
- 复现：`process.run("xl.exe parse \"path\" --format json")` → xl 收到的文件名是 `"path"`（**带引号**），报「无法读取源文件」
- 现象：经 `process.run`（走 cmd shell）传递的 `"..."` 引号被原样并入参数，而非被 shell 剥离
- 绕过：用**不带引号的绝对路径**（docgen 同款写法），路径含空格时无可靠绕过
- 严重度：低（路径无空格即可）

### BUG-XL7: `time.clock()` 在 `xl run` 下恒返回 0
- 工具：xlint（调试计时用）
- 复现：`xl run` 执行脚本时反复 `println(time.clock())` 全部打印 0
- 现象：`time.clock()` 未按预期返回单调递增的毫秒/微秒计时，无法用于测量耗时
- 绕过：外部用 PowerShell `Stopwatch` 测量
- 严重度：低

### BUG-XT1: `catch e` 后 `m = e.message` 借用异常缓冲区，跨作用域使用变乱码
- 工具：xtest
- 复现：
  ```xlang
  for name in tests
      string errMsg = ""
      try
          doSomething()            // 内部 throw "failure detail (expected 2, got 1)"
      catch e
          string m = e.message     // 此刻 m 内容正确（len=44）
          errMsg = m               // 借用，未拷贝
      end
      // 后续把 errMsg 存入 array/dict 并在循环结束后 json.stringify
      results.push({"message": errMsg})
  end
  println(json.stringify(results)) // message 字段变成 "\b??" 等乱码（每次运行不同）
  ```
- 现象：`catch e` 绑定的 `e.message` 是对异常对象内部字符串缓冲区的**借用/别名**，而非 owned 拷贝。把它赋值给另一变量并存入容器累积、跨多次循环迭代后引用，底层缓冲被复用/释放，读到悬空内存（表现为 JSON 中出现 `\b` 等控制字符 + 每次运行不同的乱码字节）。
- 边界：
  - 在 catch 块内**立即**消费（如直接 `println(m)`）结果正确。
  - `string.mid(m, ...)` 等会生成**新字符串**的运算结果是 owned，可安全跨作用域使用。
- 绕过（xtest 已采用）：`string m = e.message + ""`（拼接空串强制生成 owned 拷贝）。
- 严重度：高（静默数据损坏；try/catch 是极常用模式）
- 状态：未修复

### BUG-XT2: 普通 class 实例作为函数参数按浅拷贝传递，标量字段修改丢失
- 工具：xtest
- 复现：
  ```xlang
  class Info
      array<string> items
      bool flag
      create() self.items = []; self.flag = false end
  end
  func fill(Info info)
      info.flag = true          // 标量字段
      info.items.push("x")      // 容器字段
  end
  Info i = Info()
  fill(i)
  println(i.flag)               // false（期望 true）—— 标量修改丢失
  println(i.items.length)       // 1   —— 容器修改可见（共享引用）
  ```
- 现象：普通（非 native-backed）class 实例作为函数参数时被**浅拷贝**：标量字段（bool/int/float/string）的写操作作用于副本、调用方不可见；容器字段（array/dict/set）因是引用、副本与原对象共享同一底层容器，故修改可见。
- 边界：
  - 从函数 `return` 一个 class 实例（含已修改的标量字段）完全正常（返回值拷贝保留全部字段，已用最小复现确认）。
  - 仅「作为参数传入并在被调方就地修改标量」受影响。
- 绕过（xtest 已采用）：让被调函数**构造并 return** class 实例（`func build(...) as Info { Info x = Info(); x.flag = true; return x }`），而不是就地修改传入参数。
- 严重度：中（反直觉；与 BUG-XL1「容器参数按值拷贝」同根因——值语义——但 class 标量字段丢失更隐蔽）
- 状态：未修复

### BUG-XT3: 在 main 中重绑定的全局容器，工作线程访问即崩溃（EXIT=3）
- 工具：xtest（并发模式 `--jobs N>1`）
- 复现：
  ```xlang
  array<string> g_files
  mutex g_mu
  func worker() as int
      g_mu.lock()
      string s = g_files[0]     // <- 进程崩溃 EXIT=3，无任何输出
      g_mu.unlock()
      return 0
  end
  func main(...) as int
      g_files = ["a", "b"]      // 在 main 中重绑定（g = ...）
      thread t = thread(worker)
      t.wait()
      return 0
  end
  ```
- 现象：模块级声明的全局容器（array/dict），若在 `main` 中以 `g = [...]` / `g = func()` 形式**重新绑定**，则后续工作线程（`thread`）读/写该全局会**硬崩溃**（进程直接退出码 3，无异常、无输出）。同时影响 `array<string>`、`array<int>`、`dict<int>` 等。
- 边界：
  - 全局**标量**（int/string/bool）在 main 中重绑定后线程访问**正常**。
  - 全局容器若在**模块级声明处初始化**（`array<string> g = []`）且只做**就地修改**（push / clear / 按索引赋值）、从不重绑定，则线程访问**完全正常**（读/push 均验证通过）。
  - 即：崩点在「main 中对全局容器名的重赋值」，而非「线程访问容器」本身。
- 绕过（xtest 已采用）：所有需要跨线程共享的全局容器都在**声明处初始化**，且全程只 `clear()` + `push()`，绝不写 `g = ...`。
- 严重度：高（致命且无诊断；任何「main 收集任务 → 线程消费」的并发程序都会踩）
- 状态：未修复

### BUG-XT4: 多个 codegen/前端小限制（合集）
- 工具：xtest（开发期集中踩到，逐条绕过）
- 1. **`path.join` 最大 4 实参**：`path.join(a, b, c, d, e)` → `[sem] 实参数量和重载签名不匹配: expected 4, actual 5`。重载仅到 4 参。绕过：嵌套 `path.join(path.join(a,b,c), d)`。
- 2. **形参名不能用 C 标准库保留标识符**：`func f(string stdout)` → 生成的 C 形参 `stdout` 与 `<stdio.h>` 宏冲突，tcc 报 `')' expected (got '&')`。同样适用 `stderr`/`stdin`/`FILE`/`errno` 等。绕过：改用非保留名（如 `outText`）。注意：`r.stdout`（结构体字段访问）不受影响。
- 3. **`throw` 只接受 string**：`throw SomeClass(...)` → `[sem] throw 表达式必须是 string`。绕过：抛字符串；需要区分异常类型时用字符串前缀编码（如 `"[XTEST_SKIP]" + reason`）。
- 4. **动态 var 比较只能直接放进 `if` 条件**：`if not (a == b) then` 和 `bool eq = a == var`（a/b 为 var）→ `[checked] dynamic var comparison requires statement-level lowering`。绕过：用「相等则提前 return、否则落到失败」的早返回写法，或嵌套 `if`（`if a>=low then if a<=high then ... end end`）替代 `not`/`and` 组合。注意：`if a == b then`（直接作条件）合法。
- 严重度：低（均有清晰绕过；其中 #4 与 BUG-RT4 同族「statement-level lowering」限制）

## 设计限制（不予修复）

| 限制 | 说明 |
|---|---|
| `~=` 是近似相等 | 不是"不等于"；不等判断用 `!=` |
| 无 stdin API | 交互式工具用命令行参数驱动 |
| native-backed class 不可按值传参 | 带 destroy 的不可复制资源（设计决策） |
| json pretty 用 TAB 缩进 | XRT 底层未提供缩进配置 |
| list 无 append | int64 稀疏存储，用 `li[index] = value` |
| `#` 不是注释 | `//` 才是注释；`#` 开启时间字面量 |
| `auto` 无法从 `pop()` 推断 | 用显式类型 `int p = a.pop()` |
