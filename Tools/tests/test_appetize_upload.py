"""Network-mocked upload safety checks; no Appetize session or upload occurs."""
from contextlib import redirect_stdout, redirect_stderr
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / "appetize_upload.py"
spec = importlib.util.spec_from_file_location("appetize_uploader", SOURCE)
uploader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uploader)


class UploadTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.apk = self.root / "test.apk"
        self.apk.write_bytes(b"mock apk")
        self.verification = self.root / "verification.json"
        self.result = self.root / "appetize_upload.json"
        self.report = {
            "passed": True, "package": uploader.PACKAGE, "missing_assets": [],
            "required_entries": 2, "sha256": hashlib.sha256(self.apk.read_bytes()).hexdigest(),
        }
        self.write_report()
        self.remote = {"platform": "android", "bundle": uploader.PACKAGE,
                       "privateKey": "never-persist-this-secret"}

    def write_report(self):
        self.verification.write_text(json.dumps(self.report))

    def run_upload(self, key="", remote=None):
        with patch.object(uploader, "fetch_app", return_value=remote or self.remote), \
                patch.object(uploader, "upload_file", return_value={
                    "publicKey": key or "b_testfalloforan", "privateKey": "never-persist-this-secret",
                }) as upload, redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            code = uploader.upload_verified(
                self.apk, self.verification, "test-token", "test", key, self.result)
        return code, upload

    def test_success_records_only_safe_metadata(self):
        code, upload = self.run_upload()
        self.assertEqual(code, 0)
        upload.assert_called_once()
        data = self.result.read_text()
        self.assertNotIn("never-persist-this-secret", data)
        self.assertNotIn("test-token", data)
        self.assertFalse(json.loads(data)["device_tested"])
        self.assertTrue(json.loads(data)["remote_bundle_verified"])

    def test_changed_apk_fails_before_upload(self):
        self.apk.write_bytes(b"changed")
        with patch.object(uploader, "upload_file") as upload, \
                self.assertRaisesRegex(ValueError, "changed after verification"):
            uploader.upload_verified(
                self.apk, self.verification, "test-token", "test", "", self.result)
        upload.assert_not_called()

    def test_failed_content_report_is_rejected(self):
        self.report["passed"] = False
        self.write_report()
        with self.assertRaisesRegex(ValueError, "did not pass"):
            self.run_upload()

    def test_wrong_existing_app_refused(self):
        with patch.object(uploader, "fetch_app", return_value={
                "platform": "android", "bundle": "other.game"}), \
                patch.object(uploader, "upload_file") as upload, \
                self.assertRaisesRegex(ValueError, "refusing update"):
            uploader.upload_verified(
                self.apk, self.verification, "test-token", "test", "b_existingapp", self.result)
        upload.assert_not_called()

    def test_http_error_never_reveals_body(self):
        response = type("Response", (), {"status": 403, "read": lambda _, size: b"private-secret"})()
        with self.assertRaisesRegex(RuntimeError, "^Appetize HTTP 403;"):
            uploader.decode_response(response)

    def test_followup_failure_retains_public_id_without_duplicate(self):
        with patch.object(uploader, "fetch_app", side_effect=RuntimeError("not ready")), \
                patch.object(uploader, "upload_file", return_value={"publicKey": "b_createdapp"}) as upload, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            code = uploader.upload_verified(
                self.apk, self.verification, "test-token", "test", "", self.result)
        self.assertEqual(code, 5)
        upload.assert_called_once()
        result = json.loads(self.result.read_text())
        self.assertEqual(result["public_key"], "b_createdapp")
        self.assertFalse(result["remote_bundle_verified"])

    def test_missing_token_rejected(self):
        with self.assertRaisesRegex(ValueError, "ask the owner"):
            uploader.upload_verified(self.apk, self.verification, "", "", "", self.result)


if __name__ == "__main__":
    unittest.main()
