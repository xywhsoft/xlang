# opencv xlang package

This package is a Python-style OpenCV wrapper for xlang.

## Current API

- `cv2.available()`
- `cv2.backend()`
- `cv2.last_error()`
- `cv2.imread(path)`
- `cv2.imread(path, flags)`
- `cv2.imwrite(path, image)`
- `cv2.resize(image, width, height)`
- `cv2.crop(image, x, y, width, height)`
- `cv2.capture_rect(x, y, width, height)`
- `cv2.capture_window(hwnd)`
- `cv2.cvtColor(image, code)`
- `cv2.copyMakeBorder(...)`
- `cv2.normalize(image, scale, mean0, mean1, mean2, std0, std1, std2)`
- `cv2.toCHWTensorJson(image, scale, mean0, mean1, mean2, std0, std1, std2)`

`toCHWTensorJson` returns ONNX Runtime-compatible JSON:

```json
{"dtype":"float32","shape":[1,3,32,320],"data":[...]}
```

## Build note

The bundled OpenCV 4.13.0 Windows binaries use the MSVC ABI. Build `native/opencv_xlang.cpp` with `native/build_msvc.bat` after Visual Studio Build Tools are available.

For environments that only have MinGW, `native/build_gcc.bat` builds a limited
stb-image backend. That backend implements the wrapper functions needed by the
current OCR preprocessing and UI automation screenshot paths, but it is not a
full OpenCV replacement. On Windows, `capture_rect` and `capture_window` use
GDI; HWND-backed capture is preferred for automation targets.
