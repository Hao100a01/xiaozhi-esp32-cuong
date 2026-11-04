# UTA-robot

Board trợ lý giọng nói dùng ESP32-S3-N16R8: Flash 16 MB, PSRAM Octal 8 MB; mic INMP441, bộ khuếch đại MAX98357, màn hình TFT SPI ST7789 240 × 240. Không tích hợp camera.

Từ đánh thức mặc định: **Hi Lili** (tên trong ESP-SR: `Hi,Lily/Hi,莉莉`), dùng model `wn9_hilili_tts` cùng AFE. Biến thể `uta-robot` chọn model này và bỏ chọn model “你好小智”. Cấu hình có hiệu lực khi tạo và nạp firmware mới.

## Sơ đồ chân mặc định

| Thiết bị | Tên chân | GPIO |
|---|---|---:|
| Mic WS | mic_ws | 4 |
| Mic SCK | mic_sck | 5 |
| Mic SD | mic_din | 6 |
| Loa DIN | spk_dout | 7 |
| Loa BCLK | spk_bclk | 15 |
| Loa LRC | spk_lrck | 16 |
| Màn hình MOSI | d_mosi | 40 |
| Màn hình SCK | d_clk | 39 |
| Màn hình DC | d_dc | 42 |
| Màn hình RST | d_rst | 41 |
| Màn hình CS | d_cs | 2 |
| Đèn nền | d_blk | 1 |

Nối chung GND. Mic dùng nguồn 3,3 V, nối L/R xuống GND để chọn kênh trái. Cấp nguồn các module khác theo thông số của chúng.

## Cài đặt bằng trang web

1. Chọn board UTA-robot, biến thể uta-robot, biên dịch bằng ESP-IDF 6.1 (tối thiểu 6.0.1).
2. Kết nối Wi-Fi, xem địa chỉ http://<địa-chỉ-thiết-bị>:8080 trên màn hình. Tài khoản uta, mật khẩu riêng của thiết bị; nhấn BOOT ba lần để xem lại.
3. Mở Cài đặt GPIO và cảm ứng, đổi chân mic, loa, màn hình hoặc phụ kiện rồi lưu.
4. Tắt nguồn để đấu lại dây và bật lại. Cài đặt vẫn được giữ sau khi mất nguồn. GPIO và phụ kiện áp dụng khi khởi động lại.

Không cho phép trùng chân hoặc dùng chân khởi động, USB và bộ nhớ. Nhận GPIO 1–21 trừ 3, 19, 20 và GPIO 38–48 trừ 45, 46. Giá trị -1 chỉ dành cho đặt lại màn hình hoặc ngắt cảm ứng. Chân phụ kiện vẫn được kiểm tra khi tắt để tránh sơ đồ xung đột.

Nhấn BOOT sáu lần để khôi phục GPIO mặc định, tắt phụ kiện và khởi động lại; giữ cấu hình nhạc và mật khẩu. Khi nâng cấp từ sơ đồ phần cứng thử nghiệm cũ, thiết bị dùng chân mặc định mới; cần lưu lại GPIO trên web.

## Nhạc và đài Internet

Điền địa chỉ máy chủ nhạc và khóa tương thích dịch vụ IoT Việt Nam. Có tìm bài theo tên/ca sĩ, phát, tạm dừng, tiếp tục, dừng, chỉnh âm lượng và độ sáng. Để trống khóa hoặc mật khẩu mới để giữ giá trị đang lưu. Hỗ trợ các đài VOV và luồng đài tùy chỉnh HTTP/HTTPS; không có bộ thu FM.

Âm thanh MP3/AAC đi qua dịch vụ âm thanh chung. Yêu cầu phát nhạc có thể bị từ chối khi đang hội thoại; khả năng phát đài còn phụ thuộc mạng và định dạng nguồn.

URL máy chủ nhạc trên web có thể đổi sang máy chủ khác và được lưu khi mất nguồn. Máy chủ cần nhận `/stream_pcm?song=...&artist=...` và trả JSON có `audio_url`; thay URL không tự chuyển đổi giao thức API khác. Không thay firmware khi đổi địa chỉ máy chủ.

Phổ nhạc 16 dải khoảng 80 Hz–8 kHz hiển thị trên màn hình khi phát nhạc hoặc radio. Phổ lấy từ PCM giải mã, cập nhật tối đa 10 lần/giây, trở về mức không khi tạm dừng và ẩn khi ngừng phát/chuyển hội thoại. Không dùng dữ liệu mic hoặc cột chuyển động giả.

## Video MP4 có hình và tiếng

Nhập URL tệp trực tiếp ở mục Phát video MP4 trên web. Hình hiển thị trên ST7789, tiếng qua MAX98357; có tạm dừng, tiếp tục, dừng và ưu tiên hội thoại. Công cụ AI `self.video.play_mp4` nhận URL trực tiếp. Đường dẫn `audio_url` kết thúc bằng `.mp4` từ máy chủ nhạc cũng chuyển sang chế độ video.

Phạm vi triển khai: MP4 với H.264 Baseline hoặc MJPEG, tối đa 240×240 và 15 hình/giây; âm thanh AAC-LC hoặc MP3, 1–2 kênh, 8–48 kHz. Video tối đa một giờ, khung H.264 sau chuẩn hóa tối đa 128 KiB. Máy chủ cung cấp kích thước tệp và hỗ trợ HTTP Range để tìm bảng chỉ mục. Không nhận trang xem YouTube, H.265, DRM hoặc coi mọi tệp có đuôi MP4 là tương thích. Chưa có tua video. Phổ nhạc ẩn khi phát video để dành màn hình cho hình ảnh.

Mã dùng bộ tách `esp_extractor ~1.0.0` và `esp_video_codec ~0.5.7` của Espressif, chỉ thêm dependency cho UTA. Các dependency mới chưa được tải hoặc kiểm tra liên kết vì hiện chưa thực hiện bước biên dịch. Đồng bộ hình/tiếng dùng mốc thời gian khung; chưa đo độ lệch hoặc tốc độ thực tế trên thiết bị.

## Hai động cơ N20 và hai servo 180 độ

Chọn Tắt / L298N / DRV8833 ở trang cài đặt; động cơ và servo mặc định tắt. Không bật động cơ trực tiếp đồng thời với UART. Có tiến, lùi, quay trái, quay phải, dừng; tốc độ 1–100% và đảo chiều từng bánh. Lệnh có thời hạn tối đa hai giây, tự dừng khi hết hạn, mất Wi-Fi, mở cấu hình mạng hoặc khởi động lại. Nhảy múa và hiệu ứng đèn chỉ có ở phụ kiện UART; chưa tạo động tác nhảy cho hai N20.

| Chức năng | GPIO mặc định | L298N | DRV8833 |
|---|---:|---|---|
| dc_trai_1 | 8 | IN1 | AIN1 |
| dc_trai_2 | 9 | IN2 | AIN2 |
| dc_phai_1 | 10 | IN3 | BIN1 |
| dc_phai_2 | 14 | IN4 | BIN2 |
| dc_trai_en | 17 | ENA | Không nối |
| dc_phai_en | 18 | ENB | Không nối |
| servo_dau | 21 | Tín hiệu servo đầu | Tín hiệu servo đầu |
| servo_co | 43 | Tín hiệu servo cổ | Tín hiệu servo cổ |

Các chân đều đổi được qua web. L298N cần tháo cầu nối ENA/ENB để điều tốc bằng PWM. DRV8833 cần đưa nSLEEP lên mức bật theo module. Động cơ nối đầu ra công suất driver, dùng nguồn phù hợp điện áp/dòng của N20. Hai servo dùng nguồn ngoài phù hợp và nối chung GND với ESP32; không cấp nguồn servo từ GPIO.

Hai servo dùng xung 50 Hz, 500–2500 µs ứng với 0–180°. Web cho phép đặt góc nhỏ nhất, lớn nhất và góc giữa riêng từng trục; mặc định 15°, 165°, 90°. Sau khi bật/khởi động lại, dùng thanh trượt đầu/cổ hoặc nút về giữa; công cụ AI `self.robot.set_head` điều khiển hai góc. Không lưu góc di chuyển tức thời vào bộ nhớ. Giới hạn góc và cấu hình phần cứng áp dụng sau khởi động lại, góc giữa được đặt khi bật thiết bị. Kênh PWM động cơ và servo tách khỏi đèn nền.

## Phụ kiện tùy chọn

Cảm ứng XPT2046 mặc định tắt, dùng chung MOSI/SCK màn hình; mặc định MISO 11, CS 12, IRQ 13. Vuốt ngang đổi độ sáng, vuốt dọc đổi âm lượng, giữ 0,8 giây phát/tạm dừng, chạm đôi dừng nhạc và chuyển hội thoại. ST7789 thông thường không tự có cảm ứng.

Điều khiển robot UART mặc định tắt: TX 38, RX 48, tốc độ 115200. Cần bộ điều khiển ngoài tương thích lệnh x/y, d1 và w2–w7. Sau khi bật và khởi động lại, trang web và công cụ robot hỗ trợ tiến, lùi, trái, phải, dừng, nhảy và sáu hiệu ứng đèn. Hành động thực tế do bộ điều khiển ngoài thực hiện, không phải điều khiển trực tiếp động cơ từ ESP32.

BOOT một lần dừng nhạc và chuyển hội thoại; hai lần phát/tạm dừng; giữ lâu cấu hình lại Wi-Fi.

## Biên dịch và kiểm tra

Chạy: python scripts/build.py UTA-robot --name uta-robot --language vi-VN

Kiểm tra riêng board: python -m unittest discover -s scripts/tests -p test_uta_robot.py -v

Giao diện riêng của board, thông báo và chú thích dùng tiếng Việt. Cần thử phần cứng thật: mic kênh trái, loa, màn hình, đèn nền, đổi GPIO rồi mất nguồn/bật lại, nhạc/đài và phụ kiện nếu có. Biên dịch thành công không thay thế thử phần cứng.
