"""Exercise the headless runner's failure handling without pretending to test UE."""
import importlib.util
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "fo_tests",
    Path(__file__).resolve().parents[1] / "run_headless_tests.py",
)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def run_mock(self, body, *, level=0, mode="selftest", timeout=5):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            project = root / "FallOfOran.uproject"
            project.write_text("{}")
            mock = root / "mock_editor"
            mock.write_text("#!/usr/bin/env python3\n" + body)
            mock.chmod(0o700)
            return runner.run_level(mock, project, level, mode, root, timeout)

    def markers(self, level=0):
        return (
            "GameInstance: 3 levels\nBuilding level\nMission: stage 1/2\n"
            f"SELFTEST PASS: level {level + 1} won\nSELFTEST EXIT OVERLAP:\n"
            "SELFTEST RESTART:\nSELFTEST HUD CLEANUP PASS:"
        )

    def test_pass_requires_exit_and_markers(self):
        result = self.run_mock(f"print({self.markers()!r})\n")
        self.assertTrue(result["passed"])

    def test_clean_exit_without_pass_is_failure(self):
        result = self.run_mock("print('startup only')\n")
        self.assertFalse(result["passed"])
        self.assertIn("SELFTEST PASS: level 1 won", result["missing_markers"])

    def test_crash_after_pass_is_failure(self):
        result = self.run_mock(f"print({self.markers()!r})\nraise SystemExit(3)\n")
        self.assertFalse(result["passed"])
        self.assertEqual(result["exit_code"], 3)

    def test_error_marker_even_with_zero_exit_is_failure(self):
        result = self.run_mock(f"print({self.markers()!r})\nprint('SELFTEST FAIL: broken')\n")
        self.assertFalse(result["passed"])
        self.assertTrue(result["critical_errors"])

    def test_missing_puzzle_is_failure(self):
        result = self.run_mock(f"print({self.markers(1)!r})\n", level=1)
        self.assertFalse(result["passed"])
        self.assertIn("Puzzle 'breakers' spawned", result["missing_markers"])

    def test_timeout_terminates_only_owned_process(self):
        result = self.run_mock("import time\ntime.sleep(30)\n", timeout=1)
        self.assertTrue(result["timed_out"])
        self.assertFalse(result["passed"])
        self.assertIsNotNone(result["exit_code"])

    def test_stale_engine_log_cannot_pass_a_new_failed_launch(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            project = root / "FallOfOran.uproject"
            project.write_text("{}")
            mock = root / "mock_editor"
            mock.write_text("#!/usr/bin/env python3\nprint('startup only')\n")
            mock.chmod(0o700)
            (root / "selftest_L0.engine.log").write_text(self.markers())
            result = runner.run_level(mock, project, 0, "selftest", root, 5)
            self.assertFalse(result["passed"])
            self.assertIn("SELFTEST PASS: level 1 won", result["missing_markers"])


if __name__ == "__main__":
    unittest.main()
