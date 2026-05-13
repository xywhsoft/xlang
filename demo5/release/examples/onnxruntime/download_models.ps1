$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $root "squeezenet1.1-7.onnx"
$url = "https://github.com/onnx/models/raw/main/validated/vision/classification/squeezenet/model/squeezenet1.1-7.onnx"

if (-not (Test-Path -LiteralPath $out)) {
    Invoke-WebRequest -Uri $url -OutFile $out
}

Write-Host $out
