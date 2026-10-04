# Builds and runs the off-device tests with MSVC.
#
#   pwsh extras/tests/host/run.ps1 [-ArduinoJson <folder containing ArduinoJson.h>]
#
# Needs Visual Studio (or its Build Tools) with the C++ workload, and ArduinoJson 7. The default
# ArduinoJson location is where `pio pkg install -g -l bblanchon/ArduinoJson` puts it.
# SPDX-License-Identifier: MIT

param(
    [string] $ArduinoJson = (Join-Path $HOME '.platformio/lib/ArduinoJson/src')
)

$ErrorActionPreference = 'Stop'

$library = Resolve-Path (Join-Path $PSScriptRoot '../../..')
$out = Join-Path ([IO.Path]::GetTempPath()) 'mdw-host-tests'
New-Item -ItemType Directory -Force $out | Out-Null

if (-not (Test-Path (Join-Path $ArduinoJson 'ArduinoJson.h'))) {
    throw "ArduinoJson.h is not in '$ArduinoJson'. Install it with: pio pkg install -g -l bblanchon/ArduinoJson"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio with the C++ workload was not found.' }

$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$sources = "`"$library/extras/tests/host/test_client.cpp`" `"$library/src/mdw/Client.cpp`""
$build = "cl /nologo /std:c++17 /EHsc /W4 /WX /I`"$library/src`" /I`"$ArduinoJson`" $sources /Fe`"$out/test_client.exe`" /Fo`"$out/`""

cmd /c "`"$vcvars`" >nul && $build"
if ($LASTEXITCODE -ne 0) { throw 'The tests did not compile.' }

& (Join-Path $out 'test_client.exe')
exit $LASTEXITCODE
