"""Render UTA previews directly from the embedded RGB565, icon and glyph data.

No ESP-IDF, compiler, Pillow or Internet required. Values are illustrative.
"""
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "main/boards/UTA-robot/assets"


def png(path, pixels, width=240, height=240):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    raw = b"".join(b"\0" + bytes(pixels[y * width * 3:(y + 1) * width * 3]) for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def background(name):
    result = bytearray()
    for (value,) in struct.iter_unpack("<H", (ASSETS / name).read_bytes()):
        result.extend((((value >> 11) & 31) * 255 // 31,
                       ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
    return result


def blend(pixels, x, y, color, alpha=255):
    if not (0 <= x < 240 and 0 <= y < 240):
        raise ValueError(f"Pixel outside screen: {x}, {y}")
    offset = (y * 240 + x) * 3
    for index, value in enumerate(color):
        pixels[offset + index] = (value * alpha + pixels[offset + index] * (255 - alpha)) // 255


def font(name):
    source = (ASSETS / f"{name}.inc").read_text(encoding="utf-8")
    bitmap = bytes(int(x, 16) for x in re.search(r"_bitmap\[\]=\{(.*?)\};", source, re.S)[1].replace("\n", "").split(",") if x.strip())
    glyphs = [tuple(map(int, fields)) for fields in re.findall(
        r"\.bitmap_index=(\d+), .adv_w=(\d+), .box_w=(\d+), .box_h=(\d+), .ofs_x=(-?\d+), .ofs_y=(-?\d+)", source)]
    codes = [int(x) + 32 for x in re.search(r"_unicode\[\]=\{(.*?)\}", source)[1].split(",")]
    assert len(codes) == len(glyphs)
    height, baseline = map(int, re.search(r"\.line_height=(\d+),.base_line=(\d+)", source).groups())
    return bitmap, dict(zip(codes, glyphs)), height, baseline


def text(pixels, content, x, y, color, name="uta_font_small"):
    bitmap, glyphs, height, baseline = font(name)
    pen = x * 16
    for char in content:
        offset, advance, width, box_height, dx, dy = glyphs[ord(char)]
        for row in range(box_height):
            for col in range(width):
                pos = row * width + col
                alpha = ((bitmap[offset + pos // 2] >> (0 if pos % 2 else 4)) & 15) * 17
                if alpha:
                    blend(pixels, pen // 16 + dx + col, y + height - baseline - box_height - dy + row, color, alpha)
        pen += advance
    return (pen + 15) // 16 - x


def icon(pixels, index, x, y, size):
    data = (ASSETS / "uta_icons.argb8888").read_bytes()
    for row in range(size):
        for col in range(size):
            offset = (index * 1024 + (row * 32 // size) * 32 + col * 32 // size) * 4
            blue, green, red, alpha = data[offset:offset + 4]
            blend(pixels, x + col, y + row, (red, green, blue), alpha)


def standby(online=True, available=True, condition="Ít mây", weather_icon=3):
    pixels = background("uta_neon.rgb565")
    text(pixels, "ATFUTA", 16, 16, (152, 179, 200))
    icon(pixels, 9 if online else 10, 171, 14, 18)
    text(pixels, "Wi-Fi" if online else "Offline", 191, 16, (100, 223, 197) if online else (139, 155, 170))
    text(pixels, "08:24" if online else "--:--", 13, 59, (110, 232, 239), "uta_font_clock")
    text(pixels, "Thứ Năm · 05/11/2026" if online else "Đang đồng bộ giờ", 16, 129, (157, 176, 194))
    for x in range(16, 224):
        blend(pixels, x, 158, (35, 56, 72))
    icon(pixels, 11, 16, 176, 15)
    text(pixels, "TP. Hồ Chí Minh" if available else "Chưa có vị trí", 36, 176, (238, 248, 255))
    icon(pixels, weather_icon if available else 0, 15, 199, 26)
    text(pixels, condition if available else "Chưa cập nhật", 47, 205, (157, 176, 194))
    sample = bytearray(240 * 240 * 3)
    value = "28°" if available else "--°"
    width = text(sample, value, 0, 0, (238, 248, 255), "uta_font_temperature")
    text(pixels, value, 238 - width, 193, (238, 248, 255), "uta_font_temperature")
    return pixels


def main():
    boot = background("uta_boot.rgb565")
    sample = bytearray(240 * 240 * 3)
    width = text(sample, "ĐANG KHỞI ĐỘNG", 0, 0, (48, 56, 60))
    text(boot, "ĐANG KHỞI ĐỘNG", (240 - width) // 2, 184, (48, 56, 60))
    for y in (216, 217):
        for x in range(88, 152):
            blend(boot, x, y, (48, 56, 60))
    png(ASSETS / "preview-boot.png", boot)
    png(ASSETS / "preview-neon.png", standby())
    png(ASSETS / "preview-no-data.png", standby(False, False))
    png(ASSETS / "preview-rain.png", standby(condition="Có mưa", weather_icon=6))
    print("Rendered boot, Neon, rain and no-data previews from embedded assets.")


if __name__ == "__main__":
    main()
