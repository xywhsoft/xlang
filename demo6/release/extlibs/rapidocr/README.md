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

- `release/extlibs/rapidocr/models` is the release package model directory.
- Example programs should use `rapidocr.default_model_dir()` instead of hardcoding repository-relative paths.
- Package code treats package `models/` as the authoritative layout for demos and 0-config lookup.

When `rapidocr_xlang.dll` is not present, the xlang wrapper currently falls back to an xlang-level pipeline:

1. OpenCV wrapper loads and preprocesses the image.
2. ONNX Runtime runs the detector, classifier, and recognizer models through typed tensor inputs.
3. RapidOCR helpers postprocess detector boxes, decode classifier/CTC recognizer typed outputs, and assemble a RapidOCR-style JSON result.

Current xlang-pipeline coverage:

- Detector postprocess returns connected-component boxes with basic IoU suppression.
- Recognizer input uses detected text crops when boxes are available.
- Classifier output can trigger 180-degree correction before recognition.
- Visualization draws up to 16 detected boxes.
- Result assembly supports up to four recognized text entries in the current V1 path.

Remaining OCR quality work is broader batching, PP-OCR-style geometric unclip/box scoring, and accuracy tuning against real OCR fixtures.
