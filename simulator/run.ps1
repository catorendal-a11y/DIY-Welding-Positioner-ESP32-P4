param(
  [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path "$PSScriptRoot\.."
$build = Join-Path $PSScriptRoot "build"

cmake -S $PSScriptRoot -B $build -G Ninja `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++

cmake --build $build

$exe = Join-Path $build "rotator_simulator.exe"
if ($SelfTest) {
  & $exe --self-test
} else {
  & $exe
}

if ($LASTEXITCODE -ne 0) {
  throw "rotator_simulator.exe exited with code $LASTEXITCODE"
}
