# Virtual Input Lab V2

Đây là bộ thử nghiệm Win32 độc lập dành cho ứng dụng do chúng ta sở hữu. Bộ này
không can thiệp, không hook và không nạp DLL vào game.

## V2 kiểm tra những gì?

- Mỗi `VirtualInputTarget.exe` có nút **TRỢ GIÚP** với vùng hit-test thật.
- Controller phát riêng `LEFT DOWN`, giữ nút theo số mili giây đã chọn, rồi phát
  `LEFT UP` với cùng sequence.
- Bộ đếm chỉ tăng khi DOWN và UP đều nằm trong nút.
- Chế độ tọa độ chuẩn hóa dùng thang `0..10000`; vì vậy Target kích thước khác
  nhau vẫn nhận đúng vị trí tương đối.
- Mỗi Target hiển thị PID, kích thước client, X/Y thực nhận, sequence và thời
  điểm nhận để đối chiếu.
- Lệnh được gửi song song qua Named Pipe và không di chuyển con trỏ Windows.
- Có lặp, tạm dừng/tiếp tục và đặt lại bộ đếm.

## Build bằng GitHub Actions

Workflow cũ vẫn dùng được vì tên hai file EXE không đổi. Sau khi commit source
V2, mở **Actions → Build Virtual Input Lab**, chờ dấu tích xanh rồi tải artifact
`VirtualInputLab_Windows_x64`.

## Kiểm thử chuẩn

1. Xóa hoặc đóng toàn bộ EXE V1 đang chạy.
2. Mở `VirtualInputTarget.exe` 6 lần.
3. Kéo thay đổi kích thước các Target thành nhiều cỡ khác nhau.
4. Mở `VirtualInputController.exe`, bấm **Làm mới danh sách** và kiểm tra đủ 6 PID.
5. Giữ chọn **Tọa độ chuẩn hóa 0–10000**.
6. Đặt `X=5000`, `Y=6900`, giữ nút `80 ms`.
7. Bấm **Đặt lại bộ đếm**, sau đó bấm **Click đồng thời** một lần.
8. Cả 6 Target phải hiện `Click trúng nút: 1`, cùng sequence; con trỏ thật vẫn
   có thể dùng ở nơi khác.
9. Đặt lặp `10`, giãn cách `1000 ms`, bấm **Bắt đầu lặp**. Sau khi hoàn tất, cả
   6 Target phải có cùng bộ đếm.

Nếu bỏ chọn tọa độ chuẩn hóa, X/Y được hiểu là pixel client tuyệt đối. Khi các
Target khác kích thước, cùng một X/Y có thể không còn nằm trong nút; đây là phép
đối chứng để thấy vì sao cần quy đổi tọa độ.

## Phạm vi kết quả

Thử nghiệm chỉ chứng minh IPC, hit-test và tọa độ ảo hoạt động khi ứng dụng đích
chủ động hỗ trợ giao thức. Nó không làm cho một ứng dụng hoặc trò chơi bên thứ ba
tự nhận Named Pipe hay tọa độ ảo.
