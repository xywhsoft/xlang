[CmdletBinding()]
param([string]$ToolchainRoot, [string]$ArchivePath)
$ErrorActionPreference='Stop'
$ideRoot=Split-Path -Parent $PSScriptRoot
if(-not $ToolchainRoot) { $ToolchainRoot=Join-Path $ideRoot 'build\providers\clangd-23.1.0\clangd_23.1.0' }
if(-not $ArchivePath) { $ArchivePath=Join-Path $ideRoot 'build\providers\clangd-windows-23.1.0.zip' }
$expected='23412a240756a162e7b98a282f36aa2a23a88db5ce16a0cbc4fef7253768c810'
if(-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf) -or (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Official clangd 23.1.0 Windows archive hash mismatch or archive missing.' }
$packageRoot=[IO.Path]::GetFullPath($ToolchainRoot).TrimEnd('\','/')
if((Get-Item -LiteralPath $packageRoot).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'clangd package root must be a regular directory.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ArchivePath))
$known=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$sha=[Security.Cryptography.SHA256]::Create()
$prefix='clangd_23.1.0/'
try {
 foreach($entry in $archive.Entries) {
  $name=$entry.FullName.Replace('\','/')
  if(-not $name.StartsWith($prefix,[StringComparison]::Ordinal) -or $name.Contains('../') -or $name.Contains(':')) { throw "Unexpected clangd archive entry: $name" }
  $relative=$name.Substring($prefix.Length)
  if(-not $relative -or $relative.EndsWith('/')) { continue }
  $path=[IO.Path]::GetFullPath((Join-Path $packageRoot $relative))
  if(-not $path.StartsWith($packageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or -not $known.Add($path)) { throw "Invalid or duplicate clangd archive path: $name" }
  $part=$path
  while($part -and $part.StartsWith($packageRoot,[StringComparison]::OrdinalIgnoreCase)) {
   if((Get-Item -LiteralPath $part -ErrorAction Stop).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "clangd package contains a reparse point: $part" }
   if($part -eq $packageRoot) { break }
   $part=[IO.Path]::GetDirectoryName($part)
  }
  $file=Get-Item -LiteralPath $path
  if($file.PSIsContainer -or $file.Length -ne $entry.Length) { throw "clangd package size mismatch: $relative" }
  $stream=$entry.Open()
  try { $inside=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() } finally { $stream.Dispose() }
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inside) { throw "clangd package hash mismatch: $relative" }
 }
 foreach($file in Get-ChildItem -LiteralPath $packageRoot -Recurse -File -Force) { if(-not $known.Contains($file.FullName)) { throw "Unrecognized clangd package file: $($file.FullName)" } }
 if($known.Count -ne 354) { throw "Unexpected clangd 23.1.0 file count: $($known.Count)" }
} finally { $sha.Dispose(); $archive.Dispose() }
$program=Join-Path $packageRoot 'bin\clangd.exe'
$version=& $program --version 2>&1 | Out-String
if($LASTEXITCODE -ne 0 -or $version -notmatch 'clangd version 23\.1\.0 ' -or $version -notmatch 'ea7d852a70e8bdfaf601d6626a760f9771b2c4b4') { throw 'clangd version or upstream revision mismatch.' }
$options=& $program --help-hidden 2>&1 | Out-String
if($LASTEXITCODE -ne 0 -or $options -notmatch '--use-dirty-headers' -or $options -notmatch '--skip-preamble-build') { throw 'clangd lacks required unsaved-header/full-prelude parsing options.' }
Write-Output "Official clangd 23.1.0 verified: $($known.Count) package files; SHA256 $expected"
