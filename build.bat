premake5 gmake2

if %errorlevel% neq 0 (
    exit /b %errorlevel%
)

compiledb make

if %errorlevel% neq 0 (
    exit /b %errorlevel%
)

D:\dev\agmp\bin\Debug\agmp.exe
