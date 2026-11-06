# Bộ emotion đôi mắt 240×240

21 GIF gốc do người dùng cung cấp từ `D:\xiaozhi\emoji_doi_mat` đã được sao chép
nguyên byte vào repo. Không cần thư mục nguồn trên máy khi build về sau.
`manifest.json` lưu SHA-256, kích thước, số khung, thời gian vòng và tốc độ khung
để kiểm tra tài nguyên. `emotions.cmake` liệt kê các GIF được đóng gói vào phân vùng
tài nguyên khi chọn UTA-robot.
Tổng GIF: **898.150 byte (~877 KiB)**; chưa tính mã và tài nguyên Neon. Cần kiểm
tra phân vùng firmware sau khi được phép build, chưa khẳng định ảnh firmware vừa phân vùng.

| Chức năng / cảm xúc | GIF |
|---|---|
| Trung tính / trả lời khi chưa có cảm xúc riêng | `1macdinh.gif` |
| Đang lắng nghe | `binhthuong.gif` |
| Suy nghĩ / kết nối khi chưa có cảm xúc riêng | `suynghi2.gif` |
| Vui / rất vui / cười | `vuive.gif`, `vuimung.gif`, `cuoito.gif` |
| Buồn / buồn nhẹ | `buon.gif`, `buon2.gif` |
| Giận / khó chịu | `tucgian.gif`, `tucgian2.gif` |
| Ngạc nhiên | `ngacnhien2.gif` |
| Buồn ngủ | `buonngu.gif` |
| Chớp mắt / nháy mắt | `nhaymat.gif`, `duamat.gif` |
| Tò mò / bối rối | `chamhoi.gif`, `hoamat.gif` |
| Sợ / ngượng | `khongchiudau.gif`, `domohoi.gif` |
| Dò xét / im lặng / nheo mắt | `doxet.gif`, `camlang.gif`, `nheomat.gif` |

`uta_emotions.h` ánh xạ các tên emotion server như `neutral`, `happy`, `sad`,
`angry`, `laughing`, `surprised`, `thinking`, `winking`, v.v. Tên tệp gốc có hoặc
không có `.gif` cũng dùng được. Tên không có trong bộ ảnh tiếp tục đi qua cơ chế
hiển thị gốc để giữ gear/cảnh báo và các biểu tượng chức năng.

`UtaDisplay::SetEmotion()` chỉ chuyển yêu cầu; GIF được nạp bởi tác vụ điều khiển
dưới khóa LVGL. Decoder mắt chỉ giữ một bộ khung hiện tại (~288 KiB cộng cấu trúc
và LZW), không giải mã cả 21 GIF cùng lúc. Cùng một emotion không tải lại; ẩn thì
pause; mở lại thì resume. Vòng lặp được đặt liên tục khi hiển thị, giữ nguyên các
mốc thời gian khung gốc. Nạp lỗi giữ giao diện gốc và giới hạn thử lại 30 giây.

Khi listening, neutral tự đổi thành `binhthuong`; khi connecting tự đổi thành
`suynghi2`; cảm xúc riêng từ server được ưu tiên. Khi speaking, dùng emotion gần
nhất. Sau hội thoại trở về Neon. Cảm xúc riêng được yêu cầu trong idle có thể
hiện tối đa 5 giây trước khi về Neon. Thông báo, ảnh xem trước, cấu hình Wi-Fi,
nâng cấp và cảnh báo được ưu tiên. Caption trạng thái và phụ đề một dòng nằm
trên/dưới bộ mắt; phụ đề giới hạn 192 byte và không cắt giữa ký tự UTF-8.

Tạo lại tài nguyên, không build firmware:

```powershell
python scripts/prepare_uta_emotions.py --source D:/xiaozhi/emoji_doi_mat
./scripts/preview_uta_emotions.ps1
```

Không có `--source` thì kiểm tra và tạo manifest từ GIF đã nằm trong repo.
`preview-emotions.png` là bảng ảnh tĩnh để duyệt. Script PowerShell đọc và vẽ
mọi khung bằng bộ giải mã Windows; kiểm tra này chưa thay thế thử giải mã LVGL
và tốc độ màn hình trên ESP32. Chưa chạy build/nạp firmware ở bước tích hợp này.
