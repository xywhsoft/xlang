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
- 现象：`array<T>` / `set<T>` / `list<T>` / `array<Local>` 等**静态（typed）容器**作为函数参数时被**整体按值拷贝**，函数内的 `push`/`add`/`remove`/`[i]=` 不会影响调用方的容器。
- 重要更正（logagg 验证）：原条目称「`array<var>` / `var` 容器同样是值拷贝」——**这是错误的**。实测 `var` dict 与 `var` array 作为函数参数是**按引用传递**（写回传播、被调方也能读到调用方既有状态）：
  ```xlang
  func mutate(var d)
      d["fromFunc"] = 99
      d["seeded"] = int(d["seeded"]) + 5      // 读到调用方写入的 10
  end
  var d = {}
  d["seeded"] = 10
  mutate(d)
  println(int(d["fromFunc"]))   // 99（写回可见）
  println(int(d["seeded"]))     // 15（读+写都生效）
  ```
  即：需要「helper 函数就地修改容器并回传」时，**用 `var` 容器参数是安全且可用的**；只有 typed 容器（`array<T>`/`set<T>`/`list<T>`）才被值拷贝。
- 绕过：typed 容器用全局变量（函数直接访问全局，xlint 已采用），或让函数 `return` 新容器由调用方赋值；或改用 `var` 容器作参数（logagg 早期版本误把 list<T> 当输出参数导致写回丢失，改为内联或 var 容器后正常）。
- 严重度：中（设计语义，但容易踩；注意 typed 与 var 行为不同）

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

### BUG-LP1: var dict 在递归计算中分配会触发**二次方** GC 劣化；var 数组不受影响
- 工具：lisp（Lisp 解释器）
- 复现：
  ```xlang
  // 在递归调用中每次分配一个 dict（模拟解释器的环境帧）
  func fakefib(int n, var env) as int
      if n < 2 then return n end
      var d = {}
      d["n"] = n
      d["%up"] = env
      return fakefib(n - 1, d) + fakefib(n - 2, d)
  end
  ```
- 现象（实测，单进程内）：
  - dict 版：`fakefib(15)` 9.5ms（4.8µs/调用）→ `fakefib(20)` 1081ms（49µs/调用）→ `fakefib(22)` **11200ms（196µs/调用）**。每调用成本随累计分配数**线性增长**，总耗时随调用数**二次方增长**。
  - 对照：**同样模式但用 var 数组**（`var d = [n, env]`）分配：`fib(25)`（242785 次调用）仅 **111ms（0.46µs/调用）**，完全线性，无劣化。
  - 对照：纯循环里分配 70 万个 dict（无递归引用链）只需 1.77s，**不劣化**。劣化仅在「递归调用 + 每帧 dict 形成引用图」时出现。
  - 推测根因：XRT 的 dict GC（疑似 mark-sweep，非精确 refcount）在递归释放带引用图的 dict 帧时，每轮扫描成本随堆中未回收对象数增长，导致 O(n²)。var 数组走不同 GC 路径，不受影响。
- 影响 lisp 解释器：最初用 var dict 表示词法环境（每函数调用 1 个 dict），fib(25)（3M 次 eval 调用、24 万次 dict 分配）预估 >7 分钟不可用；实际 fib(22) 已需 25s。
- 绕过（lisp 已采用）：**用 var 数组代替 dict 表示环境**——`env = [parent, key1, val1, key2, val2, ...]`，线性扫描查找（后向，保证 shadow 正确）。改用数组后 fib(25) 降至 **~8s**，fib(22) 从 25s 降至 2s。
- 通用建议：在**递归 / 高频调用路径上避免创建 var dict**；需要键值结构时用扁平 var 数组（key/val 交替）+ 线性查找替代。仅在低频路径（如一次性配置、JSON 装载）使用 dict。
- 严重度：**高**（静默、二次方劣化；任何「递归中分配 dict」的程序——解释器、AST 遍历、回溯搜索——都会踩）

### BUG-CE1: `var + string` 字符串拼接产生乱码（静默数据损坏）
- 工具：csvengine
- 复现（最小）：
  ```xlang
  var g = "a."
  string key = "id"
  string k1 = g + key          // → "D"  （len=1，乱码）
  string k2 = string(g) + key  // → "a.id"（正确）
  string g2 = "a."
  string k3 = g2 + key         // → "a.id"（正确）
  ```
- 现象：用 `+` 把一个 **`var` 类型**（即使其运行时值是 string）与一个 `string` 拼接时，结果不是字符串拼接，而是一个**长度异常的乱码串**（本例 `"a." + "id"` → `"D"`）。每次运行乱码内容可能不同（疑似读取了未初始化/错误的内存并按某种编码截断）。
- 边界：
  - `string + string` 正确。
  - `string(var) + string` 正确（显式构造强制 owned string）。
  - 仅当左操作数（或任一操作数）的**静态类型是 `var`** 时触发；静态 `string` 不受影响。
  - 编译期无任何诊断；运行期不抛异常；结果静默错误。
- 影响 csvengine：JOIN 时行 dict 的 key 由 `key = gAlias + key` 构造（gAlias 原声明为 `var`），导致所有「`alias.col`」键塌缩成同一个乱码键，JOIN 退化为笛卡尔积且取值全空。最初表现为「JOIN 输出全空 / 全部匹配」。
- 绕过（csvengine 已采用）：把会被拼接到 string 的变量声明为静态 `string`（`string gAlias = ""`），或在拼接处 `string(v) + ...` 强制 owned string。
- 推测根因：`+` 在左操作数为 `var` 时未按 string 派发，而是走到数值/原始字节相加路径，把 string 内部指针/长度当成数值运算，产生悬空截断串。
- 严重度：**高**（静默数据损坏；`var` 是动态类型主推用法，拼接极常见；无警告）
- 状态：未修复

### BUG-JP2: var 数组负数下标返回 null（不支持 Python/JS 风格的 `arr[-1]`）
- 工具：jsonpath
- 复现：
  ```xlang
  var arr = [10, 20, 30]
  var x = arr[-1]        // → null（期望 30）
  var y = arr[3]         // → null（越界，与负数同表现）
  ```
- 现象：对 `var` 数组使用负数下标 `arr[-1]` 不会取倒数第一个元素，而是返回 `null`（与正数越界 `arr[len]` 表现一致）。习惯 Python `list[-1]` / JS `arr.at(-1)` 的用户会得到静默的空值。
- 边界：typed 容器（`array<T>`）负数下标行为未在本工具中测试；`var` 数组确定受影响。
- 绕过（jsonpath 已采用）：在取值前手工把负数下标归一化为非负——`if idx < 0 then real = len + idx end`，再判 `0 <= real < len`。
- 严重度：低（清晰绕过；但「静默返回 null」对动态语言用户反直觉）
- 状态：未修复

### BUG-AC1: `channel<string>` 不支持——只允许 bool/整数/浮点/time/point
- 工具：actorsys
- 复现：`channel<string> ch = channel::<string>(64)` → `[sem] channel<T> 当前只支持 bool、整数、浮点、time、point 等可按值复制的元素类型`
- 现象：`channel<T>` 的泛型参数被限制为值类型，**string 被排除**，尽管 xlang 的 string 本身是不可变值类型（与 int 同属值语义）。
- 影响：Actor 模型 / 消息队列中极常见的「字符串消息邮箱」模式不可用；只能用 `channel<int>` + 整数编码协议代替。
- 绕过（actorsys 已采用）：全部 Actor 邮箱使用 `channel<int>`，消息协议为整数编码（正数 = payload，MSG_STOP=-1，MSG_FAIL=-2，MSG_RESET=-3）。需要传字符串消息时编码为 int 或用外部全局 `array<string>` + channel 通知。
- 严重度：中（限制并发编程的表达力，但有清晰的整数编码绕过）
- 状态：未修复

### BUG-RAS1: `set`/`get`/`let` 不能作为 class 成员方法名
- 工具：rasterizer
- 复现：
  ```xlang
  class Mat4
      array<float> m = array[]
      func set(int row, int col, float val)      // <- [sem] get/let/set cannot be used as class member names
          self.m[row*4+col] = val
      end
  end
  ```
- 现象：class 内任何名为 `set` / `get` / `let` 的成员方法（即使不是属性语法）在编译期被拒。`set` 是极常见的 setter 命名（矩阵/容器/缓冲写入），易踩。
- 边界：字段名 `set` 同样被拒；`get`/`let` 是属性关键字，连同普通方法名一起被保留。
- 绕过：改用其它方法名（如 `setCell` / `put` / `write`），或直接 `self.m[i] = v` 内联。
- 严重度：低（清晰错误信息，一行改名绕过；但 `set` 命名直觉性强）
- 状态：未修复

### BUG-FTP1: accept 之后服务端的「首次 send」静默丢失
- 工具：ftpserver
- 复现：
  ```xlang
  point lp = tcp.listen(0, "127.0.0.1", 4)
  TcpServer srv = TcpServer(lp)
  point cp = tcp.connect("127.0.0.1", srv.port(), 3000)
  TcpStream client = TcpStream(cp)
  point ap = srv.accept(3000)
  TcpStream peer = TcpStream(ap)
  bool ok = peer.send("220 hi\r\n")   // -> false，对端 client 收不到任何字节
  peer.drain(2000)                     // 仍然收不到
  ```
- 现象：`TcpServer.accept()` 返回后，服务端 socket 的**第一次** `send` / `sendBytes` 返回 `false` 且字节从未到达对端；`drain` 也救不回来。对照：connect 侧（客户端）首次 `send` 正常，只有「刚 accept 出来的服务端 socket 首发」触发。
- 触发面：所有「服务端先开口」的行协议 —— FTP `220 Welcome`、SMTP `220 greeting`、POP3 `+OK` 等。后续 send（首发被重试或时间推移后）正常。
- 绕过（ftpserver 已采用）：发送时重试循环 `while not ok do ok = s.send(text); time.sleep(5) end`（~5ms 间隔、约 30 次，~80–100ms 内落地）。见 `sendRetry` / `sendBytesRetry`。
- 严重度：高（静默丢数据；任何 server-speaks-first 协议首发踩坑，无明显警告）
- 状态：未修复

### BUG-FTP2: `TcpStream.close()` 是 abortive 关闭，丢弃未 ACK 的发送缓冲
- 工具：ftpserver
- 复现：
  ```xlang
  // connector 发送后立即 close；accepted 侧并发 recvBytes 收到 0 字节
  client.sendBytes(payload.data(), c_size(payload.len()))
  client.drain(3000)
  client.close()                       // -> 对端 peer 收到 0 字节（数据被 RST 丢弃）
  ```
- 现象：
  - `close()` 对仍有未确认发送缓冲的 socket 做**abortive 关闭**（发 RST / 直接丢缓冲），而非 graceful（先 flush 再 FIN）。
  - `drain(timeout)` 只等**本地**发送缓冲排空（交给 OS），**不等对端 ACK / 收到**；所以 `send + drain + close` 仍可能在 close 时丢数据。
  - 连接器（connector）侧 close 和 accepted 侧 close 都受影响；并发读取的对端尤其容易拿到 0 字节。
- 触发面：所有用「close 作为 EOF 信号」的传输模型 —— FTP STOR/RETR/LIST、HTTP chunked、任何「发完即关」协议。
- 绕过（ftpserver 已采用）：`drain` 之后、`close` 之前 `time.sleep(DATA_CLOSE_DELAY)`（~300ms，小文件足够），让内核先把字节投递到对端 recv 缓冲。`recvBytes(idle_timeout)` 在对端存活但静默时也会按超时返回 len 0（已验证），可作为不依赖 close 的 EOF 判定。无 `shutdown(SHUT_WR)` 之类的半关闭 API 可用。
- 严重度：高（静默数据损坏；close-for-EOF 是 TCP 标准模式）
- 状态：未修复

### BUG-FTP3: native-backed class 不可「二次包装」同一 point（堆破坏）
- 工具：ftpserver
- 复现：
  ```xlang
  point ap = srv.accept(3000)
  if true then
      TcpStream peer = TcpStream(ap)
      peer.send("first")
  end                                    // peer 离开作用域 -> destroy
  if true then
      TcpStream peer2 = TcpStream(ap)     // 同一 point 再包一次
      peer2.send("second")
  end                                    // -> 0xC0000374 STATUS_HEAP_CORRUPTION
  ```
- 现象：用同一个 `point` 句柄**第二次** `TcpStream(p)`（或 `TcpServer(p)`）构造，运行期触发堆破坏（`0xC0000374`），首次 send 直接返回 false。
- 正向确认（同一 session 另一发现）：`TcpStream` 作为函数参数按值传入是**安全**的（句柄在调用后仍可用，后续 send 正常）—— 这与 BUG-HD1「作函数参数传入 OK」一致，修正了设计限制表里「native-backed class 不可按值传参」的过宽表述（真正禁止的是 return 已有具名值，不是 param 传入）。所以 ftpserver 的 `sendLine(TcpStream s, ...)` / `recvLine(...)` 这类 helper 合法。
- 绕过（ftpserver 已采用）：每个 accept/connect 返回的 point **只包装一次**，在该句柄的整个生命周期内只构造一个 TcpStream；PASV 复用单一 session listener（`dataListener`），不在每次 PASV 重建。
- 严重度：高（致命崩溃；复用句柄包装是自然的「按需重建 wrapper」直觉）
- 状态：未修复

### BUG-DN1: `for _, _ in dict` 触发重复声明错误
- 工具：dnsrv
- 复现：`for _, _ in g_zone do ... end` → `[sem] 同一作用域内重复声明`
- 现象：在 `for k, v in dict` 解构中，key 和 value 都用 `_` 丢弃时，编译器报同一作用域内重复声明 `_`
- 期望：`_` 作为通配符应允许多次出现（Python `for _, _ in d.items()` 合法）
- 绕过：至少给 key 或 value 一个具名变量（如 `for k, _ in dict`）
- 严重度：低
- 状态：未修复

### BUG-CL1: 多值返回丢弃 `array<int>`（typed 容器返回空）
- 工具：cryptolab
- 复现：
  ```xlang
  func makeArr(int n) as (array<int>, int)
      array<int> a = array[n]
      int i = 1
      while i < n do a.push(i); i += 1 end
      return a, n
  end
  array<int> r; int v
  r, v = makeArr(5)
  println(r.length)   // 0（期望 5）
  println(v)          // 5（正确）
  ```
- 现象：函数返回 `(array<int>, T)` 或 `(array<int>, array<int>)` 时，`array<int>` 分量在调用方
  拿到的是**空数组**（length=0），而标量分量（int/bool 等）返回正确。仅影响 typed 容器
  （`array<T>`），不影响标量多值返回。
- 边界：
  - 单值返回 `array<int>` 完全正常。
  - 标量 + 标量的多值返回正常（如 `return a, b` 其中 a/b 是 int）。
  - `var` 容器的多值返回未测试（可能也受影响）。
- 绕过（cryptolab 已采用）：将 typed 容器作为**唯一返回值**，其余返回值通过**全局变量**传递
  （如 `g_nr` / `g_tag` / `g_auth`），或拆分为两次调用。
- 严重度：**高**（静默数据丢失；typed 容器多值返回是常见模式；无编译期警告）
- 状态：未修复

### BUG-CL2: 顶层函数名与 C 标准库名冲突（tcc 链接报错）
- 工具：cryptolab
- 复现：`func xtime(int b) as int ... end` →
  `tcc: __xlvfs__/module.c:643: error: incompatible types for redefinition of 'xtime'`
- 现象：xlang 将顶层函数名直接作为 C 函数名生成。若函数名与 C 标准库已有的函数/宏重名
  （如 `xtime`、`stime`、`drand48` 等），tcc 在链接阶段报「类型不兼容的重定义」。
- 边界：与 BUG-XT4 #2（形参名不能用 C 保留标识符）同族，但此处影响的是**顶层函数名**而非
  形参名。xlang 目前不检测函数名与 C 标准库的冲突。
- 绕过：改用非保留名（如将 AES 的 `xtime` 改为 `mul2`）。
- 严重度：低（清晰错误信息，一行改名绕过；但函数名直觉性可能受影响）
- 状态：未修复

### BUG-XR1: 源文件必须以换行结尾，否则最后一条语句解析失败
- 工具：xrepl
- 复现：
  ```xlang
  // 文件内容恰好是 `println(1 + 2)`（14 字节，无尾换行）
  file.writeText("a.xl", "println(1 + 2)")
  // xl run a.xl
  // → a.xl:1:14: [parse] 语法错误（定位在行尾 `)`）
  ```
- 现象：源文件最后一条语句后若**没有换行符**，词法/解析阶段无法刷新最后一个 token，
  报 `[parse] 语法错误` 并把光标定位在行尾（如 `)` ）。只有当文件以 `\n`（或 `\r\n`）
  结尾时才正常解析。
- 边界：用 PowerShell `Set-Content` / 编辑器保存的文件通常自动追加尾换行，故大多数
  手写源码不受影响；但用 `file.writeText(path, code)` 程序化生成且不加 `\n` 时必踩。
- 绕过（xrepl 已采用）：写文件时统一 `file.writeText(path, code + "\n")`。
- 严重度：中（程序化生成源码的场景常见；错误信息不直观——看起来像语法错实则是缺尾换行）
- 状态：未修复

### BUG-XR2: `print()` / `println()` 对含 CRLF 的字符串二次翻译，产生多余空行
- 工具：xrepl
- 复现：
  ```xlang
  // 子进程 stdout 在 Windows 上是 CRLF
  ProcessResult r = process.run("xl.exe run sub.xl", 10000)
  print(r.stdout)          // r.stdout = "3\r\n"
  // 实际输出到管道/文件的是 "3\r\n\r\n"（多一个换行），表现为多一个空行
  ```
- 现象：xlang 的 stdout 走 C 运行时**文本模式**，会把字符串里的 `\n` 翻译成 `\r\n`。
  当被打印的字符串本身已经包含 `\r\n`（典型来源：`process.run` 捕获的子进程 stdout、
  或 `file.readText` 读入的 CRLF 文件），`\n` 被再次翻译为 `\r\n`，得到 `\r\r\n`，
  在终端/重定向文件中表现为**多出一个空行**。
- 边界：
  - 源码字面量 `print("3\n")`（只有 LF）→ 输出 `3\r\n`（正常，一次翻译）。
  - `print(capturedStdout)`（含 CRLF）→ 输出多一个空行（二次翻译）。
  - 不崩溃，仅静默多出空白，易误判为逻辑错误。
- 绕过（xrepl 已采用）：打印前 `string.replace(s, "\r", "")` 去掉 CR，统一为 LF，
  再由文本模式统一翻译为 CRLF。
- 严重度：低（Windows 专属；有清晰绕过；但「回显子进程输出」是常见模式，易踩）
- 状态：未修复

### BUG-SM1: `time.timezoneOffset()` 返回秒，文档却标注「分钟」
- 工具：smtpcli
- 复现：
  ```xlang
  int tz = time.timezoneOffset()    // 系统时区 UTC+8
  println(string(tz))               // -> 28800（8*3600）
  ```
- 现象：`标准库设计.md` / 功能清单均标注 `time.timezoneOffset()` 返回「当前本地时区偏移**分钟数**」，
  实测返回**秒**（UTC+8 → 28800；若是分钟应为 480）。直接拿来当分钟算会得到荒谬的偏移
  （如 RFC 2822 日期头部写成 `+48000` 而非 `+0800`）。
- 边界：返回值本身是「本地时间 − UTC」的有符号秒数；符号正确（东区正、西区负）；
  仅文档/单位描述错误，非数值错误。
- 绕过（smtpcli 已采用）：按秒使用——`tzH = tzAbs / 3600`，`tzM = (tzAbs - tzH*3600) / 60`。
- 严重度：中（静默错误；时区相关代码极常见；文档误导，无编译期警告）
- 状态：未修复



| 限制 | 说明 |
|---|---|
| `~=` 是近似相等 | 不是"不等于"；不等判断用 `!=` |
| 无 stdin API | 交互式工具用命令行参数驱动 |
| native-backed class 不可按值传参 | 带 destroy 的不可复制资源（设计决策） |
| json pretty 用 TAB 缩进 | XRT 底层未提供缩进配置 |
| list 无 append | int64 稀疏存储，用 `li[index] = value` |
| `#` 不是注释 | `//` 才是注释；`#` 开启时间字面量 |
| `auto` 无法从 `pop()` 推断 | 用显式类型 `int p = a.pop()` |
| 字符串无 `.length` 属性 | 用 `string.len(s)`；`.length` 仅用于 array/容器变量 |
| 字面量不能作 `.` 接收者 | `"x".method()` / `[1,2,3].length` 非法；先赋值给变量再访问 |
