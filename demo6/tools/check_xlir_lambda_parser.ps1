param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $inputs = @('test/cases/xlir_lambda_parser_contract.c','src/parser/xl6_parser_api.c','src/parser/xl6_parser.c',
        'src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c','src/lexer/xl6_token.c','src/diag/xl6_diag.c',
        'src/ast/xl6_ast.c','src/common/xl6_output.c','src/common/xl6_console.c')
    & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -O2 -I. @inputs release/xl.dll -o build/test/xlir_lambda_parser.exe
    if ($LASTEXITCODE -ne 0) { throw 'Lambda parser source build failed' }
    & build/test/xlir_lambda_parser.exe
    if ($LASTEXITCODE -ne 0) { throw 'All lambda/type contexts collected; inspect failures' }
} finally { Pop-Location }
