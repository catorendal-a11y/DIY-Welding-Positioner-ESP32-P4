import importlib.util
from pathlib import Path
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[2] / 'scripts/package_release.py'
SPEC = importlib.util.spec_from_file_location('package_release', MODULE_PATH)
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class FirmwareImageSelectionTests(unittest.TestCase):
    def test_selects_application_alongside_combined_factory_image(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            application = folder / 'fw_9fa_a83707.bin'
            application.write_bytes(b'application')
            (folder / 'fw_9fa_a83707.factory.bin').write_bytes(b'bootloader-and-application')
            (folder / 'bootloader.bin').write_bytes(b'bootloader')
            (folder / 'partitions.bin').write_bytes(b'partitions')
            self.assertEqual(release.application_image(folder).read_bytes(), b'application')

    def test_rejects_factory_only_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / 'fw_9fa_a83707.factory.bin').write_bytes(b'combined-image')
            with self.assertRaises(ValueError):
                release.application_image(folder)

    def test_rejects_multiple_application_builds(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / 'fw_first.bin').write_bytes(b'first')
            (folder / 'fw_second.bin').write_bytes(b'second')
            with self.assertRaises(ValueError):
                release.application_image(folder)

    def test_rejects_missing_build_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                release.application_image(Path(temporary))


if __name__ == '__main__':
    unittest.main()
