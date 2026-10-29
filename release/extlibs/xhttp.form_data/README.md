# xhttp.form_data

`xhttp.form_data` 提供拥有型 `multipart/form-data` 容器，并复用
`xhttp.body` 与 `xhttp.multipart` 的稳定协议契约。

常见字段使用 `FormData.append()`、`set()`、`get()` 和迭代器；需要避免正文
复制时直接传入 `Body`。`encode(boundary)` 返回组合式 `Body`，不会先把全部
Part 拼接为一个连续缓冲；`FormData.parse()` 是完整正文的拥有型便利路径，
增量接收仍使用 `xhttp.multipart.MultipartReader`。

```xlang
import xhttp.form_data
from xhttp.form_data import FormData

FormData form = FormData()
form.append("message", "hello")
form.append("payload", Body.fromText("streamable"))
var encoded = form.encode()
```
