# onnxruntime xlang package

This package exposes a small Python-style ONNX Runtime wrapper for xlang.

## Tensor JSON schema

`InferenceSession.run(output_names_json, input_feed_json)` accepts tensors as JSON strings.

Input feed format:

```json
{
  "input_name": {
    "dtype": "float32",
    "shape": [1, 3],
    "data": [1.0, 2.0, 3.0]
  }
}
```

Output names format:

```json
["output_name"]
```

Supported `dtype` values:

- `float32`, `float64`
- `int64`, `int32`, `int16`, `int8`
- `uint64`, `uint32`, `uint16`, `uint8`
- `bool`
- `string`

Returned tensors use the same `{ dtype, shape, data }` shape inside a JSON object keyed by output name.

## Notes

- JSON tensors are convenient for smoke tests and small models.
- `TensorZerosF32(shape_json)` and `sess.run_tensor(input_name, tensor, outputs_json)` provide the first typed tensor handle path.
- `TensorFromOpenCV(image.handle, scale, mean0, mean1, mean2, std0, std1, std2)` creates a CHW float32 tensor from the current OpenCV image backend without JSON input transfer.
- `RunResult.float_shape_json()`, `RunResult.float_count()`, and `RunResult.float_at(index)` expose the first typed float output reader path.
- CPU inference is the verified provider path in this package. The current local bundle includes `onnxruntime.dll`, `onnxruntime_providers_shared.dll`, and `onnxruntime_xlang.dll`, but no non-CPU provider DLLs.
