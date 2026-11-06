param([string]$Root = "", [switch]$Run, [int]$Frames = 3)
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$runtime = Join-Path $Root "release\win_x64"
$xl = Join-Path $runtime "xl.exe"
$games = @(Get-ChildItem -LiteralPath (Join-Path $runtime "examples\games") -Filter "*.xl" -File | Where-Object { !$_.Name.StartsWith("_") } | Sort-Object Name)
if (!(Test-Path -LiteralPath $xl) -or $games.Count -ne 9 -or $Frames -le 0) { throw "Missing runtime/games or invalid frame count" }
$out = Join-Path $Root "build\game-examples"
if ($Run) { New-Item -ItemType Directory -Path $out -Force | Out-Null }
Push-Location $runtime
try {
 foreach ($game in $games) {
  & $xl check $game.FullName
  if ($LASTEXITCODE -ne 0) { throw "Semantic check failed: $($game.Name)" }
  if ($Run) {
   $exe = Join-Path $out ($game.BaseName + ".exe")
   & $xl build $game.FullName -o $exe
   if ($LASTEXITCODE -ne 0) { throw "Build failed: $($game.Name)" }
   & $exe $Frames
   if ($LASTEXITCODE -ne 0) { throw "Game failed: $($game.Name)" }
  }
 }
} finally { Pop-Location }
Write-Host "All nine game examples passed."
