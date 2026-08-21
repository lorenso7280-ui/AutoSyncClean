# AutoSync Clean

Ứng dụng Windows C++ độc lập để đồng bộ thao tác bàn phím và chuột giữa nhiều cửa sổ. Dự án được viết mới dựa trên hành vi quan sát trong video, không chứa hệ thống tài khoản, VIP hoặc key.

## Chức năng

- Khi khởi động, danh sách luôn trống; chỉ hiển thị cửa sổ Doomsday do người dùng kéo thả nút tròn để nhận thủ công.
- Giữ các cửa sổ đã nhận trong danh sách và tự đổi trạng thái `ONLINE`/`OFFLINE` khi game mở hoặc đóng.
- Kéo biểu tượng tròn bên trái rồi thả vào cửa sổ Doomsday để nhận cửa sổ thủ công.
- Mở đồng thời nhiều tiến trình từ một file `.exe`, với tham số dòng lệnh, số lượng và thời gian giãn cách tùy chọn.
- Tự lưu đường dẫn game, tham số, số lượng và thời gian giãn cách cho lần sử dụng sau.
- Chọn/bỏ chọn từng cửa sổ bằng checkbox.
- Đặt một cửa sổ làm cửa sổ chính bằng nút **Cửa sổ chính**, nhấp đúp hoặc menu chuột phải.
- Đồng bộ phím xuống/lên, chuột trái/phải/giữa, di chuyển và cuộn.
- Quy đổi tọa độ chuột theo tỉ lệ vùng client của từng cửa sổ.
- Xếp chồng các cửa sổ game tại cùng vị trí để nhìn như một cửa sổ, với thông số kích thước, tọa độ X/Y và độ lệch tùy chọn.
- Thanh xem trước thu nhỏ cập nhật trực tiếp cho toàn bộ cửa sổ game; bấm vào ảnh để đưa game tương ứng lên trên.
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

### Mở nhiều cửa sổ

1. Chọn một cửa sổ game trong danh sách rồi bấm nút biểu tượng cửa sổ ở góc trên bên trái. Phần mềm sẽ cố tự lấy đường dẫn file `.exe`; cũng có thể bấm `...` để chọn thủ công.
2. Nhập tham số khởi động mà game hỗ trợ, ví dụ `-la` như trong bản tham chiếu.
3. Nhập số cửa sổ và thời gian giãn cách (mili giây), rồi bấm **Xác nhận**.
4. Nếu game chạy quyền Administrator, hãy chạy AutoSync Clean cùng quyền. Nếu game hoặc launcher tự khóa một phiên, phần mềm không vượt khóa đó.

Các thông số trong cửa sổ **Mở cửa sổ** được lưu ngay khi bấm **Xác nhận** và tự điền lại trong lần chạy tiếp theo.

### Nhận cửa sổ bằng kéo thả và trạng thái

1. Giữ chuột trái trên nút tròn `◎` ở góc trên bên trái.
2. Kéo con trỏ dấu cộng vào cửa sổ game Doomsday rồi thả chuột.
3. Cửa sổ được nhận sẽ xuất hiện trong danh sách với trạng thái `ONLINE`.
4. Khi game đóng, dòng đó vẫn được giữ lại và chuyển thành `OFFLINE`. Khi cửa sổ cùng tên mở lại, phần mềm tự ghép và chuyển về `ONLINE`.
5. Dùng menu chuột phải **Xóa khỏi danh sách** nếu muốn xóa hẳn một dòng, kể cả dòng đang `OFFLINE`.

Để ghi chuỗi thao tác, bấm **Ghi thao tác**, thao tác trong cửa sổ chính, bấm **Dừng ghi**, sau đó dùng **Phát lại**.

### Xếp chồng và di chuyển cửa sổ

1. Đánh dấu các cửa sổ game cần sắp xếp và bấm nút biểu tượng lưới ở góc trên bên phải.
2. Nhập kích thước vùng game, vị trí cách mép trái `X` và cách mép trên `Y`.
3. Đặt độ lệch mỗi cửa sổ `X = 0`, `Y = 0` để các cửa sổ chồng khít và nhìn như một cửa sổ duy nhất.
4. Có thể nhập độ lệch khác 0 nếu muốn nhìn thấy mép của từng cửa sổ.
5. Bấm **Xác nhận**. Cửa sổ chính sẽ được đưa lên trên cùng.

Bản Windows yêu cầu quyền Administrator khi mở để có thể di chuyển/đổi kích thước các cửa sổ game đang chạy quyền cao. Sau khi sắp xếp, thanh trạng thái sẽ báo số cửa sổ thực sự áp dụng thành công.

### Thanh cửa sổ thu nhỏ

1. Bấm nút biểu tượng thanh thu nhỏ `▤` ở góc trên bên phải.
2. Thanh **Xem cửa sổ thu nhỏ** sẽ mở sát đáy màn hình và tự hiển thị ảnh trực tiếp của mọi cửa sổ Doomsday.
3. Bấm vào một ảnh thu nhỏ để khôi phục và đưa cửa sổ game đó lên trên cùng.
4. Kéo cạnh thanh để đổi kích thước; bấm lại nút `▤` hoặc nút đóng của thanh để ẩn.

## Giới hạn kỹ thuật

- Ứng dụng dùng hook bàn phím/chuột cấp thấp và gửi thông điệp Win32 tới cửa sổ đích.
- Nếu ứng dụng đích chạy bằng quyền Administrator, AutoSync Clean cũng cần chạy cùng mức quyền.
- Một số game dùng Raw Input, DirectInput độc quyền hoặc cơ chế chống gian lận có thể không nhận thông điệp `PostMessage`. Dự án không can thiệp tiến trình, không tiêm DLL và không vượt cơ chế chống gian lận.
- Các cửa sổ nên có cùng tỉ lệ khung hình để tọa độ chuột tương ứng chính xác nhất.
