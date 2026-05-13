# xlang external libraries tracking spec

Status legend:

- [ ] Not started
- [X] Completed
- [~] In progress
- [!] Needs confirmation

## Goal

Build Python-style XLE extension libraries for:

- `onnxruntime`
- `opencv`
- `rapidocr`

The libraries should be usable from xlang examples, support native binary dependencies, and verify that xlang can copy required DLLs into the compiled program output directory.

## Current Snapshot

- [X] Package names use direct upstream-style names: `onnxruntime`, `opencv`, `rapidocr`.
- [X] Packages live under `demo5/release/libs/<name>`.
- [X] Example mirror packages live under `demo5/release/examples/libs/<name>`.
- [X] Example programs live under `demo5/release/examples/<name>`.
- [X] XLE dependency copying has been validated for `onnxruntime.dll`, `onnxruntime_providers_shared.dll`, and `onnxruntime_xlang.dll`.
- [X] `onnxruntime` has broad CPU inference coverage for the current local DLL set.
- [X] `opencv` package scaffold, Python-style wrapper, official OpenCV binary bundle, MSVC C++ backend source, and MinGW stb-image backend are available.
- [X] `rapidocr` package scaffold exists and the xlang-level OCR pipeline is functional; optional native `rapidocr_xlang.dll` backend remains a future compatibility path.

## ONNX Runtime

### Package and Binary Layout

- [X] Create `demo5/release/libs/onnxruntime/package.json`.
- [X] Copy official ONNX Runtime headers into `demo5/release/libs/onnxruntime/include`.
- [X] Copy `onnxruntime.dll`, `onnxruntime.lib`, and `onnxruntime_providers_shared.dll` into `demo5/release/libs/onnxruntime/bin`.
- [X] Build local native shim `onnxruntime_xlang.dll`.
- [X] Provide `native/build.bat` for rebuilding `onnxruntime_xlang.dll`.
- [X] Mirror package files into `demo5/release/examples/libs/onnxruntime`.

### Python-Style API Surface

- [X] `ort.InferenceSession(model_path)`.
- [X] `ort.InferenceSession(model_path, session_options)`.
- [X] `ort.SessionOptions()`.
- [X] `ort.RunOptions()`.
- [X] `sess.run(output_names_json, input_feed_json)`.
- [X] `sess.run(output_names_json, input_feed_json, run_options)`.
- [X] `sess.get_inputs()`.
- [X] `sess.get_outputs()`.
- [X] `sess.get_modelmeta()`.
- [X] `sess.get_providers()`.
- [X] `sess.io_binding()`.
- [X] `sess.run_with_iobinding(binding)`.
- [X] `sess.end_profiling()`.
- [X] `ort.get_available_providers()`.
- [X] `ort.get_providers()` alias.
- [X] `ort.get_version_string()`.
- [X] `ort.last_error()`.
- [X] `ort.get_last_error()` alias.

### Constants and Diagnostics

- [X] Graph optimization constants: `ORT_DISABLE_ALL`, `ORT_ENABLE_BASIC`, `ORT_ENABLE_EXTENDED`, `ORT_ENABLE_ALL`.
- [X] Execution mode constants: `ORT_SEQUENTIAL`, `ORT_PARALLEL`.
- [X] Logging constants: `ORT_LOGGING_LEVEL_VERBOSE`, `INFO`, `WARNING`, `ERROR`, `FATAL`.
- [X] Provider constant: `CPUExecutionProvider`.
- [X] Add `ok()`, `valid()`, and `error()` where useful on session/options/result/binding objects.

### Tensor Support

- [X] JSON tensor feed format for numeric tensors.
- [X] `float32` input/output.
- [X] `float64` input/output.
- [X] `int64`, `int32`, `int16`, `int8` input/output.
- [X] `uint64`, `uint32`, `uint16`, `uint8` input/output.
- [X] `bool` input/output.
- [X] `string` input/output.
- [X] Large tensor policy decision: JSON remains a smoke/debug path; binary/handle tensors are the target for real vision-model inference.
- [X] Optional binary tensor buffer API for large image/model inputs; float32 tensor handle path and OpenCV-to-ORT CHW buffer handoff are implemented for detector/classifier/recognizer inputs.
- [X] Optional typed Tensor object API instead of JSON-only feed; `TensorZerosF32` and `sess.run_tensor(...)` smoke passes.
- [X] Typed float output reader API for `RunResult`: shape JSON, count, and indexed float access.

### Metadata

- [X] Input metadata includes name, tensor type, dtype, and shape.
- [X] Output metadata includes name, tensor type, dtype, and shape.
- [X] Model metadata includes producer name, graph name, domain, description, and version.
- [X] Custom metadata map is included in model metadata JSON.
- [X] Add richer symbolic dimension reporting through `shape_symbols` in input/output metadata JSON.

### SessionOptions

- [X] `set_intra_op_num_threads`.
- [X] `set_inter_op_num_threads`.
- [X] `set_graph_optimization_level`.
- [X] `set_execution_mode`.
- [X] `enable_profiling` and `disable_profiling`.
- [X] `enable_cpu_mem_arena` and `disable_cpu_mem_arena`.
- [X] `enable_mem_pattern` and `disable_mem_pattern`.
- [X] `set_log_id`.
- [X] `set_log_verbosity_level`.
- [X] `set_log_severity_level`.
- [X] `append_execution_provider(provider)`.
- [X] `append_execution_provider(provider, options_json)`.
- [X] CPU provider is treated specially because ORT's generic append API does not accept `CPUExecutionProvider`.
- [X] Provider-specific convenience helpers are deferred; current local bundle exposes CPU provider only.

### RunOptions

- [X] `set_log_verbosity_level`.
- [X] `set_log_severity_level`.
- [X] `set_run_tag`.
- [X] `set_terminate`.
- [X] `unset_terminate`.
- [X] Run config entries are deferred; no local API-backed wrapper target is confirmed now.

### IOBinding

- [X] Create `IoBindingObject` via `sess.io_binding()`.
- [X] Bind CPU input from tensor JSON.
- [X] Bind CPU output by name.
- [X] Run session with binding.
- [X] Read bound output JSON.
- [X] Clear inputs and outputs.
- [X] Synchronize inputs and outputs.
- [X] GPU/device buffer binding is out of V1 scope; current verified package is CPU-only and has no non-CPU provider DLLs.
- [X] Add external memory/buffer binding after OpenCV image/tensor memory layout is settled; direct OpenCV-to-ORT detector/classifier/recognizer input handoff and typed float output reading are implemented for CPU float tensors.

### Examples and Tests

- [X] `onnxruntime_python_style.xl`.
- [X] `onnxruntime_options_and_providers.xl`.
- [X] `onnxruntime_typed_tensors.xl`.
- [X] `onnxruntime_run_options_and_metadata.xl`.
- [X] `onnxruntime_string_and_custom_metadata.xl`.
- [X] `onnxruntime_profiling_and_provider_options.xl`.
- [X] `onnxruntime_iobinding_cpu.xl`.
- [X] `onnxruntime_provider_options_json.xl`.
- [X] `onnxruntime_constants_and_diagnostics.xl`.
- [X] `onnxruntime_downloaded_squeezenet.xl`.
- [X] `onnxruntime_tensor_object_smoke.xl`.
- [X] `onnxruntime_opencv_tensor_bridge.xl`.
- [X] `onnxruntime_typed_output_reader.xl`.
- [X] `onnxruntime_symbolic_dims_smoke.xl`.
- [X] Local generated identity models for float32, float64, int64, bool, and string.
- [X] Download script for SqueezeNet model.
- [X] Standalone exe validation confirms dependency DLL copying.

### Remaining ONNX Runtime Work

- [X] Decide whether JSON tensor feed is sufficient for RapidOCR preprocessing outputs. Decision: not sufficient for real OCR; keep only for smoke/debug.
- [X] Add large binary tensor feed API if RapidOCR image inference requires it; OpenCV image -> CHW float32 tensor object path is implemented for detector/classifier/recognizer inputs.
- [X] Add optional typed tensor object API only if it simplifies RapidOCR/OpenCV handoff.
- [X] Provider-specific tests are out of V1 scope because no non-CPU provider DLLs are available locally.
- [X] Document supported JSON tensor schema in package README.

## OpenCV

### Current State

- [X] Create `demo5/release/libs/opencv/package.json`.
- [X] Create public C ABI header `include/opencv_xlang_c_api.h`.
- [X] Create xlang wrapper `src/opencv.xl`.
- [X] Create dynamic shim loader `src/opencv_artifact.c`.
- [X] Mirror package into `demo5/release/examples/libs/opencv`.
- [X] Create initial example `demo5/release/examples/opencv/opencv_python_style.xl`.
- [X] API names are Python-like: `import opencv as cv2`, `cv2.imread`, `cv2.imwrite`, `cv2.resize`, `cv2.cvtColor`, `cv2.copyMakeBorder`, image `save`, `empty`, `ok`, `valid`, `error`, `width`, `height`, `channels`.
- [X] Functional `opencv_xlang.dll` backend is built with MinGW using stb-image for the wrapper's current API surface.
- [X] Official OpenCV C++ `opencv_xlang.dll` is deferred; official OpenCV Windows binaries are MSVC ABI and this machine currently has only MinGW GCC.

### Required Implementation

- [X] Build or locate OpenCV DLLs and import libraries for Windows.
- [X] Download OpenCV 4.13.0 official Windows package into `dev/extlibs/opencv/downloads`.
- [X] Extract OpenCV 4.13.0 into `dev/extlibs/opencv/work/opencv`.
- [X] Copy `opencv_world4130.dll` into `demo5/release/libs/opencv/bin`.
- [X] Copy `opencv_world4130.lib` into `demo5/release/libs/opencv/lib`.
- [X] Copy OpenCV headers into `demo5/release/libs/opencv/include/opencv2`.
- [X] Mirror OpenCV binaries and headers into `demo5/release/examples/libs/opencv`.
- [X] Create native backend source `native/opencv_xlang.cpp` using `cv::imread`, `cv::imwrite`, `cv::resize`, `cv::cvtColor`, and `cv::copyMakeBorder`.
- [X] Add MSVC build entry `native/build_msvc.bat`.
- [X] Add MinGW build entry `native/build_gcc.bat` for limited stb-image backend.
- [X] Build functional `opencv_xlang.dll` with MinGW stb-image backend.
- [X] Build official OpenCV C++ `opencv_xlang.dll` is deferred because `cl.exe` / Visual Studio Build Tools are not available on this machine.
- [X] Implement image load: `imread(path, flags)`.
- [X] Implement image save: `imwrite(path, image)`.
- [X] Implement resize.
- [X] Implement crop helper for OCR text-region preprocessing.
- [X] Implement rotate180 helper for OCR angle-classifier correction.
- [X] Implement color conversion for BGR/RGB/GRAY where needed by OCR preprocessing.
- [X] Implement padding/border copy.
- [X] Implement rectangle drawing helper for OCR visualization.
- [X] Implement normalization support in backend source and xlang wrapper.
- [X] Implement HWC to CHW float tensor JSON helper in backend source and xlang wrapper.
- [X] Implement image object lifecycle without leaks; wrapper calls native release and 50-call OpenCV lifecycle smoke passes.
- [X] Verify DLL dependency copy for OpenCV runtime DLLs; standalone OpenCV example build copied `opencv_world4130.dll` into the exe output directory.
- [X] Verify DLL dependency copy for `opencv_xlang.dll`.

### Python-Style API Candidates

- [X] Keep `import opencv as cv2`.
- [X] `cv2.imread(path, flags = cv2.IMREAD_COLOR)`.
- [X] `cv2.imwrite(path, image)`.
- [X] `cv2.resize(image, width, height)`.
- [X] `cv2.cvtColor(image, code)`.
- [X] `cv2.copyMakeBorder(...)`.
- [X] `cv2.normalize(image, scale, mean0, mean1, mean2, std0, std1, std2)`.
- [X] `cv2.toCHWTensorJson(image, scale, mean0, mean1, mean2, std0, std1, std2)`.
- [X] Useful constants: `IMREAD_COLOR`, `IMREAD_GRAYSCALE`, `IMREAD_UNCHANGED`, `COLOR_BGR2RGB`, `COLOR_RGB2BGR`, `COLOR_BGR2GRAY`, `COLOR_GRAY2BGR`, border constants.
- [X] Avoid over-wrapping OpenCV until RapidOCR's actual preprocessing needs are fixed; current wrapper covers the OCR-required V1 surface.

### Examples and Tests

- [X] Initial OpenCV Python-style smoke example exists.
- [X] Add sample image fixture.
- [X] Add load/resize/save smoke test.
- [X] Add color conversion smoke test.
- [X] Add constants and diagnostics smoke test.
- [X] Add preprocessing-oriented test for resize + padding + normalize + HWC to CHW.
- [X] Add rectangle drawing smoke test.
- [X] Add crop smoke test.
- [X] Add rotate180 smoke test.
- [X] Add repeated-call image lifecycle smoke test.
- [X] Add standalone exe dependency-copy test for `opencv_world4130.dll`.
- [X] Add standalone exe dependency-copy test for `opencv_xlang.dll`.

## RapidOCR

### Current State

- [X] Create `demo5/release/libs/rapidocr/package.json`.
- [X] Create public C ABI header `include/rapidocr_xlang_c_api.h`.
- [X] Create xlang wrapper `src/rapidocr.xl`.
- [X] Create dynamic shim loader `src/rapidocr_artifact.c`.
- [X] Depend on `opencv` and `onnxruntime`.
- [X] Mirror package into `demo5/release/examples/libs/rapidocr`.
- [X] Create initial dependency smoke example.
- [X] Public wrapper uses Python-like entry point `rapidocr.RapidOCR()`, call syntax, model-path constructor, and option setters.
- [X] Current backend uses a dynamic native shim when available and a functional xlang-level OpenCV + ONNX Runtime OCR pipeline when native RapidOCR DLL is absent.

### Required Implementation

- [X] Decide backend strategy: implement a new native pipeline around current ONNX models, using OpenCV-wrapper preprocessing and ONNX Runtime inference.
- [X] Existing RapidOcrOnnx concerns are addressed by using a new xlang-level OpenCV + ONNX Runtime pipeline instead of depending on the old wrapper.
- [X] Define model file layout and download/acquire strategy for detection/classification/recognition models.
- [X] Add `models/download_models.ps1` for default det/cls/rec/key files.
- [X] Download default PP-OCRv4 det/rec ONNX models, PP-OCR cls ONNX model, and `ppocr_keys_v1.txt`.
- [X] Implement image preprocessing using OpenCV extension for the xlang-level RapidOCR pipeline.
- [X] Implement detector inference through onnxruntime extension; PP-OCRv4 det model smoke inference passes on a small CHW float tensor.
- [X] Implement text box postprocessing; native helper scans detector probability JSON and returns scaled connected-component box JSON with basic IoU suppression and V1 geometric unclip expansion.
- [X] Implement optional angle classifier; cls model typed inference, basic 0/180 decode, RapidOCR engine fallback integration, and 180-degree image correction are added.
- [X] Implement recognizer inference V1; rec ONNX smoke runs through OpenCV-to-ORT typed input, CTC decode reads ONNX Runtime float output directly, indexed crop helpers and V1 four-result assembly are wired.
- [X] Implement dictionary/charset loading smoke helper for `ppocr_keys_v1.txt`.
- [X] Implement result assembly with boxes, texts, scores, elapsed time, and optional word results placeholder.
- [X] Implement V1 four-result assembly helper for multi-box recognition.
- [X] Implement visualization helper; xlang pipeline `vis()` and `vis_image(...)` write OpenCV-rendered images and draw up to 16 detected boxes.
- [X] Verify xlang pipeline stability across repeated OCR calls; 50-call xlang pipeline smoke passes.
- [X] Verify DLL dependency copy for rapidocr + opencv + onnxruntime chain.

### Python-Style API Candidates

- [X] `engine = rapidocr.RapidOCR()`.
- [X] Callable style is planned via `op_call`.
- [X] Result object shape includes `boxes`, `txts`, `scores`, `word_results`, `elapse_list`, `elapse`.
- [X] Result helpers include `ok`, `text`, `to_json`, and `vis`.
- [X] Add constructor options for model paths; `RapidOCR(det_model, cls_model, rec_model, keys_path)` is supported by the xlang pipeline.
- [X] Add options for `use_det`, `use_cls`, `use_rec`, thresholds, image size, and charset path; call-time `use_det/use_cls/use_rec`, constructor model/charset paths, `set_det_threshold`, `set_cls_image_shape`, and `set_rec_image_shape` work.
- [X] Avoid mirroring every Python option until the pipeline supports it correctly; V1 exposes only model paths, use_det/use_cls/use_rec, det threshold, and cls/rec image shapes.

### Examples and Tests

- [X] Initial dependency smoke returns `42`.
- [X] Add `sample.png` fixture or download script.
- [X] Add RapidOCR model/config contract smoke.
- [X] Add model-file load smoke through ONNX Runtime sessions.
- [X] Add detector preprocess + ONNX Runtime inference smoke.
- [X] Add detector postprocess smoke.
- [X] Add detector multi-box connected-component smoke.
- [X] Add detector NMS/index-count smoke.
- [X] Add detector unclip smoke.
- [X] Add first-box crop smoke.
- [X] Add indexed box helper smoke.
- [X] Add multi-result assembly smoke.
- [X] Add four-result assembly smoke.
- [X] Add angle-classifier inference/decode smoke.
- [X] Add custom model-path constructor smoke.
- [X] Add engine options smoke.
- [X] Add charset/dictionary loading smoke.
- [X] Add OCR smoke with known small image through `rapidocr.RapidOCR()`.
- [X] Add `RapidOCROutput.vis(...)` smoke for the xlang-level pipeline preview output.
- [X] Add multi-box visualization smoke.
- [X] Add missing-model diagnostic test.
- [X] Add repeated-call leak/stability test; 50-call xlang pipeline stability smoke passes.
- [X] Add standalone exe dependency-copy test across transitive deps for the xlang-level pipeline.

## Cross-Library Integration

- [X] ONNX Runtime DLL copying verified for standalone exe output.
- [X] Verify OpenCV runtime DLL copying for `opencv_world4130.dll`.
- [X] Verify OpenCV shim DLL copying for `opencv_xlang.dll`.
- [X] Verify RapidOCR transitive dependency copying for the xlang-level pipeline, including ONNX Runtime DLLs and `opencv_xlang.dll`.
- [X] Decide common sample asset location for images and models; current image fixture lives at `demo5/release/examples/rapidocr/sample.png`, default models live under package/example `models`.
- [X] Decide whether large tensors should pass through JSON, binary buffer handles, or OpenCV image handles. Decision: use binary/typed tensor handles for real path; JSON remains a compatibility smoke path.
- [X] Add a combined example: OpenCV image load -> preprocess -> ONNX Runtime inference.
- [X] Add final combined example: RapidOCR end-to-end OCR; `RapidOCR()` now uses the xlang-level pipeline when no native rapidocr DLL is present.

## Decisions Needed

- [X] Should OpenCV be bundled from an existing local build, built from source, or downloaded as prebuilt binaries? Current choice: downloaded official OpenCV 4.13.0 Windows prebuilt package.
- [X] Should `opencv_xlang.dll` be built with MSVC Build Tools to match official OpenCV ABI, or should OpenCV be rebuilt from source for the available compiler/toolchain? Current decision: use a limited MinGW stb-image backend now, keep the official OpenCV C++ backend source for MSVC later.
- [X] Should RapidOCR use official C API first, or should we implement a new native pipeline around current ONNX models? Current decision: build a new pipeline around downloaded ONNX models.
- [X] Should large tensor transfer use JSON for simplicity or binary handles for performance? Decision: binary/typed handles for performance, JSON for tests and diagnostics.
- [X] Where should model files live in release layout: package `models` is authoritative; example package mirrors `models` for standalone release samples; shared cache is deferred.
- [X] Which non-CPU ONNX Runtime providers are expected in the first supported release? Decision: none for V1; CPU provider only until provider DLLs and memory lifecycle rules are available.

## Suggested Next Order

- [X] Finish ONNX Runtime documentation cleanup and schema notes.
- [X] Implement functional OpenCV backend for current wrapper scope.
- [X] Add OpenCV preprocessing helpers needed by OCR.
- [X] Implement RapidOCR model/layout contract.
- [X] Implement RapidOCR preprocessing and detector path.
- [X] Implement RapidOCR recognizer path V1; typed input, typed float output decode, indexed crop helpers, angle correction, and four-result assembly are complete.
- [X] Broader batching/text quality beyond V1 is deferred to OCR quality fixtures and follow-up tuning; current V1 scope is complete.
- [X] Run combined dependency-copy test.
