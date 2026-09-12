# AutoSync Clean v.79 Clean

Bản v79 Clean giữ nguyên nền đồng bộ của v78 Clean và tập trung sửa độ ổn định giao diện.

## Sửa trong v79

- Sửa lỗi tích checkbox rồi kéo danh sách xuống nhưng sau lần refresh tự nhảy trở lại phía trên.
- Giữ vị trí cuộn của danh sách qua chu kỳ refresh tự động.
- Thêm bộ lọc thay đổi HWND tạm thời: chỉ cập nhật danh sách/preview khi việc mở/đóng cửa sổ tồn tại qua hai lần kiểm tra liên tiếp.
- Giảm tình trạng thanh xem cửa sổ thu nhỏ chớp, đổi preview bất thường khi một HWND thoáng mất rồi xuất hiện lại.
- Không thay đổi cơ chế đồng bộ bàn phím/chuột đang dùng ở v78 Clean.

## Chức năng chính

- Nhận diện và quản lý nhiều cửa sổ game.
- Chọn cửa sổ chính và các cửa sổ cần đồng bộ.
- Đồng bộ trực tiếp bàn phím và chuột từ cửa sổ chính.
- Mở nhiều cửa sổ, sắp xếp/đổi kích thước cửa sổ.
- Xem cửa sổ thu nhỏ, Proxy và Thiết lập.

## Sử dụng

1. Mở các cửa sổ game.
2. Kéo nút tròn `◎` và thả vào một cửa sổ game để nhận diện.
3. Tích những cửa sổ cần đồng bộ.
4. Chọn một cửa sổ làm **Cửa sổ chính**.
5. Bấm **Bật đồng bộ** rồi thao tác trực tiếp trên cửa sổ chính.
6. Bấm **Tắt đồng bộ** khi hoàn tất.

## Build

Workflow `.github/workflows/build-windows.yml` tạo artifact `AutoSyncClean-Windows-x64-v79`, bên trong có:

`AutoSyncClean v.79 Clean.exe`

## Giới hạn

Ứng dụng đồng bộ bằng hook bàn phím/chuột Windows và thông điệp Win32. Một số game có thể không nhận thao tác nền. Bản v79 Clean không chứa DLL injection, hook nội bộ trò chơi hoặc cơ chế vượt bảo vệ tiến trình.
