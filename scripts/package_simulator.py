"""Create a portable Windows simulator ZIP from an existing MinGW build."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'simulator/build')
    parser.add_argument('--toolchain', type=Path, default=Path('C:/msys64/mingw64'))
    parser.add_argument('--output', type=Path, default=ROOT / 'release')
    parser.add_argument('--allow-dirty', action='store_true', help='Allow an explicitly labeled local development build')
    args = parser.parse_args()
    version = re.search(r'#define FW_VERSION "([^"]+)"',
                        (ROOT / 'src/config.h').read_text(encoding='utf-8')).group(1)
    name = f'welding-positioner-{version}-simulator-windows-x64'
    args.output.mkdir(parents=True, exist_ok=True)
    executable = args.build.resolve() / 'rotator_simulator.exe'
    identity = json.loads(subprocess.check_output([str(executable), '--build-info'], text=True))
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if identity.get('version') != version or identity.get('commit') != commit:
        raise ValueError('Executable identity does not match this checkout; rebuild simulator')
    source_changes = subprocess.check_output(
        ['git', 'status', '--porcelain', '--untracked-files=normal', '--', 'src', 'simulator', 'lib'],
        cwd=ROOT, text=True).strip()
    if (identity.get('dirty') or source_changes) and not args.allow_dirty:
        raise ValueError('Dirty development binary; rebuild from a clean commit for publication')
    pe_info = subprocess.check_output([str(args.toolchain / 'bin/objdump.exe'), '-f', str(executable)], text=True)
    if 'pei-x86-64' not in pe_info:
        raise ValueError('Expected a Windows x64 executable')
    with tempfile.TemporaryDirectory(prefix='sim-stage-', dir=args.output.resolve()) as staging:
        folder = Path(staging) / name
        folder.mkdir()
        package_into(folder, executable, args, identity, name)


def package_into(folder, executable, args, identity, name):
    pending = [executable]
    included = set()
    while pending:
        binary = pending.pop()
        if binary.name.lower() in included:
            continue
        included.add(binary.name.lower())
        shutil.copy2(binary, folder / binary.name)
        imports = subprocess.check_output([str(args.toolchain / 'bin/objdump.exe'),
                                           '-p', str(binary)], text=True)
        for dependency in re.findall(r'DLL Name:\s*(\S+)', imports):
            local = args.toolchain / 'bin' / dependency
            if local.is_file():
                pending.append(local)
            elif not (Path('C:/Windows/System32') / dependency).is_file() and not dependency.lower().startswith(('api-ms-', 'ext-ms-')):
                raise RuntimeError('Unresolved dependency: ' + dependency)
    guide = (ROOT / 'docs/releases/SIMULATOR.md').read_bytes()
    (folder / 'README.txt').write_bytes(guide)
    (folder / 'Start Simulator.cmd').write_text('@echo off\ncd /d "%~dp0"\nstart "" "%~dp0rotator_simulator.exe"\n', encoding='ascii')
    (folder / 'Run Self Test.cmd').write_text('@echo off\ncd /d "%~dp0"\nrotator_simulator.exe --self-test > self-test.log 2>&1\ntype self-test.log\npause\n', encoding='ascii')
    licenses = [ROOT / 'LICENSE', ROOT / 'src/ui/fonts/OFL-Montserrat.txt',
                ROOT / '.pio/libdeps/esp32p4-release/ArduinoJson/LICENSE.txt',
                ROOT / '.pio/libdeps/esp32p4-release/lvgl/LICENCE.txt',
                ROOT / '.pio/libdeps/esp32p4-release/lvgl/COPYRIGHTS.md',
                ROOT / '.pio/libdeps/esp32p4-release/lvgl/src/stdlib/builtin/LICENSE_TLSF.txt',
                ROOT / '.pio/libdeps/esp32p4-release/lvgl/src/stdlib/builtin/LICENSE_SPRINTF.txt',
                ROOT / '.pio/libdeps/esp32p4-release/lvgl/scripts/built_in_font/font_license/FontAwesome5/LICENSE.txt']
    for dependency in ['SDL2', 'gcc-libs', 'winpthreads']:
        notices = sorted(p for p in (args.toolchain / 'share/licenses' / dependency).glob('*') if p.is_file())
        if not notices:
            raise FileNotFoundError('Missing toolchain license notices: ' + dependency)
        licenses.extend(notices)
    missing = [str(p) for p in licenses if not p.is_file()]
    if missing:
        raise FileNotFoundError('Missing required licenses: ' + ', '.join(missing))
    notices = '\n'.join('\n--- ' + p.name + ' ---\n' + p.read_text(encoding='utf-8', errors='replace') for p in licenses)
    (folder / 'LICENSES.txt').write_text(notices, encoding='utf-8')
    identity.update({'type': 'offline simulator', 'libraries': sorted(included),
                     'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()})
    (folder / 'BUILD.json').write_text(json.dumps(identity, indent=2) + '\n', encoding='utf-8')
    files = sorted(p for p in folder.iterdir() if p.is_file() and p.name != 'SHA256SUMS.txt')
    (folder / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files), encoding='ascii')
    archive = args.output / (name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zip_file:
        for path in sorted(p for p in folder.iterdir() if p.is_file()):
            zip_file.write(path, name + '/' + path.name)
    checksum = archive.with_suffix('.zip.sha256')
    checksum.write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n', encoding='ascii')
    print(archive)


if __name__ == '__main__':
    main()
