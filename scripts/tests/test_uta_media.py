"""Host regression tests for the real media validation and response parsers.

Does not invoke ESP-IDF or contact a media server.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class UtaMediaTests(unittest.TestCase):
    def test_media_inputs_song_response_and_http_range(self):
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Cần trình biên dịch C++ trên máy để chạy parser media")
        cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        if not (cjson / "cJSON.c").is_file():
            self.skipTest("Chưa có mã nguồn cJSON cho host test")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("media-test.exe" if os.name == "nt" else "media-test")
            result = subprocess.run(
                [compiler, "-std=c++23", "-I", str(ROOT), "-I", str(cjson),
                 str(ROOT / "scripts/tests/cpp/uta_media_test.cc"), str(cjson / "cJSON.c"),
                 "-o", str(executable)], capture_output=True, text=True,
                encoding="utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
