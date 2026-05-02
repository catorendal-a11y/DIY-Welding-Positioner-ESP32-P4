param(
  [switch]$SelfTest,
  [string]$Screenshots,
  [string]$UsbMirror,
  [int]$Baud = 4000000
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path "$PSScriptRoot\.."
$build = Join-Path $PSScriptRoot "build"

cmake -S $PSScriptRoot -B $build -G Ninja `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++

$simExe = Join-Path $build "rotator_simulator.exe"
$mirrorExe = Join-Path $build "rotator_usb_mirror.exe"
if ($UsbMirror) {
  cmake --build $build --target rotator_usb_mirror
  if ($LASTEXITCODE -ne 0) { throw "simulator build exited with code $LASTEXITCODE" }
  & $mirrorExe $UsbMirror $Baud
} elseif ($SelfTest) {
  cmake --build $build --target rotator_simulator
  if ($LASTEXITCODE -ne 0) { throw "simulator build exited with code $LASTEXITCODE" }
  & $simExe --self-test
} elseif ($Screenshots) {
  cmake --build $build --target rotator_simulator
  if ($LASTEXITCODE -ne 0) { throw "simulator build exited with code $LASTEXITCODE" }
  & $simExe --screenshots $Screenshots
} else {
  cmake --build $build --target rotator_simulator
  if ($LASTEXITCODE -ne 0) { throw "simulator build exited with code $LASTEXITCODE" }
  & $simExe
}

if ($LASTEXITCODE -ne 0) {
  throw "simulator tool exited with code $LASTEXITCODE"
}
