# Phân tích 5 board XiaoZhi AI-IoT Vietnam

Nguồn: `D:/xiaozhi/xiaozhi-esp32_vietnam`, commit `287e0ea`.
Ngày phân tích: 2026-11-04. Chỉ đọc source; không sửa repository Việt Nam, không build/nạp firmware.
Repository Việt Nam chưa có graphify-out/graph.json tại thời điểm kiểm tra; kết luận dưới đây từ đọc cấu hình, code board, Application và các tính năng dùng chung, không dùng đồ thị của repository khác để suy ra tính năng.

## So sánh

Tên dưới đây đều có tiền tố `xiaozhi-ai-iot-vietnam-`.

| Đuôi tên board | Target | Mạng thực tế trong code | Màn hình | Audio | Cảm ứng | SD |
|---|---|---|---|---|---|---|
| 1st | ESP32-S3 | WifiBoard | NV3023, cấu hình 284×240, offset X=36 | BoxAudioCodec: ES8311 + ES7210, reference; config yêu cầu device AEC | Chưa thấy triển khai | Có hàm có điều kiện nhưng thiếu khai báo chân CARD_* trong thư mục, chưa dùng được như cấu hình hoàn chỉnh |
| dtd | ESP32-S3 | WifiBoard | Mặc định trong config.json: ILI9341 240×320 non-IPS; nhiều LCD khác chọn khi build | NoAudioCodecSimplex: mic và loa I2S tách; thu 16 kHz, phát 24 kHz | XPT2046 khi bật CONFIG_TOUCH_PANEL_ENABLE | SDMMC 4-bit hoặc SPI có pin map; cần bật CONFIG_SD_CARD_ENABLE |
| es3n28p-lcd-2.8 | ESP32-S3 | WifiBoard | ILI9341 240×320; config.h xoay 270°, khung logic 320×240 | Es8311AudioCodec, 24 kHz | FT6236G khi bật CONFIG_TOUCH_PANEL_ENABLE | SDMMC 4-bit hoặc SPI có pin map; cần bật CONFIG_SD_CARD_ENABLE |
| nvt | ESP32-S3 | WifiBoard | Mặc định config.json: ST7789 240×280; nhiều LCD khác chọn khi build | NoAudioCodecSimplex: thu 16 kHz, phát 24 kHz | Có code XPT2046 có điều kiện, nhưng pin map xung đột với LCD/đèn nền | SDMMC 4-bit hoặc SPI có pin map; cần bật CONFIG_SD_CARD_ENABLE |
| s3-lcd-2.4-ml307 | ESP32-S3 | WifiBoard; chưa thấy khởi tạo ML307 hoặc chuyển sang modem | ILI9341, landscape 320×240 / portrait 240×320; đọc lcd_mode từ NVS | BoxAudioCodec: ES8311 + ES7210, reference; config yêu cầu device AEC | Chưa thấy triển khai | Không override GetSdCard, mặc định Board trả nullptr |

Target chỉ chứng minh họ chip; không đủ xác nhận dung lượng Flash/PSRAM và mã module của PCB đang sử dụng.

## Nghe nhạc và radio

Application gọi InitMusic và InitRadio dùng chung, không giới hạn vào một board trong năm board này:
[application.cc:435](D:/xiaozhi/xiaozhi-esp32_vietnam/main/application.cc:435).

- Nhạc online: Esp32Music tìm bài qua server rồi phát MP3; URL server đọc từ NVS namespace wifi, key music_url, có URL mặc định khi trống. Cần server tương thích và kết nối mạng; chưa kiểm tra server đang hoạt động.
- Radio Internet: Esp32Radio có danh sách VOV và VOV English, chọn decoder AAC/MP3 theo URL. Đây là stream mạng, không phải tuner FM/AM thu bằng anten.
- Bộ phát AudioStreamPlayer có nhánh MP3/AAC/FLAC và WAV PCM. Không đồng nghĩa mọi nguồn nhạc đều cung cấp đủ mọi định dạng.
- Nhạc SD: Application chỉ tạo player khi CONFIG_SD_CARD_ENABLE được bật, board cung cấp SdCard và mount thành công. Có playlist, tìm bài, next/prev, chế độ lặp/xáo trộn và MCP tương ứng.
- CONFIG_SD_CARD_MMC_INTERFACE=y trong config.json chưa thay thế được CONFIG_SD_CARD_ENABLE=y. Kconfig của SD_CARD_ENABLE và TOUCH_PANEL_ENABLE đều mặc định n; các config.json của năm board không bật rõ hai cờ enable này. Cấu hình sdkconfig cục bộ hay người dùng có thể bật chúng, nên không kết luận tất cả ROM đang chạy đều tắt.
- Không xác nhận đài/server Internet còn truy cập được; chưa thử phát âm thanh trên phần cứng.

Nguồn:
[InitMusic/InitRadio/InitSdMusic](D:/xiaozhi/xiaozhi-esp32_vietnam/main/application.cc:1532),
[radio](D:/xiaozhi/xiaozhi-esp32_vietnam/main/features/music/esp32_radio.cc:1),
[music URL](D:/xiaozhi/xiaozhi-esp32_vietnam/main/features/music/esp32_music.cc:387),
[feature tools](D:/xiaozhi/xiaozhi-esp32_vietnam/main/features/mcp_server_features.cc:94),
[Kconfig enable](D:/xiaozhi/xiaozhi-esp32_vietnam/main/Kconfig.projbuild:558).

## Cử chỉ cảm ứng: DTD, NVT, ES3N28P

Chỉ có hiệu lực khi bật cảm ứng, driver khởi tạo được và pin/hardware đúng.

| Cử chỉ | Hành vi trong callback |
|---|---|
| Vuốt phải | Nếu nguồn hiện tại là SD_CARD: Stop rồi Next; ngoài trường hợp đó: tăng độ sáng 5 điểm |
| Vuốt trái | Nếu nguồn hiện tại là SD_CARD: Stop rồi Prev; ngoài trường hợp đó: giảm độ sáng 5 điểm |
| Vuốt lên | Tăng âm lượng 5 điểm, tối đa 100 |
| Vuốt xuống | Giảm âm lượng 5 điểm, tối thiểu 0 |
| Chạm một lần | Không có hành động trong case TAP |
| Chạm đôi | ToggleChatState; lúc starting và Wi-Fi chưa nối thì có reset cấu hình Wi-Fi |
| Giữ chạm | Nếu không có nguồn media: gọi SdMusic::Play nếu player tồn tại; nếu có nguồn media: đặt volume=0 |

Không gọi giữ chạm là toggle play/pause: comment trong code nói vậy nhưng logic thực tế là Play hoặc mute.
Không gọi vuốt trái/phải là chuyển đài: khi phát radio thì hai cử chỉ này đổi độ sáng.
Chưa thấy driver IMU/lắc/cử chỉ tay trên không, servo hay robot được khởi tạo trong năm board.

Nguồn: [DTD](D:/xiaozhi/xiaozhi-esp32_vietnam/main/boards/xiaozhi-ai-iot-vietnam-dtd/xiaozhi_ai_iot_vietnam_dtd.cc:276),
[NVT](D:/xiaozhi/xiaozhi-esp32_vietnam/main/boards/xiaozhi-ai-iot-vietnam-nvt/xiaozhi_ai_iot_vietnam_ntv.cc:277),
[ES3N28P](D:/xiaozhi/xiaozhi-esp32_vietnam/main/boards/xiaozhi-ai-iot-vietnam-es3n28p-lcd-2.8/xiaozhi_ai_iot_vietnam_es3n28p_lcd_2.8.cc:346).

## Nút vật lý

| Board | Nút |
|---|---|
| 1st | BOOT chat/đánh thức; nhấn 6 lần reset Wi-Fi; nút +/- chỉnh 10 điểm; giữ + đặt 100, giữ - đặt 0 |
| DTD/NVT | BOOT chat; nhấn 5 lần reset Wi-Fi; +/- chỉnh 10 điểm; giữ + max, giữ - mute |
| ES3N28P | BOOT chat; nhấn 5 lần reset Wi-Fi; không khai báo nút volume riêng trong class |
| S3 LCD 2.4 ML307 | BOOT chat; nhấn đôi lúc idle đổi AEC và đặt âm lượng 60; giữ BOOT reset Wi-Fi; +/- chỉnh 10; giữ + max, giữ - mute |

Nhánh BOOT của các board còn có reset Wi-Fi khi đang starting và chưa kết nối. Hành vi nhiều lần click tùy thư viện nút và thời gian nhấn; chưa kiểm tra phần cứng.

## GPIO và web

- Chân audio/LCD/SD/touch/nút được khai báo trong config.h hoặc code khởi tạo. Chọn LCD/SD/touch qua Kconfig là thay đổi lúc build, không phải remap pin runtime.
- README ghi Web Server Cấu Hình GPIO đang phát triển; không tìm thấy trang/API đổi sơ đồ GPIO tổng quát ở mã đã kiểm tra.
- OtaServer đăng ký /ota, /ota_upload, /assets, /assets_upload, phục vụ cập nhật firmware/tài nguyên; không phải giao diện đổi GPIO.
- Music URL trong NVS có thể thay đổi không build lại, khác với đổi chân phần cứng.
- Một vài chức năng cố định có LampController/MCP; điều khiển trạng thái chân đã gán không đồng nghĩa chọn một chân mới qua web.
- Board S3 LCD 2.4 có tool AEC set/get và restart. Không thấy tool remap pin trong mcp_controller.cc.

Nguồn: [trạng thái README](D:/xiaozhi/xiaozhi-esp32_vietnam/README.md:147),
[các route OTA](D:/xiaozhi/xiaozhi-esp32_vietnam/main/ota_server.cc:55),
[MCP controller](D:/xiaozhi/xiaozhi-esp32_vietnam/main/boards/xiaozhi-ai-iot-vietnam-s3-lcd-2.4-ml307/mcp_controller.cc:24).

## Các chân LCD quan trọng trong config.h

Số là GPIO ESP32, không phải thứ tự chân trên đầu cắm; phải đối chiếu schematic PCB trước khi đấu dây.

| Board | MOSI | CLK | DC | CS | RESET | Backlight |
|---|---:|---:|---:|---:|---:|---:|
| 1st | 10 | 9 | 8 | 14 | 18 | 13 |
| DTD | 12 | 11 | 13 | 21 | 14 | 10 |
| ES3N28P | 11 | 12 | 46 | 10 | NC | 45 |
| NVT | 10 | 9 | 46 | 12 | 11 | 3 |
| S3 LCD 2.4 ML307 | 8 | 5 | 6 | 7 | 4 | 42 |

Touch DTD/NVT theo nhánh riêng mặc định trong header: SPI2, MOSI46, MISO18, CLK9, CS3, INT17.
DTD LCD dùng SPI3 với pin khác; NVT bị trùng CS touch3/backlight3 và MOSI touch46/DC LCD46; CLK9 còn nằm trên hai SPI host. Cần sửa board/variant đúng phần cứng trước khi bật touch trên NVT.
Touch ES3N28P dùng I2C chung audio SDA16/SCL15, RESET18, INT17, địa chỉ 0x38.
SDMMC DTD/NVT: CLK40, CMD39, D0=41, D1=42, D2=45, D3=38.
SDMMC ES3N28P: CLK38, CMD40, D0=39, D1=41, D2=48, D3=47.

## Điểm cần kiểm tra trước khi build/nạp

1. Tên ML307 chưa chứng minh hỗ trợ modem: class AIoTS3Lcd24ML307 kế thừa WifiBoard; chưa thấy init modem, pin UART modem hoặc GetCamera override. Include camera và README copy từ board camera không chứng minh chức năng camera đã có.
2. NVT: thư mục nvt nhưng config.json đặt tên build ...-vnt. Class còn mang tên DTD. Đây là dấu hiệu sao chép/đặt tên chưa thống nhất, cần dùng đúng tên build và xác nhận OTA identity khi phát hành.
3. NVT: xung đột chân touch với LCD/đèn nền như bảng trên; chỉ phát sinh nhánh này khi bật touch.
4. ES3N28P: config.json có dòng ES3N28P_LCD_ILI9341_240X320=y thiếu tiền tố CONFIG_. Chưa coi đó là tùy chọn Kconfig hợp lệ; hiện config.h vẫn định nghĩa panel qua LCD_ROTATION_DEGREE=270 nên chưa kết luận riêng lỗi này làm build hỏng.
5. 1st: có hàm GetSdCard có điều kiện nhưng chưa thấy định nghĩa pin CARD_SDMMC_* hoặc CARD_SPI_* trong thư mục; bật SD cần bổ sung/kiểm tra nguồn định nghĩa chân, không đánh dấu có SD sẵn.
6. SD và touch mặc định n. Chỉ chọn kiểu giao tiếp trong config.json chưa đủ chứng minh đã bật tính năng.
7. Chưa build, chưa xác nhận SDK tương thích, dung lượng Flash/PSRAM, kết nối radio/server hay chất lượng mic/loa. Cần test thực tế khi chọn cấu hình cuối.

## Mức độ xác nhận

Đã đối chiếu năm config.json/config.h, năm lớp board, callback cử chỉ, Application init media, music/radio player, MCP feature tools, Kconfig, CMake và OtaServer.
Không sửa các vấn đề phát hiện vì yêu cầu hiện tại là phân tích. Không truy cập hoặc thay đổi máy chủ ngoài.
