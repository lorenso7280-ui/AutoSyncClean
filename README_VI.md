# AutoSync Clean

Ứng dụng Windows C++ độc lập để đồng bộ thao tác bàn phím và chuột giữa nhiều cửa sổ. Dự án được viết mới dựa trên hành vi quan sát trong video, không chứa hệ thống tài khoản, VIP hoặc key.

## Chức năng

- Tự tìm các cửa sổ cấp cao đang hiển thị.
- Chọn/bỏ chọn từng cửa sổ bằng checkbox.
- Đặt một cửa sổ làm cửa sổ chính bằng nút **Cửa sổ chính**, nhấp đúp hoặc menu chuột phải.
- Đồng bộ phím xuống/lên, chuột trái/phải/giữa, di chuyển và cuộn.
- Quy đổi tọa độ chuột theo tỉ lệ vùng client của từng cửa sổ.
- Sắp xếp các cửa sổ đã chọn theo dạng lưới.
- Ghi và phát lại một chuỗi thao tác trong phiên làm việc.
- Menu quản lý: thêm/làm mới, hiện, đóng hoặc loại cửa sổ khỏi danh sách.

## Biên dịch

Yêu cầu Windows 10/11, Visual Studio 2022 với workload **Desktop development with C++**, và CMake.

1. Mở `Developer Command Prompt for VS 2022`.
2. Chuyển vào thư mục dự án.
3. Chạy `build.bat`.
4. File kết quả nằm tại `build\Release\AutoSyncClean.exe`.

Hoặc mở thư mục dự án trực tiếp bằng Visual Studio và chọn cấu hình x64 Release.

## Sử dụng

1. Mở các cửa sổ cần điều khiển rồi bấm **Làm mới**.
2. Đánh dấu các cửa sổ con cần nhận thao tác.
3. Chọn cửa sổ nguồn và bấm **Cửa sổ chính**.
4. Bấm **Bật đồng bộ**, sau đó thao tác bên trong cửa sổ chính.
5. Bấm **Tắt đồng bộ** khi hoàn tất.

Để ghi chuỗi thao tác, bấm **Ghi thao tác**, thao tác trong cửa sổ chính, bấm **Dừng ghi**, sau đó dùng **Phát lại**.

## Giới hạn kỹ thuật

- Ứng dụng dùng hook bàn phím/chuột cấp thấp và gửi thông điệp Win32 tới cửa sổ đích.
- Nếu ứng dụng đích chạy bằng quyền Administrator, AutoSync Clean cũng cần chạy cùng mức quyền.
- Một số game dùng Raw Input, DirectInput độc quyền hoặc cơ chế chống gian lận có thể không nhận thông điệp `PostMessage`. Dự án không can thiệp tiến trình, không tiêm DLL và không vượt cơ chế chống gian lận.
- Các cửa sổ nên có cùng tỉ lệ khung hình để tọa độ chuột tương ứng chính xác nhất.

