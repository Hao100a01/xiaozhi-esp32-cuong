# UTA-robot

Board trợ lý giọng nói dùng ESP32-S3-N16R8: Flash 16 MB, PSRAM Octal 8 MB; mic INMP441, bộ khuếch đại MAX98357, màn hình TFT SPI ST7789 240 × 240. Không tích hợp camera.

Từ đánh thức mặc định: **Hi Lili** (tên trong ESP-SR: `Hi,Lily/Hi,莉莉`), dùng model `wn9_hilili_tts` cùng AFE. Biến thể `uta-robot` chọn model này và bỏ chọn model “你好小智”. Cấu hình có hiệu lực khi tạo và nạp firmware mới.

Biến thể khai báo `wake_word` trong `config.json` để script build không thay model theo ngôn ngữ. Tham số `--wake-word` vẫn được ưu tiên nếu chỉ định. Trong `idf.py menuconfig`, cách xử lý AFE nằm ở **Xiaozhi Assistant → Wake Word Implementation Type**; câu đánh thức nằm ở **ESP Speech Recognition → WakeNet**. Với cấu hình cũ, bỏ chọn “你好小智” rồi chọn “Hi,Lily/Hi,莉莉”, lưu và thoát. Mở menuconfig trực tiếp không tự áp dụng `config.json` vào cấu hình đã lưu.

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
2. Kết nối Wi-Fi, xem địa chỉ http://<địa-chỉ-thiết-bị>:8080 trên màn hình. Mở trực tiếp, không cần tài khoản hoặc mật khẩu; nhấn BOOT ba lần để xem lại địa chỉ.
3. Mở Cài đặt GPIO và cảm ứng, đổi chân mic, loa, màn hình hoặc phụ kiện rồi lưu.
4. Tắt nguồn để đấu lại dây và bật lại. Cài đặt vẫn được giữ sau khi mất nguồn. GPIO và phụ kiện áp dụng khi khởi động lại.

Không cho phép trùng chân hoặc dùng chân khởi động, USB và bộ nhớ. Nhận GPIO 1–21 trừ 3, 19, 20 và GPIO 38–48 trừ 45, 46. Giá trị -1 chỉ dành cho đặt lại màn hình hoặc ngắt cảm ứng. Chân phụ kiện vẫn được kiểm tra khi tắt để tránh sơ đồ xung đột.

Nhấn BOOT sáu lần để khôi phục GPIO mặc định, tắt phụ kiện và khởi động lại; giữ cấu hình nhạc. Khi nâng cấp từ sơ đồ phần cứng thử nghiệm cũ, thiết bị dùng chân mặc định mới; cần lưu lại GPIO trên web.

## Vị trí cho màn hình chờ và thời tiết

Địa chỉ IP thiết bị lấy qua MCP **`self.get_device_status` → `network.ip`**.
Đây là IPv4 nội bộ đọc trực tiếp từ interface Wi-Fi STA đang hoạt động, không
phải IP Internet dùng định vị. Khi mất kết nối, interface chưa lên hoặc chưa
nhận DHCP, `network.ip` là `null`, không dùng địa chỉ đã lưu cũ. Trong chế độ
cấu hình, IP hotspot nằm riêng ở `network.ap_ip`; không coi địa chỉ AP là IP
STA. `self.get_system_info` cũng dùng nguồn IP hiện tại ở `board.ip` khi có Wi-Fi.

Ví dụ `192.168.1.42` chỉ là địa chỉ minh họa; IP thật do mạng đang kết nối cấp.
Địa chỉ web `:8080` trên màn hình/nhấn BOOT ba lần dùng cùng nguồn dữ liệu và
chọn đúng STA/AP theo chế độ, không tự rơi về AP khi STA đang thiếu IP.

Trong chế độ cấu hình Wi-Fi, mở `http://192.168.4.1`, vào **Tùy chọn nâng cao →
Vị trí và thời tiết**, chọn tỉnh/thành rồi nhấn **Lưu vị trí**. Form vị trí có nút
lưu riêng, không thay đổi cấu hình Wi-Fi/OTA. Có 34 tỉnh/thành theo
[Quyết định 19/2025/QĐ-TTg](https://baochinhphu.vn/bang-danh-muc-va-ma-so-cua-34-tinh-thanh-moi-3321-don-vi-hanh-chinh-cap-xa-moi-102250704153652947.htm).
Lựa chọn được lưu ở NVS `uta_location/province`, giữ sau khi mất nguồn, không
thay đổi các khóa cấu hình cũ. Tọa độ là điểm đại diện của địa phương, không phải GPS.

Mặc định **Tự động qua IP**. Khi chưa có tỉnh/thành hợp lệ được lưu, tác vụ nền
gọi [ipwho.is qua HTTPS](https://ipwhois.io/documentation) sau khi có Internet.
Không gọi định vị khi chỉ đang phát điểm truy cập cấu hình. Vị trí IP chỉ gần
đúng và có thể lệch do nhà mạng/VPN; không ghi kết quả IP đè lên tỉnh đã chọn.
Kết quả IP giữ trong RAM, cập nhật sau 6 giờ hoặc kết nối lại; lỗi thử lại sau
5 phút. Không có dữ liệu thì `LocationService::Get().available` là false, màn
hình phải hiển thị chưa có vị trí thay vì địa phương giả. Dịch vụ định vị có thể
không khả dụng; việc chọn tỉnh thủ công vẫn hoạt động khi không có Internet.

Thời tiết dùng [Open-Meteo](https://open-meteo.com/en/docs), HTTPS, đơn vị °C,
các trường `current.temperature_2m`, `weather_code`, `is_day` và `time`.
Tác vụ riêng cập nhật 15 phút/lần khi có mạng và robot rảnh; lỗi thử lại sau
5 phút. Phản hồi giới hạn 4 KiB, thời gian đọc hữu hạn, kiểm tra đơn vị, số hữu
hạn, mã thời tiết và độ sâu JSON. Đổi vị trí sẽ bỏ dữ liệu của nơi cũ; kết quả
đang tải không được ghi đè vị trí mới. Dữ liệu giữ trong RAM; mất mạng hoặc quá
một giờ chưa cập nhật thì hiện **Dữ liệu cũ**, chưa từng có dữ liệu thì hiện
**--° / Chưa cập nhật**. Không dùng thời tiết giả trong firmware. Open-Meteo
cung cấp dữ liệu thời tiết theo mô hình, không phải cảm biến nhiệt độ trong phòng.

## Logo ATFUTA và màn hình chờ Neon 240×240

`UtaDisplay::SetupUI()` tạo màn hình khởi động từ ảnh ATFUTA gốc do người dùng
cung cấp, hiển thị tối đa 2,2 giây mà không chặn khởi động âm thanh/mạng. Cấu hình
Wi-Fi, kích hoạt và lỗi được ưu tiên. Màn hình chờ Neon hiển thị khi idle, không
phát media; tự ẩn khi lắng nghe, trả lời, phát nhạc/video, thông báo, xem ảnh hoặc
cảnh báo pin. Khi giao tiếp dùng bộ [21 GIF đôi mắt](assets/emotions/README.md):
lắng nghe dùng `binhthuong`, kết nối dùng `suynghi2`, lúc trả lời theo emotion
server (vui, buồn, giận, ngạc nhiên...). Giữ nguyên ảnh động gốc 240×240, có
caption trạng thái và phụ đề ngắn; kết thúc hội thoại trở về Neon.

Màn hình chờ có HH:MM, thứ/ngày/tháng/năm, tỉnh/thành, biểu tượng thời tiết,
nhiệt độ °C và Wi-Fi/Offline. Thời gian lấy từ đồng hồ đã đồng bộ của hệ thống;
không tự cộng UTC+7 lần nữa vì luồng OTA hiện có đã xử lý offset. Chưa đồng bộ
thì hiện `--:--` và **Đang đồng bộ giờ**. Vị trí dài dùng một dòng có dấu chấm
lửng. Các nhãn cập nhật tối đa một lần/giây, kiểm tra chuyển chế độ mỗi 100 ms.

Tất cả ảnh, biểu tượng và glyph tiếng Việt được đóng gói trong
[`assets/`](assets/README.md), không tải ảnh qua mạng. Chỉ UTA-robot nhúng tài
nguyên này qua CMake. Bản xem trước 240×240 trong thư mục assets dùng dữ liệu
minh họa và dựng trực tiếp từ ảnh/font nhúng.

Chưa build hoặc nạp firmware ở bước này theo yêu cầu. Cần kiểm tra trên mạch:
logo và màu RGB565, chữ tiếng Việt, chuyển idle/hội thoại/media/thông báo,
vào lại AP nhiều lần, lưu/đọc tỉnh sau mất nguồn, định vị IP, lỗi Internet,
thời tiết đổi địa phương và dung lượng phân vùng firmware.

## Nhạc và đài Internet

VOV1 dùng URL gốc `https://audio-lss.vov.vn/live/vov1.m3u8`. Firmware đọc master
playlist để nhận URL CDN theo phiên, đọc media playlist theo số thứ tự và tách
AAC ADTS từ MPEG-TS 188 byte. URL VOV1 cũ `stream.vovmedia.vn/vov-1` được
chuyển sang nguồn mới khi phát, kể cả cài đặt đã lưu trước đây. Các preset đài
khác cần kiểm chứng nguồn riêng. URL CDN có chữ ký được giữ trong RAM.

Phạm vi HLS hiện tại là radio MPEG-TS/AAC không mã hóa. Playlist tối đa 64 KiB,
128 đoạn, mỗi đoạn tối đa 512 KiB, thời gian chờ có giới hạn. Không hỗ trợ HLS
fMP4, AES/DRM, byte-range hoặc delta playlist. Khi hết phiên, mất đoạn hay đổi
mốc âm thanh, luồng radio kết nối lại từ URL gốc trong giới hạn thử lại hiện có.
Thử nghiệm parser và tách ADTS từ đoạn VOV1 thật đã qua trên máy tính. Phát loa,
TLS trên ESP32, chuyển đoạn liên tục và pause/reconnect cần kiểm tra trên mạch.

Các MCP media của UTA được đăng ký tại `uta_media_mcp.cc`, chỉ dùng cho board
này; giữ nguyên các tên lệnh cũ. Kết quả lệnh phát là **đã nhận yêu cầu**, không
đồng nghĩa đã giải mã/phát thành công. Xem `self.media.get_status` để biết tên
nguồn, loại media, đang chuẩn bị/phát/tạm dừng và lỗi chi tiết. Trạng thái không
trả khóa máy chủ hoặc URL đã ký.

| MCP | Tham số / tác dụng |
|---|---|
| `self.music.play_song` | `bai_hat`, `ca_si` tùy chọn; mỗi trường tối đa 160 byte |
| `self.radio.list_stations` | Liệt kê mã/tên đài có sẵn và đài tùy chỉnh đã lưu |
| `self.radio.play_station` | `ma_dai` nhận mã hoặc tên đài, ví dụ `VOV 3`, `Tây Nguyên`, `VOV Mekong`; `tuy_chinh` phát đài đã lưu |
| `self.radio.play_url` | `url` luồng MP3/AAC trực tiếp, `ten` tùy chọn |
| `self.video.play_mp4` | `url` tệp MP4 trực tiếp; URL tối đa 2048 byte |
| `self.media.get_status` | Xem kết quả phát và lỗi bất đồng bộ |
| `self.media.pause` / `self.media.resume` | Tạm dừng/tiếp tục nguồn còn mở, chưa bị hội thoại ngắt |
| `self.media.replay` | Nhạc/MP4 mở lại từ đầu; radio nối lại tại thời điểm trực tiếp |
| `self.media.stop` / `self.music.stop` | Dừng cả nhạc/radio/MP4, hủy yêu cầu đang chuẩn bị |

Đánh thức để hội thoại sẽ ngắt media nhằm ưu tiên giọng nói. Khi đó không có
cam kết tiếp tục tại vị trí cũ: cần dùng **phát lại**. Không tự phát lại nguồn
cũ sau khi hội thoại kết thúc. MP4 vẫn chưa có tua hoặc lưu vị trí xem. Tạm dừng
có thể còn một đoạn PCM ngắn đã nằm trong hàng đợi phát ra loa.

Lệnh trống, ký tự điều khiển, URL không hợp lệ, máy chủ nhạc chưa cài, thiết bị
chưa có đầu ra video hoặc đang chuẩn bị nguồn khác trả lỗi cụ thể. Dừng sẽ giải
phóng trạng thái chuẩn bị ngay; callback cũ không được xóa trạng thái của yêu
cầu mới. Hàng chờ giữ tối đa một nguồn, và chỉ giữ một nguồn gần nhất trong RAM
để phát lại. Lỗi mở mạng/đọc radio thử lại tối đa hai lần, không lặp lỗi codec
hoặc HTTP 4xx. Mọi I/O/giải mã nằm ở tác vụ media, không chạy trong callback MCP.

Phản hồi tìm bài giới hạn 8192 byte, có hạn thời gian đọc và kiểm tra JSON đầy
đủ, độ sâu, NUL, `audio_url`. Không ghi đè lỗi cụ thể bằng thông báo chung.
PCM vẫn dùng dịch vụ âm thanh chung; khi queue đầy giữ lại buffer hiện có để
thử lại, giới hạn chờ 2 giây, kiểm tra thế hệ/hủy phát giữa các khối. Không đổi
contract Protocol, WebSocket hoặc MQTT/UDP.

Điền địa chỉ máy chủ nhạc và khóa tương thích dịch vụ IoT Việt Nam. Có tìm bài theo tên/ca sĩ, phát, tạm dừng, tiếp tục, dừng, chỉnh âm lượng và độ sáng. Để trống khóa máy chủ nhạc để giữ giá trị đang lưu. Hỗ trợ các đài VOV và luồng đài tùy chỉnh HTTP/HTTPS; không có bộ thu FM.

Âm thanh MP3/AAC đi qua dịch vụ âm thanh chung. Yêu cầu phát nhạc có thể bị từ chối khi đang hội thoại; khả năng phát đài còn phụ thuộc mạng và định dạng nguồn.

URL máy chủ nhạc trên web có thể đổi sang máy chủ khác và được lưu khi mất nguồn. Máy chủ cần nhận `/stream_pcm?song=...&artist=...` và trả JSON có `audio_url`; thay URL không tự chuyển đổi giao thức API khác. Không thay firmware khi đổi địa chỉ máy chủ.

Phổ nhạc 16 dải khoảng 80 Hz–8 kHz hiển thị trên màn hình khi phát nhạc hoặc radio. Phổ lấy từ PCM giải mã, cập nhật tối đa 10 lần/giây, trở về mức không khi tạm dừng và ẩn khi ngừng phát/chuyển hội thoại. Không dùng dữ liệu mic hoặc cột chuyển động giả.

## Video MP4 có hình và tiếng

Nhập URL tệp trực tiếp ở mục Phát video MP4 trên web. Hình hiển thị trên ST7789, tiếng qua MAX98357; có tạm dừng, tiếp tục, dừng và ưu tiên hội thoại. Công cụ AI `self.video.play_mp4` nhận URL trực tiếp. Đường dẫn `audio_url` kết thúc bằng `.mp4` từ máy chủ nhạc cũng chuyển sang chế độ video.

Phạm vi triển khai: MP4 với H.264 Baseline hoặc MJPEG, tối đa 240×240 và 15 hình/giây; âm thanh AAC-LC hoặc MP3, 1–2 kênh, 8–48 kHz. Video tối đa một giờ, khung H.264 sau chuẩn hóa tối đa 128 KiB. Máy chủ cung cấp kích thước tệp và hỗ trợ HTTP Range để tìm bảng chỉ mục. Không nhận trang xem YouTube, H.265, DRM hoặc coi mọi tệp có đuôi MP4 là tương thích. Chưa có tua video. Phổ nhạc ẩn khi phát video để dành màn hình cho hình ảnh.

Mã dùng bộ tách `esp_extractor ~1.0.0` và `esp_video_codec ~0.5.7` của Espressif,
chỉ thêm dependency cho UTA. MP4 kiểm tra đầy đủ `Content-Range` (đầu/cuối/tổng,
kích thước phản hồi và kích thước tệp nhất quán), mở Range tiếp theo khi máy chủ
chỉ gửi một phần ngắn và báo lỗi nếu luồng kết thúc sớm. Đồng bộ hình/tiếng dùng
mốc thời gian khung, dời mốc khi tạm dừng; không vẽ thêm khung đã hủy. Bộ tách trả
khung rỗng liên tiếp hoặc thiếu callback/đầu ra hợp lệ sẽ bị từ chối.

Các sửa đổi tối ưu MCP/media ở bước này **chưa build/nạp** theo yêu cầu. Cần thử
trên mạch: tìm bài/không tìm thấy, HTTP 401/403/mất mạng, chuyển nguồn liên tiếp,
dừng trong khi chuẩn bị, pause/resume, đánh thức và phát lại, đài MP3/AAC,
MP4 H.264/MJPEG, Range nhiều phần, kết thúc tệp, độ lệch hình/tiếng và tốc độ SPI.

## Hai động cơ N20 và hai servo 180 độ

Chọn Tắt / L298N mini / DRV8833 ở trang cài đặt; động cơ và servo mặc định tắt. Không bật động cơ trực tiếp đồng thời với UART. Có tiến, lùi, quay trái, quay phải, dừng; tốc độ 1–100% và đảo chiều từng bánh. Lệnh có thời hạn tối đa hai giây, tự dừng khi hết hạn, mất Wi-Fi, mở cấu hình mạng hoặc khởi động lại. Nhảy múa và hiệu ứng đèn chỉ có ở phụ kiện UART; chưa tạo động tác nhảy cho hai N20.

| Chức năng | GPIO mặc định | L298N mini | DRV8833 |
|---|---:|---|---|
| dc_trai_1 | 8 | IN1 | AIN1 |
| dc_trai_2 | 9 | IN2 | AIN2 |
| dc_phai_1 | 10 | IN3 | BIN1 |
| dc_phai_2 | 14 | IN4 | BIN2 |
| servo_dau | 21 | Tín hiệu servo đầu | Tín hiệu servo đầu |
| servo_co | 43 | Tín hiệu servo cổ | Tín hiệu servo cổ |

Các chân đều đổi được qua web. L298N mini dùng PWM trực tiếp trên bốn chân IN1–IN4. DRV8833 cần đưa nSLEEP lên mức bật theo module. Động cơ nối đầu ra công suất driver, dùng nguồn phù hợp điện áp/dòng của N20. Hai servo dùng nguồn ngoài phù hợp và nối chung GND với ESP32; không cấp nguồn servo từ GPIO.

Hai servo dùng xung 50 Hz, 500–2500 µs ứng với 0–180°. Web cho phép đặt góc nhỏ nhất, lớn nhất và góc giữa riêng từng trục; mặc định 15°, 165°, 90°. Sau khi bật/khởi động lại, dùng thanh trượt đầu/cổ hoặc nút về giữa; công cụ AI `self.robot.set_head` điều khiển hai góc. Không lưu góc di chuyển tức thời vào bộ nhớ. Giới hạn góc và cấu hình phần cứng áp dụng sau khởi động lại, góc giữa được đặt khi bật thiết bị. Kênh PWM động cơ và servo tách khỏi đèn nền.

## Phụ kiện tùy chọn

Cảm ứng XPT2046 mặc định tắt, dùng chung MOSI/SCK màn hình; mặc định MISO 11, CS 12, IRQ 13. Vuốt ngang đổi độ sáng, vuốt dọc đổi âm lượng, giữ 0,8 giây phát/tạm dừng, chạm đôi dừng nhạc và chuyển hội thoại. ST7789 thông thường không tự có cảm ứng.

Điều khiển robot UART mặc định tắt: TX 38, RX 48, tốc độ 115200. Cần bộ điều khiển ngoài tương thích lệnh x/y, d1 và w2–w7. Sau khi bật và khởi động lại, trang web và công cụ robot hỗ trợ tiến, lùi, trái, phải, dừng, nhảy và sáu hiệu ứng đèn. Hành động thực tế do bộ điều khiển ngoài thực hiện, không phải điều khiển trực tiếp động cơ từ ESP32.

BOOT một lần dừng nhạc và chuyển hội thoại; hai lần phát/tạm dừng; giữ lâu cấu hình lại Wi-Fi.

## Biên dịch và kiểm tra

Chạy: python scripts/build.py UTA-robot --name uta-robot --language vi-VN

Kiểm tra riêng board: python -m unittest discover -s scripts/tests -p test_uta_robot.py -v

Giao diện riêng của board, thông báo và chú thích dùng tiếng Việt. Cần thử phần cứng thật: mic kênh trái, loa, màn hình, đèn nền, đổi GPIO rồi mất nguồn/bật lại, nhạc/đài và phụ kiện nếu có. Biên dịch thành công không thay thế thử phần cứng.
