import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("release_assets", "scripts/release_assets.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseAssetsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.assets = self.root / "assets"
        self.docs = self.root / "docs"
        for revision in ("rev1", "rev2"):
            build = self.root / revision
            build.mkdir()
            for filename in ("firmware.bin", "bootloader.bin", "partitions.bin", "firmware.elf", "boot_app0.bin"):
                (build / filename).write_bytes((revision + filename).encode())
            release.package(build, self.assets, revision, "v1.4.0", build / "boot_app0.bin")

    def test_both_manifests_reference_exact_packaged_bytes(self):
        release.update_docs(self.assets, self.docs, "v1.4.0")
        for rev in ("rev1", "rev2"):
            manifest = json.loads((self.docs / f"manifest-{rev}.json").read_text())
            self.assertEqual(manifest["version"], "v1.4.0")
            self.assertEqual([p["offset"] for p in manifest["builds"][0]["parts"]],
                             [0x1000, 0x8000, 0xE000, 0x10000])
            for part in manifest["builds"][0]["parts"]:
                path = Path(part["path"])
                self.assertIn(rev, path.name)
                self.assertEqual((self.docs / path).read_bytes(), (self.assets / path.name).read_bytes())
        self.assertEqual((self.docs / "manifest.json").read_bytes(),
                         (self.docs / "manifest-rev2.json").read_bytes())

    def test_bad_checksum_cannot_modify_installer(self):
        firmware = self.assets / "homekit-grgdo1-rev2-v1.4.0.firmware.bin"
        firmware.write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            release.update_docs(self.assets, self.docs, "v1.4.0")
        self.assertFalse(self.docs.exists())

    def test_missing_revision_cannot_modify_installer(self):
        (self.assets / "homekit-grgdo1-rev1-v1.4.0.bootloader.bin").unlink()
        with self.assertRaisesRegex(ValueError, "Missing rev1"):
            release.update_docs(self.assets, self.docs, "v1.4.0")
        self.assertFalse(self.docs.exists())

    def test_md5_is_for_ota_firmware_only(self):
        name = "homekit-grgdo1-rev1-v1.4.0.firmware"
        self.assertEqual((self.assets / f"{name}.md5").read_text().strip(),
                         hashlib.md5((self.assets / f"{name}.bin").read_bytes()).hexdigest())

    def test_reject_branch_names_as_release_tags(self):
        for tag in ("refs/heads/main", "main", "v1.4.0/other", "v1.4"):
            with self.assertRaises(ValueError):
                release.validate_tag(tag)


if __name__ == "__main__":
    unittest.main()
