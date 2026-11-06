# Tài nguyên giao diện ATFUTA / UTA-robot

- [`emotions/`](emotions/README.md): 21 GIF đôi mắt gốc đã nhúng cho lúc lắng
  nghe và giao tiếp; có bảng xem trước và manifest để kiểm tra toàn bộ tài nguyên.

- `atfuta-original.png`: ảnh gốc do người dùng cung cấp, lưu ngay trong repo để
  không phụ thuộc đường dẫn Desktop. Không nhúng ảnh gốc dung lượng lớn vào firmware.
- `uta_boot.png`, `uta_boot.rgb565`: logo trên khung trắng 240×240, giữ tỷ lệ ảnh.
- `uta_neon.png`, `uta_neon.rgb565`: nền Neon 240×240, không chứa dữ liệu giờ/thời tiết giả.
- `icon-0.png`–`icon-11.png`, `uta_icons.argb8888`: chưa có dữ liệu, nắng, đêm,
  ít mây, nhiều mây, sương mù, mưa, tuyết, dông, Wi-Fi, Offline, vị trí.
  Mỗi biểu tượng 32×32; bitmap có alpha, thứ tự trùng với `WeatherIcon` và `uta_ui_assets.h`.
- `uta_font_small.inc`, `uta_font_clock.inc`, `uta_font_temperature.inc`:
  glyph 4 bpp cho chữ nhỏ tiếng Việt, đồng hồ 54 px và nhiệt độ 30 px.
  Đây là glyph raster, không kèm tệp chương trình font. Chữ nhỏ raster bằng
  Arial, đồng hồ bằng Segoe UI Light có sẵn trên máy Windows chuẩn bị tài nguyên.
- `preview-boot.png`, `preview-neon.png`, `preview-no-data.png`, `preview-rain.png`:
  bản xem trước 240×240; giờ, địa phương, nhiệt độ là ví dụ để duyệt giao diện.
  `preview-icons.png` là bảng biểu tượng. Tệp preview không được nhúng vào firmware.

RGB565 lưu little endian, 2 byte/pixel, 115.200 byte mỗi nền; icon ARGB8888 lưu
BGRA little endian, 4 byte/pixel, tổng 49.152 byte. Các descriptor LVGL nằm trong
`uta_ui_assets.cc`; dữ liệu nằm ở flash, không cấp thêm framebuffer 240×240 cho
màn hình chờ. Cần kiểm tra tổng dung lượng firmware và màu thực tế khi build/nạp sau.

Tạo lại bằng PowerShell 7 trên Windows (chỉ chuẩn bị ảnh, không build firmware):

```powershell
./scripts/prepare_uta_ui_assets.ps1
python scripts/preview_uta_ui.py
```

Đổi ảnh gốc bằng tham số `-LogoSource <đường-dẫn-ảnh>`. Script ghi tài nguyên
riêng của board trong thư mục này; không sửa thư mục vendor hoặc build.
Script Python dựng preview trực tiếp từ RGB565, icon và glyph nhúng, không cần
Pillow hoặc Internet. Các icon/nền là đồ họa tạo bằng mã, không phụ thuộc CDN.
