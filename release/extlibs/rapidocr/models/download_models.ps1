$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$files = @(
	@{
		Name = "ch_PP-OCRv4_det_infer.onnx"
		Url = "https://huggingface.co/SWHL/RapidOCR/resolve/main/PP-OCRv4/ch_PP-OCRv4_det_infer.onnx"
	},
	@{
		Name = "ch_ppocr_mobile_v2.0_cls_infer.onnx"
		Url = "https://huggingface.co/SWHL/RapidOCR/resolve/main/PP-OCRv1/ch_ppocr_mobile_v2.0_cls_infer.onnx"
	},
	@{
		Name = "ch_PP-OCRv4_rec_infer.onnx"
		Url = "https://huggingface.co/SWHL/RapidOCR/resolve/main/PP-OCRv4/ch_PP-OCRv4_rec_infer.onnx"
	},
	@{
		Name = "ppocr_keys_v1.txt"
		Url = "https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/release/2.6/ppocr/utils/ppocr_keys_v1.txt"
	}
)

foreach ($file in $files) {
	$out = Join-Path $root $file.Name
	if (Test-Path $out) {
		Write-Host "[skip] $($file.Name)"
		continue
	}
	Write-Host "[download] $($file.Name)"
	Invoke-WebRequest -Uri $file.Url -OutFile $out
}

Get-ChildItem -Path $root -File | Sort-Object Name | Select-Object Name,Length
