@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0\.."

if not exist build mkdir build
g++ -std=c++17 -Iinclude src\Compiler.cpp src\main.cpp -o build\mini_compiler.exe
if errorlevel 1 exit /b 1

build\mini_compiler.exe examples\pas.dat --tokens examples\pas.tok >nul
if errorlevel 1 exit /b 1
fc /w examples\pas.med tests\expected_pas.med >nul
if errorlevel 1 (
  echo FAILED: task sample quadruples
  exit /b 1
)
echo PASSED: task sample quadruples

> build\expr.dat <nul set /p dummy=A:=A+B*C#~
build\mini_compiler.exe build\expr.dat --no-asm >nul
if errorlevel 1 exit /b 1
fc /w build\expr.med tests\expected_expr.med >nul
if errorlevel 1 (
  echo FAILED: arithmetic precedence
  exit /b 1
)
echo PASSED: arithmetic precedence

> build\bool.dat <nul set /p dummy=while A^<C and B^<D do A:=A+1#~
build\mini_compiler.exe build\bool.dat >nul
if errorlevel 1 exit /b 1
findstr /c:"(and," build\bool.med >nul || exit /b 1
findstr /c:"(j,,,100)" build\bool.med >nul || exit /b 1
echo PASSED: boolean expression

> build\logic.dat <nul set /p dummy=if not A=B or C^>D then X:=1 else X:=2#~
build\mini_compiler.exe build\logic.dat >nul
if errorlevel 1 exit /b 1
findstr /c:"(or," build\logic.med >nul || exit /b 1
echo PASSED: not/or compilation

findstr /c:"L100:" examples\pas.asm >nul || exit /b 1
findstr /c:"INT 21H" examples\pas.asm >nul || exit /b 1
echo PASSED: assembly output

> build\bad.dat <nul set /p dummy=A:=1
build\mini_compiler.exe build\bad.dat >nul 2>nul
if not errorlevel 1 (
  echo FAILED: syntax error handling
  exit /b 1
)
echo PASSED: syntax error handling
echo All tests passed.
