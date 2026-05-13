# rapidocr dependency example

This directory tests a dependency chain:

```xlang
import rapidocr
```

`rapidocr` depends on:

- `opencv`
- `onnxruntime`

The public API is intentionally close to Python:

```xlang
import opencv as cv2
import onnxruntime as ort
import rapidocr

auto img = cv2.imread("sample.png")
auto sess = ort.InferenceSession("model.onnx")
auto engine = rapidocr.RapidOCR()
auto result = engine("sample.png")
```

Native shim DLL lookup:

- `OPENCV_XLANG_DLL` or `libs\\opencv\\bin\\opencv_xlang.dll`
- `ONNXRUNTIME_XLANG_DLL` or `libs\\onnxruntime\\bin\\onnxruntime_xlang.dll`
- `RAPIDOCR_XLANG_DLL` or `libs\\rapidocr\\bin\\rapidocr_xlang.dll`
