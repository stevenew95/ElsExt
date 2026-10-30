@echo off
rem Builds hooktest.exe and vcftest.exe into obj\tools.
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Visual Studio with the C++ workload was not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1

set "ROOT=%~dp0.."
set "OUT=%ROOT%\obj\tools"
set "MINHOOK=%ROOT%\third_party\minhook"
if not exist "%OUT%" mkdir "%OUT%"
set "FLAGS=/nologo /W4 /O1 /MT /EHsc /std:c++20 /DNOMINMAX /I"%ROOT%\include" /I"%MINHOOK%\include" /Fo"%OUT%\\""

cl /c /nologo /W1 /O1 /MT /Fo"%OUT%\\" "%MINHOOK%\src\buffer.c" "%MINHOOK%\src\hook.c" "%MINHOOK%\src\trampoline.c" "%MINHOOK%\src\hde\hde64.c" || exit /b 1
cl %FLAGS% /Fe"%OUT%\hooktest.exe" "%~dp0hooktest.cpp" "%OUT%\buffer.obj" "%OUT%\hook.obj" "%OUT%\trampoline.obj" "%OUT%\hde64.obj" || exit /b 1
cl %FLAGS% /Fe"%OUT%\vcftest.exe" "%~dp0vcftest.cpp" "%ROOT%\src\Vcf.cpp" "%ROOT%\src\Log.cpp" || exit /b 1
echo Built hooktest.exe and vcftest.exe in %OUT%
