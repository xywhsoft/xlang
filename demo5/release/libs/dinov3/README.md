# dinov3

`dinov3` is a small xlang wrapper for running DINOv3 ONNX image embedding models through the local `onnxruntime` and `opencv` packages.

The default model path is:

```text
E:\workspace\llmrunner\models\dinov3-vits16plus_fp32.onnx
```

The bundled defaults match the local model currently tested:

- input name: `image`
- input shape: `[1, 3, 224, 224]`
- output name: `embedding`
- output shape: `[1, 384]`
- preprocessing: BGR image load, RGB conversion, resize shortest side to `256`, center crop to `224x224`, ImageNet mean/std normalization

## Example

```xl
import dinov3
import std.io

func main() as int
	auto model = dinov3.DinoV3("E:\\workspace\\llmrunner\\models\\dinov3-vits16plus_fp32.onnx")
	auto result = model("D:\\git\\model-benchmark\\OCR模型\\image\\1.jpg")
	std.io.println(result.shape_json())
	std.io.println(result.count())
	std.io.println(result.l2_norm())
	std.io.println(result.embedding_json())
	return 0
end
```

Use `set_direct_resize(true)` if the caller wants plain `224x224` resize instead of aspect-preserving resize plus center crop.
