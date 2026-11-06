"""Optional host test of the real weather JSON parser; never invokes ESP-IDF."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class UtaWeatherTests(unittest.TestCase):
    def test_weather_response_validation(self):
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Cần trình biên dịch C++ trên máy để chạy parser thời tiết")
        cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        if not (cjson / "cJSON.c").is_file():
            self.skipTest("Chưa có mã nguồn cJSON cho host test")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("weather-test.exe" if os.name == "nt" else "weather-test")
            result = subprocess.run(
                [compiler, "-std=c++23", "-I", str(ROOT), "-I", str(cjson),
                 str(ROOT / "scripts/tests/cpp/uta_weather_test.cc"), str(cjson / "cJSON.c"),
                 "-o", str(executable)], capture_output=True, text=True,
                encoding="utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
