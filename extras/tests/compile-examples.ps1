# Compiles every example against this library for a Cheap Yellow Display, with PlatformIO.
#
#   pwsh extras/tests/compile-examples.ps1
#
# A compile check, not a test: the examples are what a newcomer copies first, so a broken one is the
# library's first impression. Uses the TFT_eSPI setup for the ILI9341 panel (see extras/boards).
# SPDX-License-Identifier: MIT

$ErrorActionPreference = 'Stop'

$library = Resolve-Path (Join-Path $PSScriptRoot '../..')
$pio = (Get-Command pio -ErrorAction SilentlyContinue)?.Source
if (-not $pio) { $pio = Join-Path $HOME '.platformio/penv/Scripts/pio.exe' }

$flags = @(
    '-DUSER_SETUP_LOADED=1', '-DILI9341_2_DRIVER=1', '-DTFT_WIDTH=240', '-DTFT_HEIGHT=320',
    '-DTFT_MISO=12', '-DTFT_MOSI=13', '-DTFT_SCLK=14', '-DTFT_CS=15', '-DTFT_DC=2', '-DTFT_RST=-1',
    '-DTFT_BL=21', '-DTFT_BACKLIGHT_ON=HIGH', '-DUSE_HSPI_PORT=1', '-DSPI_FREQUENCY=40000000',
    '-DLOAD_GLCD=1', '-DLOAD_FONT2=1', '-DLOAD_FONT4=1'
) -join ' '

$failed = @()

foreach ($sketch in Get-ChildItem (Join-Path $library 'examples') -Recurse -Filter *.ino) {
    Write-Host "== $($sketch.Directory.Name)"
    & $pio ci $sketch.FullName --lib $library --board esp32dev `
        -O 'lib_deps=bodmer/TFT_eSPI@^2.5.43, bblanchon/ArduinoJson@^7.4.0' `
        -O "build_flags=$flags" | Select-String -Pattern 'error|SUCCESS|FAILED|RAM:|Flash:'
    if ($LASTEXITCODE -ne 0) { $failed += $sketch.Directory.Name }
}

if ($failed) { throw "Did not compile: $($failed -join ', ')" }
Write-Host 'Every example compiles.'
