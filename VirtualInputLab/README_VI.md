# Virtual Input Lab

Đây là bộ thử nghiệm Win32 độc lập, không can thiệp và không nạp DLL vào game.

## Mục tiêu

- Mỗi tiến trình `VirtualInputTarget.exe` sở hữu một tọa độ chuột ảo riêng.
- `VirtualInputController.exe` gửi cùng một lệnh tới mọi target được đánh dấu.
- Lệnh được gửi song song qua Named Pipe nên các target phản ứng gần đồng thời.
- Không dùng `SetCursorPos`, `SendInput` hoặc con trỏ Windows thật.
- Có lặp tối đa 99.999 lần, giãn cách và tạm dừng.

## Build

Yêu cầu Windows 10/11, Visual Studio 2022 có workload **Desktop development with C++** và CMake.

1. Mở **Developer Command Prompt for VS 2022**.
2. Chạy `build.bat`.
3. Hai file EXE nằm trong `build\Release`.

## Kiểm thử

1. Mở `VirtualInputTarget.exe` từ 3 đến 10 lần.
2. Mở `VirtualInputController.exe`, bấm **Làm mới danh sách**.
3. Chọn các target, nhập X/Y rồi bấm **Click đồng thời**.
4. Quan sát dấu tròn và bộ đếm trong từng target thay đổi trong khi vẫn rê chuột thật ở Chrome.
5. Thử **Bắt đầu lặp**; bấm lại nút để tạm dừng.

## Ý nghĩa kết quả

Nếu thử nghiệm hoạt động, nó xác nhận kiến trúc IPC và trạng thái đầu vào riêng theo từng tiến trình là khả thi khi ứng dụng đích chủ động hỗ trợ giao thức. Điều đó không chứng minh một game bên thứ ba sẽ nhận `PostMessage`, Named Pipe hoặc tọa độ ảo, vì game không có phần nhận lệnh này.

## Giới hạn an toàn

Bộ thử nghiệm không có injector, hook API, driver, thao tác bộ nhớ tiến trình hoặc chức năng nạp DLL vào phần mềm khác.

