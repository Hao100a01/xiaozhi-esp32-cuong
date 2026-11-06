import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]


class UtaRobotTests(unittest.TestCase):
    def test_location_portal(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("Cần Node.js để kiểm tra cài đặt vị trí")
        result = subprocess.run(
            [node, str(ROOT / "scripts/tests/uta_location_test.cjs")],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_embedded_web_actions(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("Cần Node.js để kiểm tra mã trang điều khiển")
        result = subprocess.run(
            [node, str(ROOT / "scripts/tests/uta_web_test.cjs")],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_gpio_and_url_boundaries(self):
        compiler = os.environ.get("UTA_TEST_CXX") or os.environ.get("CXX")
        if not compiler:
            for candidate in ("g++", "clang++", "xtensa-esp32s3-elf-g++"):
                compiler = shutil.which(candidate)
                if compiler:
                    break
        if not compiler:
            self.skipTest("Cần trình biên dịch C++23 để kiểm tra GPIO và địa chỉ mạng")
        result = subprocess.run(
            [compiler, "-std=c++23", "-fsyntax-only", "-I", str(ROOT),
             str(ROOT / "scripts/tests/cpp/uta_helpers_test.cc")],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_unique_identity_and_vietnamese_default(self):
        config = json.loads(
            (ROOT / "main/boards/UTA-robot/config.json").read_text(encoding="utf-8")
        )
        self.assertEqual(config["type"], "uta-robot")
        self.assertEqual(config["target"], "esp32s3")
        options = config["builds"][0]["sdkconfig_append"]
        kconfig = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
        self.assertIn("default LANGUAGE_VI_VN if BOARD_TYPE_UTA_ROBOT", kconfig)
        self.assertFalse(any(option.startswith("CONFIG_LANGUAGE_") for option in options))
        self.assertIn("CONFIG_BOARD_TYPE_UTA_ROBOT=y", options)
        owners = []
        for file in (ROOT / "main/boards").rglob("config.json"):
            board = json.loads(file.read_text(encoding="utf-8"))
            if board.get("type") == "uta-robot":
                owners.append(file.parent.name)
        self.assertEqual(owners, ["UTA-robot"])


if __name__ == "__main__":
    unittest.main()
