param([string]$Root='', [string]$PublicDirectory='', [string]$Directory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
# The public gate includes typed/lowered counts, callable signature, retained
# capture and conditional merge, source JIT/module, deterministic strict C,
# independent AOT/provider pruning, plus the full Thread language/lifetime gate.
# Generated C calls resident xl6Thread*V1; it must not own a thread algorithm.
& "$PSScriptRoot/check_xlir_runtime_operand_catalog.ps1" -Root $Root -PublicDirectory $PublicDirectory -Directory $Directory
if (!$?) { throw 'Resident Thread contract quality gate failed' }
