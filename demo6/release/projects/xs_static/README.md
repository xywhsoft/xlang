# xs_static

`xs_static` 是基于 xlang demo6 当前能力实现的静态编译版 xs 原型。它不做运行时脚本加载、热更新、动态编译，也不复刻完整 xserver；目标是先验证“用 xlang + XRT xweb 写 xs 的编译型主路径”是否顺手。

## 当前能力

- 读取 `projects/xs_static/xs_static.json`。
- 自动准备静态目录 `wwwroot`。
- 注册静态文件服务。
- 注册基础 API：
  - `GET /health`
  - `GET /api/info`
  - `GET /api/time`
  - `POST /api/echo`
  - `ANY /api/method`
- 支持 `--port`、`--root`、`--duration`、`--quiet`、`--init`。

当前 demo6 的 `http.server` 暂未暴露完整 request/response 对象，所以本项目暂不实现状态码、响应头、cookie、中间件、虚拟主机、WebSocket、反向代理和动态脚本加载。

## 运行

在 `demo6` 根目录运行：

```bat
release\xl.exe run projects\xs_static\xs_static.xl -- --port 8088
```

自动停止的 smoke 运行：

```bat
release\xl.exe run projects\xs_static\xs_static.xl -- --port 8088 --duration 3000
```

生成默认配置和默认静态站点：

```bat
release\xl.exe run projects\xs_static\xs_static.xl -- --init
```

## 构建

```bat
projects\xs_static\build.bat
```

构建产物：

```text
projects\xs_static\release\xs_static.exe
```

运行构建产物：

```bat
projects\xs_static\release\xs_static.exe --port 8088
```

## 冒烟测试

```bat
projects\xs_static\test_smoke.bat
```

冒烟测试会启动一个短生命周期服务，验证 `/health`、`/api/info`、`/api/echo` 和静态文件访问。
