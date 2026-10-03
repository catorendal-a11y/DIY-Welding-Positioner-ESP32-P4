"""Package verified ESP32-P4 build outputs with flashing metadata and checksums."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def license_notices():
    dependencies = ROOT / '.pio/libdeps/esp32p4-release'
    notices = [ROOT / 'LICENSE', ROOT / 'src/ui/fonts/OFL-Montserrat.txt',
               dependencies / 'FastAccelStepper/LICENSE',
               dependencies / 'ArduinoJson/LICENSE.txt',
               dependencies / 'lvgl/LICENCE.txt',
               dependencies / 'lvgl/COPYRIGHTS.md',
               dependencies / 'lvgl/src/stdlib/builtin/LICENSE_TLSF.txt',
               dependencies / 'lvgl/src/stdlib/builtin/LICENSE_SPRINTF.txt',
               dependencies / 'lvgl/scripts/generators/built_in_font/font_license/FontAwesome5/LICENSE.txt']
    notices.extend(sorted((ROOT / 'lib').glob('*/license.txt')))
    sections = ['Project, font and direct dependency notices. Framework and other components retain their original licenses in the pinned source distributions.\n']
    for path in notices:
        sections.append('\n--- ' + path.relative_to(ROOT).as_posix() + ' ---\n\n' +
                        path.read_text(encoding='utf-8-sig'))
    return '\n'.join(sections).encode('utf-8')


def partitions(data):
    entries = []
    for position in range(0, len(data), 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack(
            '<HBBII16sI', data[position:position + 32])
        if magic != 0x50AA:
            break
        entries.append(dict(name=label.rstrip(b'\0').decode(), type=kind,
                            subtype=subtype, offset=hex(offset), size=hex(size)))
    expected = {'nvs': (0x9000, 0x5000), 'otadata': (0xE000, 0x2000),
                'app0': (0x10000, 0x640000), 'app1': (0x650000, 0x640000),
                'spiffs': (0xC90000, 0x360000), 'coredump': (0xFF0000, 0x10000)}
    actual = {p['name']: (int(p['offset'], 16), int(p['size'], 16)) for p in entries}
    if actual != expected:
        raise ValueError('Partition layout differs from FLASHING.md: ' + repr(actual))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--build-root', type=Path, default=ROOT / '.pio/build-fw')
    parser.add_argument('--output', type=Path, default=ROOT / 'release')
    parser.add_argument('--boot-app0', type=Path)
    parser.add_argument('--commit')
    parser.add_argument('--run-url', default='local validation')
    args = parser.parse_args()
    if not re.fullmatch(r'v\d+\.\d+\.\d+', args.version):
        parser.error('Version must use vX.Y.Z format')
    configured = re.search(r'#define FW_VERSION "([^"]+)"',
                           (ROOT / 'src/config.h').read_text(encoding='utf-8')).group(1)
    if configured != args.version:
        raise ValueError('Release tag does not match FW_VERSION')
    commit = args.commit or subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', str(Path.home() / '.platformio')))
    ota = args.boot_app0 or core / 'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    ota_data = ota.read_bytes()
    if len(ota_data) != 8192:
        raise ValueError('Unexpected OTA initialization image size')
    guide = (ROOT / 'docs/releases/FLASHING.md').read_bytes()
    licenses = license_notices()
    args.output.mkdir(parents=True, exist_ok=True)
    prepared = {}
    for variant in ['release', 'debug', 'mirror']:
        environment = 'esp32p4-' + variant
        folder = args.build_root / environment
        applications = list(folder.glob('fw_*.bin'))
        if len(applications) != 1:
            raise ValueError('Use clean build outputs with one application per environment: ' + environment)
        application = applications[0].read_bytes()
        if args.version.encode() not in application:
            raise ValueError('Application version string missing: ' + environment)
        table = (folder / 'partitions.bin').read_bytes()
        layout = partitions(table)
        if len(application) > 0x640000:
            raise ValueError('Application exceeds its flash slot')
        metadata = dict(version=args.version, commit=commit, environment=environment,
                        validation_run=args.run_url, application_offset='0x10000',
                        partitions=layout, boot_app0_source='Pinned pioarduino Arduino framework')
        files = {'firmware.bin': application,
                 'bootloader.bin': (folder / 'bootloader.bin').read_bytes(),
                 'partitions.bin': table, 'boot_app0.bin': ota_data,
                 'FLASHING.md': guide, 'LICENSES.txt': licenses,
                 'BUILD.json': (json.dumps(metadata, indent=2) + '\n').encode()}
        files['SHA256SUMS.txt'] = ''.join(checksum(data) + '  ' + name + '\n'
                                       for name, data in sorted(files.items())).encode()
        prefix = 'welding-positioner-' + args.version + '-' + variant
        prepared[prefix + '.bin'] = application
        bundle = args.output / (prefix + '.zip')
        with zipfile.ZipFile(bundle, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o644 << 16
                archive.writestr(entry, data)
        print(environment + ': version and flash layout checked')
    for name, data in prepared.items():
        (args.output / name).write_bytes(data)
    shutil.copyfile(ROOT / 'docs/images/ui_mockup_v5.zip',
                    args.output / ('welding-positioner-' + args.version + '-design-svg.zip'))
    (args.output / 'FLASHING.md').write_bytes(guide)
    (args.output / 'LICENSES.txt').write_bytes(licenses)
    assets = sorted(p for p in args.output.iterdir() if p.is_file() and p.name != 'SHA256SUMS.txt')
    (args.output / 'SHA256SUMS.txt').write_text(
        ''.join(checksum(p.read_bytes()) + '  ' + p.name + '\n' for p in assets), encoding='utf-8')
    print('Packaged ' + str(len(assets) + 1) + ' assets for ' + commit)


if __name__ == '__main__':
    main()
