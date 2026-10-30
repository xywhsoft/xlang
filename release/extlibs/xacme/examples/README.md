# XACME 范例

`read_account.xl` 只读取已有的本地账户存储，不访问 CA、不修改 DNS，也不输出
私钥。它演示上游 C 函数直接声明为普通 string，返回分配自动进入语言生命周期。

从 demo6 执行，指定创建存储时使用的目录 URL：

```powershell
$env:XACME_STORE_ROOT = Join-Path $env:TEMP 'xlang-cert-store'
$env:XACME_DIRECTORY = 'https://acme-staging-v02.api.letsencrypt.org/directory'
release/xl.exe run release/extlibs/xacme/examples/read_account.xl
```

Linux 设置相同的环境变量后使用 `release/xl run`。需要支持 `@owned` 的新编译器
和对应运行库。错误路径由正常语言异常报告；脚本不创建需要删除的文件。

`tools/check_xacme.py` 使用本地模拟 CA 生成独立测试存储，并在 JIT/AOT 中运行
同一范例，无需云凭据。其他签发接口的真实环境范例继续使用包 README 的配置说明。
