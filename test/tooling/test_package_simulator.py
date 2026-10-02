import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

MODULE_PATH = Path(__file__).resolve().parents[2] / 'scripts/package_simulator.py'
SPEC = importlib.util.spec_from_file_location('sim_package', MODULE_PATH)
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)


class SimulatorPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        self.build.mkdir()
        (self.build / 'rotator_simulator.exe').write_bytes(b'test executable')
        self.toolchain = self.root / 'toolchain'
        self.identity = {'version': 'v2.1.0', 'commit': 'abc123', 'dirty': False}
        files = ['src/config.h', 'docs/releases/SIMULATOR.md', 'LICENSE',
                 'src/ui/fonts/OFL-Montserrat.txt',
                 '.pio/libdeps/esp32p4-release/ArduinoJson/LICENSE.txt',
                 '.pio/libdeps/esp32p4-release/lvgl/LICENCE.txt',
                 '.pio/libdeps/esp32p4-release/lvgl/COPYRIGHTS.md',
                 '.pio/libdeps/esp32p4-release/lvgl/src/stdlib/builtin/LICENSE_TLSF.txt',
                 '.pio/libdeps/esp32p4-release/lvgl/src/stdlib/builtin/LICENSE_SPRINTF.txt',
                 '.pio/libdeps/esp32p4-release/lvgl/scripts/built_in_font/font_license/FontAwesome5/LICENSE.txt']
        for file in files:
            target = self.root / file
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('License notice', encoding='utf-8')
        (self.root / 'src/config.h').write_text('#define FW_VERSION "v2.1.0"', encoding='utf-8')
        for dependency in ['SDL2', 'gcc-libs', 'winpthreads']:
            folder = self.toolchain / 'share/licenses' / dependency
            folder.mkdir(parents=True)
            (folder / 'LICENSE').write_text('Required license', encoding='utf-8')

    def command(self, args, **kwargs):
        if '--build-info' in args:
            return json.dumps(self.identity)
        if args[0] == 'git':
            if args[1] == 'status':
                return ''
            return 'abc123\n'
        if '-f' in args:
            return 'file format pei-x86-64'
        return ''

    def run_package(self):
        arguments = ['package', '--build', str(self.build), '--toolchain', str(self.toolchain),
                     '--output', str(self.root / 'output')]
        with patch.object(package, 'ROOT', self.root), patch('sys.argv', arguments), \
                patch.object(package.subprocess, 'check_output', self.command):
            package.main()

    def test_rejects_mislabeled_binary(self):
        self.identity['commit'] = 'oldcommit'
        with self.assertRaisesRegex(ValueError, 'does not match'):
            self.run_package()

    def test_rejects_dirty_binary_for_publication(self):
        self.identity['dirty'] = True
        with self.assertRaisesRegex(ValueError, 'Dirty'):
            self.run_package()

    def test_requires_license_notices(self):
        (self.root / 'LICENSE').unlink()
        with self.assertRaisesRegex(FileNotFoundError, 'Missing required licenses'):
            self.run_package()

    def test_packages_split_runtime_license_notices(self):
        for old in ['gcc-libs', 'winpthreads']:
            folder = self.toolchain / 'share/licenses' / old
            (folder / 'LICENSE').unlink()
            folder.rmdir()
        for dependency in ['libgcc', 'libstdc++', 'libwinpthread']:
            folder = self.toolchain / 'share/licenses' / dependency
            folder.mkdir()
            (folder / 'LICENSE').write_text(dependency + ' notice', encoding='utf-8')
        self.run_package()
        with zipfile.ZipFile(next((self.root / 'output').glob('*.zip'))) as bundle:
            notices = bundle.read('welding-positioner-v2.1.0-simulator-windows-x64/LICENSES.txt').decode()
            for dependency in ['libgcc', 'libstdc++', 'libwinpthread']:
                self.assertIn(dependency + ' notice', notices)

    def test_rejects_missing_split_cpp_runtime_license(self):
        folder = self.toolchain / 'share/licenses/gcc-libs'
        (folder / 'LICENSE').unlink()
        folder.rmdir()
        folder = self.toolchain / 'share/licenses/libgcc'
        folder.mkdir()
        (folder / 'LICENSE').write_text('gcc notice', encoding='utf-8')
        with self.assertRaisesRegex(FileNotFoundError, 'libstdc'):
            self.run_package()

    def test_clean_staging_excludes_previous_files_and_preserves_identity(self):
        previous = self.root / 'output/welding-positioner-v2.1.0-simulator-windows-x64'
        previous.mkdir(parents=True)
        (previous / 'private-log.txt').write_text('must not enter ZIP', encoding='utf-8')
        self.run_package()
        archive = next((self.root / 'output').glob('*.zip'))
        with zipfile.ZipFile(archive) as bundle:
            self.assertFalse(any('private-log' in name for name in bundle.namelist()))
            metadata = json.loads(bundle.read('welding-positioner-v2.1.0-simulator-windows-x64/BUILD.json'))
            self.assertEqual(metadata['commit'], 'abc123')
            self.assertEqual(len(metadata['executable_sha256']), 64)
        self.assertTrue((previous / 'private-log.txt').exists())


if __name__ == '__main__':
    unittest.main()
