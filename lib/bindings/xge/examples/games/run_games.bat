@echo off
setlocal
pushd "%~dp0..\.."
set "GAME_OUT=%TEMP%\xlang-game-smoke-%RANDOM%-%RANDOM%"
mkdir "%GAME_OUT%" || exit /b 1
for %%G in (2048 asteroids breakout flappy minesweeper pong shooter snake tetris) do (
    xl.exe check "examples\games\%%G.xl" || exit /b 1
    xl.exe build "examples\games\%%G.xl" -o "%GAME_OUT%\%%G.exe" || exit /b 1
    "%GAME_OUT%\%%G.exe" 3 || exit /b 1
)
echo All nine games passed. Outputs: %GAME_OUT%
popd
exit /b 0
