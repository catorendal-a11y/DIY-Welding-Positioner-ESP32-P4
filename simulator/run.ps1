param(
  [switch]$SelfTest,
  [string]$UsbMirror,
  [int]$Baud = 2000000
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path "$PSScriptRoot\.."
$build = Join-Path $PSScriptRoot "build"

cmake -S $PSScriptRoot -B $build -G Ninja `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++

cmake --build $build

$simExe = Join-Path $build "rotator_simulator.exe"
$mirrorExe = Join-Path $build "rotator_usb_mirror.exe"
if ($UsbMirror) {
  & $mirrorExe $UsbMirror $Baud
} elseif ($SelfTest) {
  & $simExe --self-test
} else {
  & $simExe
}

if ($LASTEXITCODE -ne 0) {
  throw "simulator tool exited with code $LASTEXITCODE"
}
