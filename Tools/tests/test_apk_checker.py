"""Mock tool/archive tests; these do not validate an actual Unreal APK."""
import argparse
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SOURCE = Path(__file__).resolve().parents[1] / "verify_android_apk.py"
spec = importlib.util.spec_from_file_location("apk_checker", SOURCE)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class ApkCheckerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / "project"
        (self.project / "Content/Maps").mkdir(parents=True)
        (self.project / "Content/Maps/Oran.umap").write_bytes(b"map")
        engine = self.root / "engine"
        (engine / "Engine/Binaries/Linux").mkdir(parents=True)
        (engine / "Engine/Binaries/Linux/UnrealPak").write_bytes(b"stub")
        output = self.root / "out"
        output.mkdir()
        self.args = argparse.Namespace(
            apk=self.root/"test.apk", project=self.project, engine_root=engine,
            aapt=Path("aapt"), version_code="21", version_name="2.1", output=output,
        )
        self.entries = ["maps/oran.umap", "fonts/fokufi.ttf"]
        self.package = "com.ayoub5550.falloforan"
        self.list_fail = False
        self.calls = []

    def make_apk(self, ucas=True, native=True):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            prefix = "FallOfOran/Content/Paks/"
            archive.writestr(prefix+"FallOfOran-Android_ASTC.utoc", b"index")
            archive.writestr(prefix+"FallOfOran-Android_ASTC.pak", b"font")
            archive.writestr(prefix+"global.utoc", b"global metadata only")
            if ucas:
                archive.writestr(prefix+"FallOfOran-Android_ASTC.ucas", b"data")
        with zipfile.ZipFile(self.args.apk, "w") as archive:
            if native:
                archive.writestr("lib/arm64-v8a/libUnreal.so", b"binary")
            archive.writestr("assets/main.obb.png", buffer.getvalue())

    def run_tool(self, command, log):
        self.calls.append(command)
        if command[0] == "aapt":
            return f"package: name='{self.package}' versionCode='21' versionName='2.1'\n"
        if self.list_fail:
            raise RuntimeError("UnrealPak failed")
        self.assertNotEqual(Path(command[1]).name, "global.utoc")
        self.assertIn("-extracttomountpoint", command)
        csv = Path(next(arg[5:] for arg in command if arg.startswith("-csv=")))
        csv.write_text("Filename, Offset, Size\n" + "".join(
            f"../../../FallOfOran/Content/{entry}, 0, 10\n" for entry in self.entries
        ))
        return ""

    def verify(self):
        report = {}
        with patch.object(checker, "run_checked", side_effect=self.run_tool):
            checker.verify(self.args, report)
        return report

    def test_complete_mock_archive(self):
        self.make_apk()
        report = self.verify()
        self.assertEqual(report["missing_assets"], [])
        self.assertEqual(report["abis"], ["arm64-v8a"])
        self.assertEqual(len(report["containers"]), 2)
        self.assertEqual(report["required_entries"], 2)

    def test_missing_font_fails(self):
        self.make_apk()
        self.entries.remove("fonts/fokufi.ttf")
        with self.assertRaisesRegex(ValueError, "required assets absent"):
            self.verify()

    def test_missing_ucas_fails(self):
        self.make_apk(ucas=False)
        with self.assertRaisesRegex(ValueError, "Missing .ucas"):
            self.verify()

    def test_missing_native_fails(self):
        self.make_apk(native=False)
        with self.assertRaisesRegex(ValueError, "Missing arm64"):
            self.verify()

    def test_wrong_app_fails(self):
        self.make_apk()
        self.package = "dz.wahran.zombies3d"
        with self.assertRaisesRegex(ValueError, "Wrong Android package"):
            self.verify()

    def test_failed_list_cannot_reuse_csv(self):
        self.make_apk()
        self.list_fail = True
        (self.args.output/"container_0.csv").write_text("old successful file")
        with self.assertRaisesRegex(RuntimeError, "UnrealPak failed"):
            self.verify()
        self.assertFalse((self.args.output/"container_0.csv").exists())

    def test_path_traversal_rejected(self):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("../escape.utoc", b"bad")
        buffer.seek(0)
        with zipfile.ZipFile(buffer) as archive, self.assertRaisesRegex(ValueError, "Unsafe"):
            checker.extract_containers(archive, self.root/"safe")

    def test_runtime_literal_asset_is_required(self):
        source = self.project/"Source/FallOfOran"
        source.mkdir(parents=True)
        (source/"test.cpp").write_text('LoadObject(nullptr, TEXT("/Game/Chars/Zombies/SK_z_war.SK_z_war"));')
        self.assertIn("chars/zombies/sk_z_war.uasset", checker.required_game_assets(self.project))


if __name__ == "__main__":
    unittest.main()
