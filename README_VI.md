# AutoSync Clean v.78 Clean

Đây là bản giao diện gọn, chỉ giữ các chức năng đã sử dụng được:

- Nhận diện và quản lý nhiều cửa sổ game.
- Chọn cửa sổ chính và các cửa sổ cần đồng bộ.
- Đồng bộ trực tiếp bàn phím và chuột từ cửa sổ chính.
- Mở nhiều cửa sổ, sắp xếp/đổi kích thước cửa sổ.
- Xem cửa sổ thu nhỏ, Proxy và Thiết lập.

Đã loại bỏ khỏi giao diện:

- Nút `IPC` và mô-đun IPC tùy chọn.
- Nút `Quản lí bản ghi` và toàn bộ đường mở cửa sổ ghi/phát lại.

## Sử dụng

1. Mở các cửa sổ game.
2. Kéo nút tròn `◎` và thả vào một cửa sổ game để nhận diện.
3. Tích những cửa sổ cần đồng bộ.
4. Chọn một cửa sổ làm **Cửa sổ chính**.
5. Bấm **Bật đồng bộ** rồi thao tác trực tiếp trên cửa sổ chính.
6. Bấm **Tắt đồng bộ** khi hoàn tất.

## Build trên GitHub Actions

Upload toàn bộ nội dung dự án lên nhánh `build/windows-release`. Workflow
`.github/workflows/build-windows.yml` sẽ tạo artifact
`AutoSyncClean-Windows-x64`, bên trong có:

`AutoSyncClean v.78 Clean.exe`

## Giới hạn

Ứng dụng đồng bộ bằng hook bàn phím/chuột Windows và thông điệp Win32. Một số
game có thể không nhận thao tác nền. Bản v78 Clean không chứa DLL injection,
hook nội bộ trò chơi hoặc cơ chế vượt bảo vệ tiến trình.
