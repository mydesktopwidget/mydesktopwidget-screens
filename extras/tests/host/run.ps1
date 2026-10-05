# Builds and runs the off-device tests with MSVC.
#
#   pwsh extras/tests/host/run.ps1 [-ArduinoJson <folder containing ArduinoJson.h>]
#
# Runs the protocol tests, the LVGL binding tests and the SquareLine script tests (Python 3).
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
$client = "`"$library/src/mdw/Client.cpp`""
$flags = "/nologo /std:c++17 /EHsc /W4 /WX /I`"$library/src`" /I`"$ArduinoJson`""

# The protocol, and the LVGL binding against the stand-in LVGL in fake_lvgl/ - each its own program,
# so the stand-in never meets the protocol tests. Objects go to their own folders: both build Client.cpp.
$failed = 0
foreach ($test in 'test_client', 'test_lvgl') {
    $objects = Join-Path $out $test
    New-Item -ItemType Directory -Force $objects | Out-Null
    $include = if ($test -eq 'test_lvgl') { "/I`"$library/extras/tests/host/fake_lvgl`"" } else { '' }
    cmd /c "`"$vcvars`" >nul && cl $flags $include `"$library/extras/tests/host/$test.cpp`" $client /Fe`"$out/$test.exe`" /Fo`"$objects/`""
    if ($LASTEXITCODE -ne 0) { throw "$test did not compile." }

    Write-Host "== $test"
    & (Join-Path $out "$test.exe")
    if ($LASTEXITCODE -ne 0) { $failed++ }
}

# The SquareLine build script.
Write-Host '== squareline'
python -m unittest discover -s (Join-Path $library 'extras/tests/squareline') 2>&1 | Select-String -Pattern '^(Ran|OK|FAILED|ERROR|FAIL:)'
if ($LASTEXITCODE -ne 0) { $failed++ }

exit $failed
