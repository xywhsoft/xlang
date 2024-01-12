tcc\tcc -m64 main.c antlr/XScriptLexer.c antlr/XScriptParser.c -o release/x64/test.exe -ID:\xLang\Antlr\antlr

@echo;
@echo off

cd release
cd x64
test.exe

pause
