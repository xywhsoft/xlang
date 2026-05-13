# rapidocr xlang package

This package is intended to expose a Python-style `rapidocr.RapidOCR()` API while keeping native dependencies explicit:

- `opencv`
- `onnxruntime`

## Model Layout

Default model directory:

```text
models/
  ch_PP-OCRv4_det_infer.onnx
  ch_ppocr_mobile_v2.0_cls_infer.onnx
  ch_PP-OCRv4_rec_infer.onnx
  ppocr_keys_v1.txt
```

The wrapper exposes this contract through:

- `rapidocr.default_model_dir()`
- `rapidocr.default_config_json()`
- `rapidocr.model_layout_json()`

Default config JSON:

```json
{
  "model_dir": "models",
  "det_model_path": "models\\ch_PP-OCRv4_det_infer.onnx",
  "cls_model_path": "models\\ch_ppocr_mobile_v2.0_cls_infer.onnx",
  "rec_model_path": "models\\ch_PP-OCRv4_rec_infer.onnx",
  "keys_path": "models\\ppocr_keys_v1.txt",
  "use_det": true,
  "use_cls": true,
  "use_rec": true,
  "return_word_box": false,
  "det_limit_side_len": 960,
  "rec_img_h": 48,
  "rec_img_w": 320
}
```

Run `models/download_models.ps1` to fetch the default files from HuggingFace/PaddleOCR.

Model location policy:

- `demo5/release/libs/rapidocr/models` is the package-owned model directory.
- `demo5/release/examples/libs/rapidocr/models` mirrors it so examples can run as standalone release samples.
- Example programs may reference the mirrored example path; package code and future native pipeline work should treat package `models/` as the authoritative layout and keep the mirror in sync for demos.

When `rapidocr_xlang.dll` is not present, the xlang wrapper currently falls back to an xlang-level pipeline:

1. OpenCV wrapper loads and preprocesses the image.
2. ONNX Runtime runs the detector, classifier, and recognizer models through typed tensor inputs.
3. RapidOCR helpers postprocess detector boxes, decode classifier/CTC recognizer typed outputs, and assemble a RapidOCR-style JSON result.

This path is functional for smoke tests. The remaining OCR quality work is real text-crop handling, batching, rotation behavior, and drawn visualization.
