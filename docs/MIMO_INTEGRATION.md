# Hướng dẫn tích hợp Mimo vào firmware UTA-robot

Ngày ghi nhận: 12/11/2026.

Tài liệu bàn giao để tiếp tục triển khai trong dự án UTA-robot. **Đây là hướng dẫn và đề xuất thiết kế; phần tích hợp Mimo chưa được thực hiện, build hoặc nạp lên robot.**

## Cập nhật sau bản bàn giao ban đầu: Mimo v0.4

Phần trạng thái và khảo sát bên dưới ghi nhận thời điểm tạo tài liệu ban đầu. Thư viện Mimo hiện đã có **C++17 native cho đủ 85 hoạt động, 4 màn tham chiếu, 3 cảnh phụ và 4 combo**, với bộ vẽ RGB565 vào buffer. Xem [hướng dẫn API, bộ nhớ và kiểm chứng C++](https://github.com/Hao100a01/mimo-vector-eyes/blob/main/docs/CPP_PORT.md) trong repository Mimo riêng tư.

Các file mới cần lấy theo cùng một phiên bản: `MimoActivities.h`, `MimoVector.h`, `MimoDrawList.h`, `MimoRasterizer.h`, thư mục `generated/` và `MimoEyes.h` v0.4. Không ghép riêng header hoạt động mới với header mắt vendored cũ mà chưa kiểm tra tương thích.

Nhánh firmware `feature/atfuta-emotion-engine` đã có thêm `uta_mimo_eyes.*` và `mimo/MimoEyes.h` ở lần đồng bộ gần nhất; cần tiếp tục từ mã đó và giữ nguyên công việc đang triển khai. Lần port hoạt động này chỉ cập nhật thư viện cùng tài liệu, không thay thế renderer hoặc logic firmware hiện có. Bước tiếp theo là nối `Player`/sampling/RGB565 của hoạt động vào adapter UTA, đồng bộ khóa và quyền chiếm màn hình, rồi build và kiểm tra trên mạch. Host tests, sanitizer và đối chiếu với JavaScript đã qua; các kết quả này chưa xác nhận hiệu năng trên ESP32.

## 1. Repository và trạng thái hiện tại

- Firmware: <https://github.com/Hao100a01/xiaozhi-esp32-cuong>, nhánh triển khai `feature/atfuta-emotion-engine`.
- Board trong firmware: `main/boards/UTA-robot/`.
- Thư viện Mimo: <https://github.com/Hao100a01/mimo-vector-eyes>, repository riêng tư, nhánh `main`.
- Web duyệt animation: <https://mimo-vector-eyes.shapedcarp4.chatgpt.site>.
- Danh mục: `docs/ANIMATIONS.md` trong repository Mimo.

Tiếp tục triển khai trên nhánh firmware `feature/atfuta-emotion-engine` đã có. Các đường dẫn `main/boards/...` bên dưới là đường dẫn thư mục mã nguồn, không phải tên nhánh. Phần khảo sát ban đầu được thực hiện trên checkout `main`; cần đối chiếu lại các điểm nối với mã trên `feature/atfuta-emotion-engine` trước khi sửa firmware.

Mimo v0.3 hiện có:

| Phần | Trạng thái |
|---|---|
| 18 biểu cảm và 8 cử chỉ | Có lõi JavaScript và lõi C++ portable |
| 85 hoạt động thuộc 9 nhóm | Đã dựng trên web; chưa port sang C++ |
| 4 màn tham chiếu ngoài danh sách 85 | Đã dựng trên web; chưa port sang C++ |
| 4 combo, 3 cảnh phụ nội bộ | Có bộ chạy trên web; chưa port sang C++ |
| Renderer firmware | Chưa có adapter LVGL cho UTA |
| Âm thanh và lịch nhắc nhở | Các hoạt động hiện chỉ minh họa hình ảnh |

Piano đã nằm trong mục 21 của danh sách 85, không tính hai lần. Tổng cộng có 89 màn công khai và 3 cảnh phụ nội bộ trên web.

**Không thể chép JavaScript, SVG hoặc file cấu hình JSON từ web vào firmware rồi chạy ngay.** JSON xuất từ playground hiện chứa lựa chọn/cấu hình, không chứa đầy đủ thuật toán chuyển động. Ví dụ TFT_eSPI đi kèm Mimo cũng không phải adapter phù hợp trực tiếp với firmware ESP-IDF/LVGL này.

## 2. Những gì đã xác nhận từ mã nguồn UTA

Cấu hình mặc định theo [`config.h`](../main/boards/UTA-robot/config.h), [`config.json`](../main/boards/UTA-robot/config.json) và [README board](../main/boards/UTA-robot/README.md):

- ESP32-S3-N16R8: Flash 16 MB, PSRAM Octal 8 MB.
- Màn TFT SPI ST7789 240 × 240.
- Firmware sử dụng ESP-IDF và LVGL.
- `UtaDisplay` kế thừa `SpiLcdDisplay`.
- Bộ mắt hiện tại phát GIF; đã có lớp phủ trạng thái và phụ đề.
- Logo khởi động, màn chờ Neon/thời tiết, nhạc/phổ âm thanh, video và thông báo đã có đường hiển thị riêng.
- `RunControls()` gọi cập nhật màn chờ/phổ theo nhịp khoảng 100 ms. Đây không phải nhịp đủ để trực tiếp vẽ animation mục tiêu 30 FPS.

Đây là cấu hình mặc định của repository, không phải xác nhận phần cứng đang cắm vào thiết bị. Trang cài đặt cho phép đổi cấu hình màn hình/chân; khi triển khai phải kiểm tra cấu hình đang có hiệu lực.

## 3. Các điểm nối trong firmware

| Tệp / thành phần | Vai trò hiện tại | Việc tích hợp đề xuất |
|---|---|---|
| [`uta_display.h`](../main/boards/UTA-robot/uta_display.h) — `UtaDisplay` | Sở hữu các đối tượng hiển thị UTA | Thêm trạng thái, vòng đời và API điều khiển Mimo |
| [`uta_display_emotions.cc`](../main/boards/UTA-robot/uta_display_emotions.cc) — `SetEmotion()` | Nhận tên cảm xúc; cập nhật yêu cầu hoặc fallback về biểu tượng gốc | Giữ API hiện có, ánh xạ cảm xúc sang Mimo khi chọn backend vector |
| Cùng tệp — `SetupEyesLocked()`, `UpdateEyesLocked()`, `ReleaseEyesLocked()` | Tạo, cập nhật, tạm dừng và giải phóng mắt GIF | Nối vòng đời renderer Mimo, tránh GIF và vector cùng hoạt động trên một vùng |
| [`uta_display.cc`](../main/boards/UTA-robot/uta_display.cc) — `UpdateStandby()` | Quyết định màn chờ, mắt và các lớp phủ | Bổ sung chính sách hiển thị hoạt động Mimo, giữ thứ tự ưu tiên hệ thống |
| [`uta_robot.cc`](../main/boards/UTA-robot/uta_robot.cc) — `RunControls()` | Điều phối trạng thái và tác vụ robot | Gửi yêu cầu animation nhỏ; không nhét render mọi frame vào nhịp 100 ms |
| [`uta_emotions.h`](../main/boards/UTA-robot/uta_emotions.h) | Tên/alias cảm xúc và ánh xạ trạng thái hội thoại | Dùng làm nguồn đối chiếu để giữ tương thích tên cảm xúc |
| [`uta_web_server.cc`](../main/boards/UTA-robot/uta_web_server.cc), `uta_web_page.h` | Trang điều khiển tại cổng 8080 | Thêm chọn backend, chọn animation/combo, dừng và xem trạng thái sau khi lõi ổn định |
| [`main/CMakeLists.txt`](../main/CMakeLists.txt) | Chọn board, source và tài nguyên | Đăng ký source mới đúng phạm vi UTA; kiểm tra cách thu thập source trước khi thêm |

Các tên API/lệnh mới bên dưới là **đề xuất**, chưa tồn tại trong firmware.

## 4. Kiến trúc đề xuất

```text
Trạng thái hội thoại / trang điều khiển / MCP
                   |
          Yêu cầu animation nhỏ
                   |
         Bộ điều phối + ưu tiên
                   |
       Lõi timeline Mimo bằng C++
                   |
     Renderer vector dùng chung LVGL
                   |
     Driver LCD hiện có của firmware
```

Tách ba lớp:

1. **Lõi animation:** pose mắt, thời gian, chuyển tiếp, play/stop/pause/replay và combo. Không phụ thuộc LVGL, driver LCD hoặc mạng.
2. **Renderer LVGL:** đổi pose và lệnh vẽ vector thành hình hiển thị bằng tài nguyên màn hình hiện có.
3. **Adapter UTA:** ánh xạ trạng thái, nhận yêu cầu, quyết định lúc hiển thị và lúc nhường màn hình.

Có thể đặt các module mới trong `main/boards/UTA-robot/mimo/`. Tên và vị trí cuối cùng cần phù hợp quy ước build của repository.

Không khởi tạo thêm TFT_eSPI, bus SPI hoặc driver LCD song song. Không để tác vụ animation gọi trực tiếp driver và bỏ qua quản lý màn hình hiện có.

## 5. Renderer và bố cục màn hình

### Bố cục đầu tiên

- Hệ tọa độ Mimo: 400 × 240, tỉ lệ 5:3.
- Trên màn 240 × 240: scale 0,6 → vùng nội dung 240 × 144.
- Căn giữa: vị trí dự kiến `(0, 48)`.
- Giữ vùng trên cho trạng thái và vùng dưới cho phụ đề; đối chiếu font thực tế để không chồng chữ.
- Khi hỗ trợ màn khác hoặc xoay màn, tính scale/offset từ kích thước đang dùng, không hard-code toàn bộ renderer theo 240 × 240.

### Các primitive cần hỗ trợ

- Polygon tô kín, polyline, độ dày nét và opacity.
- Ellipse/circle.
- Biến đổi tọa độ: tịnh tiến, scale, xoay, mirror.
- Glyph LED ASCII dạng polygon.
- Compound polygon: mỗi phần tử trong trường `paths` là một subpath riêng, không nối chúng bằng nét thừa.

Renderer có thể dùng callback vẽ LVGL hoặc canvas/buffer dùng lại. Chọn phương án sau khi kiểm tra phiên bản và cấu hình LVGL thực tế. Tránh tạo/hủy hàng loạt đối tượng LVGL ở mỗi frame.

Một buffer RGB565 cần khoảng:

| Vùng | Dung lượng pixel thuần |
|---|---:|
| 240 × 144 | 69.120 byte, khoảng 67,5 KiB |
| 240 × 240 | 115.200 byte, khoảng 112,5 KiB |

Đây chưa bao gồm draw buffer LVGL, GIF decoder, âm thanh, video, stack và bộ nhớ phụ. Không quyết định double buffering chỉ dựa trên PSRAM tổng; cần đo heap còn lại và khả năng truyền SPI/DMA.

## 6. Nhịp cập nhật và đồng bộ

- Bắt đầu với mục tiêu 30 FPS, tương ứng khoảng 33 ms/frame; chỉ xác nhận FPS sau khi đo trên mạch.
- Dùng timer LVGL hoặc cơ chế cập nhật tương thích tác vụ hiển thị hiện có, không tăng tốc toàn bộ `RunControls()` chỉ để vẽ mắt.
- Tính chuyển động theo delta time hoặc thời gian monotonic, không dựa vào số lần gọi hàm.
- Callback mạng, MCP, sự kiện hội thoại chỉ gửi yêu cầu nhỏ; không vẽ hoặc làm tác vụ nặng ngay trong callback.
- Truy cập LVGL theo cơ chế khóa hiện có. Callback chạy bên trong LVGL phải tránh tự khóa lại gây deadlock.
- Tạo buffer một lần và dùng lại. Chỉ cấp phát/giải phóng trong các điểm vòng đời đã kiểm soát.
- Tạm dừng/dừng timer khi lớp Mimo bị ẩn; không duy trì render nền vô ích trong lúc video đang chiếm màn hình.
- Hủy timer/callback trước khi hủy đối tượng hoặc giải phóng buffer để tránh truy cập bộ nhớ đã giải phóng.

## 7. Chuyển đổi từ GIF và chính sách ưu tiên

Nên có tùy chọn backend `GIF` / `Mimo vector`, mặc định giữ hành vi cũ trong giai đoạn thử nghiệm.

- Giữ nguyên contract `SetEmotion(const char*)`, đối chiếu alias từ `uta_emotions.h`.
- Với backend Mimo, tên cảm xúc được hỗ trợ đi vào lõi vector. Những biểu tượng chuyên dụng như cảnh báo/nâng cấp vẫn dùng đường xử lý hệ thống phù hợp.
- Một vùng mắt chỉ có một backend đang hoạt động; dừng decoder hoặc renderer của backend còn lại.
- Nếu khởi tạo Mimo thất bại, quay về đường hiển thị cũ và báo lỗi rõ ràng; không để màn đen.

Chính sách cần triển khai và kiểm thử:

1. Khởi động/cấu hình Wi-Fi/kích hoạt/lỗi quan trọng/cảnh báo được ưu tiên theo hành vi hệ thống.
2. Tôn trọng quyền chiếm màn hình của video, thông báo, xem ảnh và các chế độ hiện có.
3. Khi đánh thức/hội thoại bắt đầu, hoạt động giải trí phải nhường chỗ cho mắt lắng nghe/kết nối/trả lời theo quy tắc đã chọn.
4. Hoạt động/combo thủ công chạy khi được phép; không tự khởi động lại hoạt động đã bị ngắt nếu chưa có chính sách rõ ràng.
5. Lặp áp dụng cho toàn combo, không lặp vô hạn cảnh đầu. Dừng phải hủy toàn bộ hàng đợi; phát lại bắt đầu từ cảnh đầu.

Không thay đổi chính sách media/giọng nói hiện có chỉ để phát animation.

## 8. Port hoạt động và combo

Nguồn tham khảo trong repository Mimo:

| Tệp | Nội dung |
|---|---|
| `esp32/src/MimoEyes.h` | Lõi C++ mắt/cử chỉ hiện có; renderer mẫu cần adapter LVGL mới |
| `dist/lib/engine.js` | Hành vi và pose mắt trên web |
| `dist/lib/activity-catalog.js` | 85 ID, nhóm, thời lượng và định nghĩa combo |
| `dist/lib/activities.js` | Choreography của hoạt động mới |
| `dist/lib/vector-kit.js` | Đạo cụ, primitive và glyph LED |
| `dist/lib/performances.js` | Timeline, chuyển tiếp, lặp và combo |
| `tests/activities.test.js`, `tests/performances.test.js` | Các trường hợp hành vi cần giữ khi port |

Các thuật toán procedural JavaScript cần được chuyển sang C++ tương đương. Có thể tách hình học tĩnh, metadata và keyframe thành dữ liệu chung rồi sinh header C++ để giảm sai lệch giữa web và firmware. Việc này đòi hỏi công cụ chuyển đổi và schema thật; file cấu hình JSON hiện tại chưa đáp ứng.

Port và đo ba màn đầu tiên:

1. `wave`: kiểm tra bàn tay, phép xoay và chuyển tiếp.
2. `gaming`: kiểm tra nhiều đạo cụ, thao tác ngón tay và mắt nhìn theo.
3. `piano`: kiểm tra nhiều primitive đổi trạng thái theo nhịp.

Sau đó mở rộng theo nhóm, giữ ID trùng với web để dùng chung danh mục. Chỉ công bố các ID đã có triển khai native; ID chưa port phải trả lỗi rõ ràng, không phát một màn thay thế rồi báo thành công.

## 9. Điều khiển qua web/MCP — giai đoạn sau

API nội bộ dự kiến có các thao tác:

```text
PlayActivity(id, loop)
PlayCombo(id, loop)
Pause() / Resume()
Stop()
Replay()
GetStatus()
```

Có thể bổ sung lệnh MCP như `self.animation.play`, `self.animation.play_combo`, `self.animation.stop`, `self.animation.get_status`; đây chưa phải tên lệnh đã đăng ký.

Trang điều khiển UTA nên hiển thị backend, danh sách hoạt động native thực sự hỗ trợ, tên cảnh đang chạy, bước combo và lỗi. Kiểm tra ID, tốc độ, chế độ lặp và giới hạn độ dài đầu vào. Phản hồi “đã nhận yêu cầu” cần phân biệt với trạng thái “đang hiển thị”.

Không yêu cầu mang toàn bộ trang playground vào ESP32. Giao diện nhẹ tại cổng 8080 chỉ điều khiển renderer trên thiết bị; playground tiếp tục là nơi duyệt và phát triển animation.

## 10. Thứ tự triển khai

- [ ] Kiểm tra cấu hình phần cứng đang dùng, phiên bản LVGL và môi trường build ESP-IDF của UTA.
- [ ] Chuyển sang nhánh `feature/atfuta-emotion-engine`, cập nhật từ remote và đối chiếu các điểm nối trong tài liệu với mã trên nhánh này; ghi nhận baseline bộ nhớ/hiệu năng.
- [ ] Đưa lõi C++ mắt/cử chỉ vào board UTA; thêm adapter LVGL.
- [ ] Nối `SetEmotion()`, giữ lựa chọn GIF/vector và fallback.
- [ ] Kiểm tra vùng 240 × 144, trạng thái, phụ đề và xoay màn.
- [ ] Thêm nhịp cập nhật riêng và quản lý vòng đời/khóa.
- [ ] Port `wave`, `gaming`, `piano`; đo RAM, thời gian vẽ và độ phản hồi giọng nói.
- [ ] Kiểm tra chuyển idle/hội thoại/media/thông báo rồi mới mở rộng nhóm hoạt động.
- [ ] Port combo và kiểm tra play/stop/pause/replay/lặp/ngắt.
- [ ] Thêm điều khiển web/MCP và danh sách ID được hỗ trợ.
- [ ] Mở rộng toàn bộ 85 hoạt động, cập nhật tài liệu và kiểm thử.

## 11. Tiêu chí kiểm tra trên mạch

- [ ] Màu đúng, không méo mắt, không cắt đạo cụ; phụ đề không bị che.
- [ ] Chuyển cảm xúc và đổi backend không nhấp nháy kéo dài hoặc để lại hình cũ.
- [ ] Không có GIF decoder và renderer vector cùng chiếm vùng mắt.
- [ ] Khởi động, Wi-Fi/AP, kích hoạt, thông báo, pin yếu và cập nhật firmware vẫn hoạt động.
- [ ] Nghe/nói vẫn phản hồi đúng khi animation đang chạy; đánh thức ngắt được màn giải trí.
- [ ] Phát/dừng nhạc, radio, video và phổ âm thanh không tranh chấp màn hình.
- [ ] Lệnh dừng/phát lại/đổi màn liên tục không deadlock, crash hoặc rò bộ nhớ.
- [ ] Combo chuyển đúng thứ tự, kết thúc đúng và lặp cả chuỗi khi bật lặp.
- [ ] FPS và mức heap/PSRAM đã đo trong điều kiện thực tế, kể cả khi chạy âm thanh/mạng.
- [ ] Thử hoạt động lâu dài và quay về GIF khi thiếu bộ nhớ/khởi tạo thất bại.

## 12. Phạm vi của lần bàn giao này

Đã đọc mã nguồn và xác định các điểm nối, cấu hình mặc định, bố cục và thứ tự triển khai. Tài liệu không khẳng định Mimo đã chạy trên UTA, không xác nhận 30 FPS trên thiết bị và không thay thế kiểm thử phần cứng.
