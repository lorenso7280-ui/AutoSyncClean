# AutoSync Clean

Ứng dụng Windows C++ độc lập để đồng bộ thao tác bàn phím và chuột giữa nhiều cửa sổ. Dự án được viết mới dựa trên hành vi quan sát trong video, không chứa hệ thống tài khoản, VIP hoặc key.

## Chức năng

- Khi khởi động, danh sách luôn trống; chỉ hiển thị cửa sổ Doomsday do người dùng kéo thả nút tròn để nhận thủ công.
- Giữ các cửa sổ đã nhận trong danh sách và tự đổi trạng thái `ONLINE`/`OFFLINE` khi game mở hoặc đóng.
- Kéo biểu tượng tròn bên trái rồi thả vào cửa sổ Doomsday để nhận cửa sổ thủ công.
- Sau khi nhận cửa sổ đầu tiên, các cửa sổ mới có cùng tiêu đề game sẽ tự động được thêm vào cùng nhóm.
- Các cửa sổ mới được thêm vào với checkbox mặc định bỏ chọn; chỉ những dòng người dùng tự tích mới nhận thao tác đồng bộ.
- Mở đồng thời nhiều tiến trình từ một file `.exe`, với tham số dòng lệnh, số lượng và thời gian giãn cách tùy chọn.
- Tự lưu đường dẫn game, tham số, số lượng và thời gian giãn cách cho lần sử dụng sau.
- Chọn/bỏ chọn từng cửa sổ bằng checkbox.
- Lệnh **Chọn tất cả** tự đổi tên theo thứ tự `Cửa sổ 1`, `Cửa sổ 2`… trong danh sách và trên thanh tiêu đề của từng cửa sổ game.
- Đặt một cửa sổ làm cửa sổ chính bằng nút **Cửa sổ chính**, nhấp đúp hoặc menu chuột phải.
- Nhấp phải trực tiếp vào một dòng và chọn **Làm cửa sổ chính**; dòng nguồn được đánh dấu `★ [CỬA SỔ CHÍNH]` ở tiêu đề và trạng thái.
- Đồng bộ phím xuống/lên, chuột trái/phải/giữa, di chuyển và cuộn.
- Quy đổi tọa độ chuột theo tỉ lệ vùng client của từng cửa sổ.
- Xếp chồng các cửa sổ game tại cùng vị trí để nhìn như một cửa sổ, với thông số kích thước, tọa độ X/Y và độ lệch tùy chọn.
- Thanh xem trước thu nhỏ cập nhật trực tiếp cho toàn bộ cửa sổ game; bấm vào ảnh để đưa game tương ứng lên trên.
- Cửa sổ chính khởi động ở chiều cao gọn, vừa khoảng 6 dòng ID game; người dùng vẫn có thể kéo cạnh để mở rộng.
- Thanh xem trước hiển thị tối đa 10 cửa sổ trên mỗi hàng và tự thêm hàng khi có nhiều hơn 10 cửa sổ.
- Sắp xếp/resize và thanh cửa sổ thu nhỏ luôn áp dụng cho toàn bộ cửa sổ game `ONLINE`, không cần tích checkbox; checkbox chỉ quyết định cửa sổ nhận thao tác đồng bộ.
- Ghi và phát lại một chuỗi thao tác trong phiên làm việc.
- Thanh công cụ theo bố cục phần mềm tham chiếu: nhận cửa sổ, mở nhiều cửa sổ và đồng bộ ở bên trái; bản ghi, sắp xếp, Proxy, cửa sổ thu nhỏ và Thiết lập ở bên phải.
- Năm nút bên phải dùng biểu tượng GDI tự vẽ, không phụ thuộc font của máy, theo đúng thứ tự: `R` bản ghi, lưới sắp xếp, quả địa cầu Proxy, màn hình thu nhỏ và bánh răng Thiết lập.
- Nút `Ⓡ` mở menu bắt đầu/dừng, phát hoặc xóa bản ghi; nhấp phải vào vùng trống của danh sách cũng mở menu này.
- Nút `⚙` cho phép bật/tắt riêng chuyển động chuột trong khi vẫn giữ đồng bộ click và bàn phím.
- Menu quản lý: thêm/làm mới, hiện, đóng hoặc loại cửa sổ khỏi danh sách.
- **Đóng tất cả** áp dụng cho mọi cửa sổ game `ONLINE`, bao gồm cửa sổ chính và không phụ thuộc checkbox. Phần mềm gửi lệnh đóng đồng thời, chờ 300 ms rồi buộc kết thúc đúng các tiến trình game vẫn không phản hồi; lệnh này tự tắt đồng bộ trước và có thể làm mất dữ liệu game chưa lưu.
- **Hiện tất cả** tiếp tục áp dụng cho những dòng đã tích checkbox.

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

Có thể nhấp chuột phải vào bất kỳ dòng `ONLINE` nào rồi chọn **Làm cửa sổ chính**. Khi đồng bộ bật, phím và chuột chỉ được lấy từ cửa sổ có dấu `★`; thao tác được gửi tới các cửa sổ còn lại đã tích checkbox và không gửi ngược lại cửa sổ chính.

Khi chọn **Đồng bộ → Chọn tất cả**, các cửa sổ được đánh số lại từ trên xuống. Phần mềm gửi thông điệp bàn phím/chuột tới control đang nhận focus bên trong mỗi cửa sổ game, giúp các game Win32 nhận thao tác nền ổn định hơn.

Ngay khi chọn **Làm cửa sổ chính**, phần mềm khôi phục, đưa cửa sổ đó lên trên và chuyển focus vào vùng game. Khi bấm **Bật đồng bộ**, thao tác nhấn/thả chuột và phím được xếp vào hàng đợi của tất cả cửa sổ đích mà không chờ tuần tự từng cửa sổ; nhờ đó một cửa sổ phản hồi chậm không làm các cửa sổ sau bị trễ. Tọa độ chuột được quy đổi theo đúng vùng điều khiển/render đang nhận focus của từng cửa sổ. Chuột di chuyển vẫn được giới hạn ở 60 lần/giây để tránh đầy hàng đợi thông điệp.

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
4. Sau khi đã nhận cửa sổ đầu tiên, nếu mở thêm 4 cửa sổ cùng tên game thì danh sách tự tăng lên thành tổng cộng 5 cửa sổ.
5. Khi game đóng, dòng đó vẫn được giữ lại và chuyển thành `OFFLINE`. Khi cửa sổ cùng tên mở lại, phần mềm tự ghép và chuyển về `ONLINE`.
6. Dùng menu chuột phải **Xóa khỏi danh sách** nếu muốn xóa hẳn một dòng, kể cả dòng đang `OFFLINE`.

Để ghi chuỗi thao tác, bấm **Ghi thao tác**, thao tác trong cửa sổ chính, bấm **Dừng ghi**, sau đó dùng **Phát lại**.

Nút Proxy `◉` giải thích và dẫn người dùng tới phương thức cấu hình an toàn. Ứng dụng không giả lập việc áp proxy riêng cho từng game: chức năng đó chỉ hoạt động khi game/launcher hỗ trợ tham số proxy hoặc khi proxy đã được cấu hình trong Windows.

### Xếp chồng và di chuyển cửa sổ

1. Bấm nút biểu tượng lưới ở góc trên bên phải. Tất cả cửa sổ game `ONLINE` sẽ được áp dụng, không cần đánh dấu checkbox.
2. Nhập kích thước vùng game, vị trí cách mép trái `X` và cách mép trên `Y`.
3. Đặt độ lệch mỗi cửa sổ `X = 0`, `Y = 0` để các cửa sổ chồng khít và nhìn như một cửa sổ duy nhất.
4. Có thể nhập độ lệch khác 0 nếu muốn nhìn thấy mép của từng cửa sổ.
5. Bấm **Xác nhận**. Cửa sổ chính sẽ được đưa lên trên cùng.

Bản Windows yêu cầu quyền Administrator khi mở để có thể di chuyển/đổi kích thước các cửa sổ game đang chạy quyền cao. Sau khi sắp xếp, thanh trạng thái sẽ báo số cửa sổ thực sự áp dụng thành công.

### Thanh cửa sổ thu nhỏ

1. Bấm nút biểu tượng thanh thu nhỏ `▤` ở góc trên bên phải.
2. Thanh **Xem cửa sổ thu nhỏ** sẽ mở sát đáy màn hình và tự hiển thị ảnh trực tiếp của mọi cửa sổ Doomsday.
   Thanh có viền và thanh tiêu đề màu xanh, nền vùng thumbnail màu tối giống giao diện tham chiếu.
3. Bấm vào một ảnh thu nhỏ để khôi phục và đưa cửa sổ game đó lên trên cùng.
4. Kéo cạnh thanh để đổi kích thước; bấm lại nút `▤` hoặc nút đóng của thanh để ẩn.

Mỗi lần mở thanh thu nhỏ, danh sách được giữ nguyên thứ tự hiện tại và đánh số lại `Cửa sổ 1`, `Cửa sổ 2`… Các cửa sổ `ONLINE` được đổi luôn tiêu đề Windows, vì vậy tên trên thumbnail cũng đúng thứ tự. Dòng `OFFLINE` có thể nhấp phải và chọn **Xóa khỏi danh sách**; sau khi xóa, mở lại thanh thu nhỏ để đánh số liên tục từ đầu.

## Giới hạn kỹ thuật

- Ứng dụng dùng hook bàn phím/chuột cấp thấp và gửi thông điệp Win32 tới cửa sổ đích.
- Nếu ứng dụng đích chạy bằng quyền Administrator, AutoSync Clean cũng cần chạy cùng mức quyền.
- Một số game dùng Raw Input, DirectInput độc quyền hoặc cơ chế chống gian lận có thể không nhận thông điệp `PostMessage`. Dự án không can thiệp tiến trình, không tiêm DLL và không vượt cơ chế chống gian lận.
- Các cửa sổ nên có cùng tỉ lệ khung hình để tọa độ chuột tương ứng chính xác nhất.
