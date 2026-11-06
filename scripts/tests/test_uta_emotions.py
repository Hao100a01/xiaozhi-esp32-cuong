import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/UTA-robot"
ASSETS = BOARD / "assets/emotions"
spec = importlib.util.spec_from_file_location("uta_import_eyes", ROOT / "scripts/prepare_uta_emotions.py")
emotions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(emotions)


class UtaEmotionTests(unittest.TestCase):
    def test_all_original_animations_are_complete(self):
        manifest = json.loads((ASSETS / "manifest.json").read_text(encoding="utf-8"))
        catalog = emotions.catalog()
        self.assertEqual(len(catalog), 21)
        header = (BOARD / "uta_emotions.h").read_text(encoding="utf-8")
        enum = re.search(r'enum class EyeEmotion : unsigned \{(.*?)\};', header, re.S)[1]
        self.assertEqual([item.strip() for item in enum.split(",")],
                         [name for name, _, _ in catalog] + ["Count"])
        self.assertEqual(len(manifest["emotions"]), len(catalog))
        for record, (name, stem, filename) in zip(manifest["emotions"], catalog):
            data = (ASSETS / filename).read_bytes()
            parsed = emotions.inspect_gif(data)
            self.assertEqual(record, {"id": name, "file": filename, **parsed})
            self.assertEqual(stem + ".gif", filename)
            self.assertGreaterEqual(parsed["min_delay_ms"], 60)
            self.assertEqual(hashlib.sha256(data).hexdigest(), record["sha256"])

    def test_invalid_or_truncated_gifs_are_rejected(self):
        good = (ASSETS / "binhthuong.gif").read_bytes()
        for invalid in (b"", good[:6], good[:-1], good[:100], good + b"extra"):
            with self.subTest(length=len(invalid)), self.assertRaises(ValueError):
                emotions.inspect_gif(invalid)
        for offset, value in ((6, 0), (7, 0), (10, good[10] & 127)):
            corrupt = bytearray(good)
            corrupt[offset] = value
            if offset == 7:
                corrupt[6] = 0
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                emotions.inspect_gif(corrupt)

    def test_uta_only_asset_packaging(self):
        expected = {str(Path("boards/UTA-robot/assets/emotions") / filename).replace("\\", "/")
                    for _, _, filename in emotions.catalog()}
        cmake = (ASSETS / "emotions.cmake").read_text(encoding="utf-8")
        self.assertEqual(set(re.findall(r'"([^"]+\.gif)"', cmake)), expected)
        main = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        block = main.split('set(UTA_UI_EMBED_FILES "")', 1)[1].split("endif()", 1)[0]
        self.assertIn("if(CONFIG_BOARD_TYPE_UTA_ROBOT)", block)
        self.assertIn("assets/emotions/emotions.cmake", block)
        self.assertIn("list(APPEND UTA_EYE_GIFS", cmake)
        self.assertIn("IN LISTS UTA_EYE_GIFS", main)
        self.assertIn('--extra_files', main)

    def test_server_aliases_have_real_assets(self):
        names = {name for name, _, _ in emotions.catalog()}
        header = (BOARD / "uta_emotions.h").read_text(encoding="utf-8")
        aliases = dict(re.findall(r'\{"([^"]+)", EyeEmotion::(\w+)\}', header))
        expected = {"neutral", "listening", "happy", "laughing", "funny", "sad", "angry", "crying",
                    "loving", "embarrassed", "surprised", "shocked", "thinking", "winking", "cool",
                    "relaxed", "delicious", "kissy", "confident", "sleepy", "silly", "confused"}
        self.assertFalse(expected - aliases.keys())
        self.assertFalse(set(aliases.values()) - names)
        self.assertEqual(len(aliases), len(re.findall(r'\{"([^"]+)", EyeEmotion::', header)))

    def test_flash_budget_is_bounded(self):
        self.assertLess(sum((ASSETS / filename).stat().st_size for _, _, filename in emotions.catalog()),
                        1024 * 1024)

    def test_state_and_emotion_policy(self):
        compiler = os.environ.get("UTA_TEST_CXX") or os.environ.get("CXX")
        if not compiler:
            compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Cần trình biên dịch C++ trên máy để kiểm tra policy emotion")
        result = subprocess.run(
            [compiler, "-std=c++23", "-fsyntax-only", "-I", str(ROOT), "-I", str(ROOT / "main"),
             str(ROOT / "scripts/tests/cpp/uta_emotions_test.cc")],
            capture_output=True, text=True, encoding="utf-8", errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
