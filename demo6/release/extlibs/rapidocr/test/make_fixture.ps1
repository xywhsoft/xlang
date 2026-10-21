param([Parameter(Mandatory = $true)][string]$Target)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath (Split-Path -Parent $Target) -PathType Container)) {
    throw "rapidocr fixture directory does not exist: $Target"
}
Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new(800, 200)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$font = [Drawing.Font]::new('Arial', 76, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$graphics.Clear([Drawing.Color]::White)
$graphics.DrawString('HELLO 123', $font, [Drawing.Brushes]::Black, 28, 48)
if ($bitmap.GetPixel(0, 0).ToArgb() -ne [Drawing.Color]::White.ToArgb() -or
    $bitmap.GetPixel(60, 90).ToArgb() -eq [Drawing.Color]::White.ToArgb()) {
    throw 'OCR fixture drawing failed'
}
$font.Dispose()
$graphics.Dispose()
$bitmap.Save($Target, [Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Output $Target
