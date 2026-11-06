import importlib.util
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "main/boards/UTA-robot/assets"
spec = importlib.util.spec_from_file_location("uta_preview", ROOT / "scripts/preview_uta_ui.py")
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


class UtaUiAssetsTests(unittest.TestCase):
    def test_embedded_binary_dimensions(self):
        for name in ("uta_boot.rgb565", "uta_neon.rgb565"):
            self.assertEqual((ASSETS / name).stat().st_size, 240 * 240 * 2)
        self.assertEqual((ASSETS / "uta_icons.argb8888").stat().st_size, 12 * 32 * 32 * 4)

    def test_preview_sizes_and_png_files(self):
        for name in ("uta_boot", "uta_neon", "preview-boot", "preview-neon", "preview-no-data", "preview-rain"):
            image = (ASSETS / f"{name}.png").read_bytes()
            self.assertEqual(image[:8], b"\x89PNG\r\n\x1a\n")
            self.assertEqual(struct.unpack(">II", image[16:24]), (240, 240))

    def test_transparent_icons_have_visible_pixels(self):
        data = (ASSETS / "uta_icons.argb8888").read_bytes()
        for index in range(12):
            alpha = data[index * 4096 + 3:(index + 1) * 4096:4]
            self.assertGreater(max(alpha), 0)
            self.assertEqual(min(alpha), 0)

    def test_vietnamese_glyphs_and_bitmap_bounds(self):
        text = (ROOT / "main/boards/UTA-robot/uta_provinces.h").read_text(encoding="utf-8")
        names = "".join(re.findall(r'\{"\d+", "([^"]+)"', text))
        phrases = "ĐANG KHỞI ĐỘNG Đang đồng bộ giờ Chưa có vị trí Chưa cập nhật Dữ liệu cũ Thứ Năm Chủ Nhật Có mưa Sương mù Đêm quang Mưa phùn Nhiều mây"
        bitmap, glyphs, _, _ = preview.font("uta_font_small")
        self.assertGreater(glyphs[ord(" ")][1], 0, "Khoảng trắng phải có độ rộng")
        self.assertFalse(set(map(ord, names + phrases)) - glyphs.keys())
        for name in ("uta_font_small", "uta_font_clock", "uta_font_temperature"):
            bitmap, glyphs, _, _ = preview.font(name)
            for offset, advance, width, height, _, _ in glyphs.values():
                self.assertLessEqual(offset + (width * height + 1) // 2, len(bitmap))
                self.assertGreaterEqual(advance, 0)

    def test_render_online_offline_and_rain_without_overflow(self):
        for args in ({}, {"online": False, "available": False}, {"condition": "Có mưa", "weather_icon": 6}):
            self.assertEqual(len(preview.standby(**args)), 240 * 240 * 3)

    def test_cmake_embeds_existing_files_for_uta_only(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        block = cmake.split('set(UTA_UI_EMBED_FILES "")', 1)[1].split("endif()", 1)[0]
        self.assertIn("if(CONFIG_BOARD_TYPE_UTA_ROBOT)", block)
        paths = re.findall(r'"(boards/UTA-robot/assets/[^"]+)"', block)
        self.assertEqual(len(paths), 3)
        for path in paths:
            self.assertTrue((ROOT / "main" / path).is_file())
        self.assertIn("${UTA_UI_EMBED_FILES}", cmake.split("idf_component_register(", 1)[1])

    def test_flash_data_budget(self):
        data_size = sum((ASSETS / name).stat().st_size for name in
                        ("uta_boot.rgb565", "uta_neon.rgb565", "uta_icons.argb8888"))
        for font in ("uta_font_small", "uta_font_clock", "uta_font_temperature"):
            bitmap, glyphs, _, _ = preview.font(font)
            data_size += len(bitmap) + len(glyphs) * 20
        self.assertLess(data_size, 350 * 1024)


if __name__ == "__main__":
    unittest.main()
