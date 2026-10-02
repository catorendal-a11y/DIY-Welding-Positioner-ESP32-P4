"""Run the pinned FastAccelStepper IDF5/6 encoder's upstream host regression."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path,
                        default=ROOT / '.pio/libdeps/esp32p4-release/FastAccelStepper')
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--output', type=Path, default=ROOT / '.pio/fas-rmt-host')
    args = parser.parse_args()
    library = args.library.resolve()
    tests = library / 'extras/tests/pc_based'
    sources = [tests / 'test_30.cpp', tests / 'StepperISR_test.cpp']
    sources.extend(library / 'src' / name for name in [
        'FastAccelStepper.cpp', 'FastAccelStepperEngine.cpp',
        'log2/Log2Representation.cpp', 'fas_ramp/RampGenerator.cpp',
        'fas_ramp/RampControl.cpp', 'fas_ramp/RampCalculator.cpp',
        'fas_queue/queue_add_entry.cpp', 'fas_queue/queue_get_position.cpp',
        'fas_queue/queue_init.cpp', 'fas_queue/queue_utils.cpp'])
    for source in sources:
        if not source.is_file():
            raise FileNotFoundError(f'Missing upstream regression source: {source}')
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise FileNotFoundError(f'C++ compiler not found: {args.compiler}')
    args.output.mkdir(parents=True, exist_ok=True)
    binary = args.output.resolve() / 'rmt_encoder_test.exe'
    subprocess.run([compiler, '-std=c++17', '-DTEST', '-DF_CPU=16000000',
                    '-I' + str(library / 'src'), '-I' + str(tests),
                    *map(str, sources), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
