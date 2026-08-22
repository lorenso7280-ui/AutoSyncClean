# AutoSync Clean

Ứng dụng dùng biểu tượng chiến binh **MAXIMUS** do người dùng cung cấp cho file EXE, taskbar và các cửa sổ con. Tài nguyên ICO chứa các kích thước 16×16, 32×32 và 48×48 để Windows hiển thị rõ trên thanh taskbar.

Chế độ phát bản ghi mặc định dùng click nền theo `HWND`: gửi `WM_MOUSEMOVE`, `WM_LBUTTONDOWN` và `WM_LBUTTONUP` bằng `PostMessage` đến vùng render/control của từng cửa sổ đã chọn. Phần mềm không gọi `SetCursorPos`, `mouse_event` hay `SendInput` trong chế độ này, vì vậy con trỏ thật vẫn dùng độc lập cho trình duyệt và công việc khác. Tùy chọn máy ảo/khóa chuột của các bản cũ đã được loại bỏ.

Ứng dụng Windows C++ độc lập để đồng bộ thao tác bàn phím và chuột giữa nhiều cửa sổ. Dự án được viết mới dựa trên hành vi quan sát trong video, không chứa hệ thống tài khoản, VIP hoặc key.

## Chức năng

- Khi khởi động, danh sách luôn trống. Kéo nút tròn vào một cửa sổ game sẽ nhận diện đường dẫn file tiến trình và tự quét toàn bộ cửa sổ/tab đang chạy từ cùng file game.
- Việc quét theo tiến trình vẫn hoạt động sau khi tắt hẳn rồi mở lại AutoSync Clean, kể cả khi các thanh tiêu đề game trước đó đã được đổi thành `Cửa sổ 1`, `Cửa sổ 2`…
- Giữ các cửa sổ đã nhận trong danh sách và tự đổi trạng thái `ONLINE`/`OFFLINE` khi game mở hoặc đóng.
- Làm mới trạng thái theo chu kỳ mà không xóa trắng hoặc làm chớp danh sách; vị trí cuộn và dòng đang chọn được giữ nguyên.
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
- Nhận cả click do phần mềm ngoài như **GS Auto Clicker** tạo ra trong Cửa sổ chính. AutoSyncClean chỉ bỏ qua input mang mã nội bộ của chính nó, vì vậy click tự động được phát tới mọi cửa sổ đang tích chọn mà không tạo vòng lặp.
- Quy đổi tọa độ chuột theo tỉ lệ vùng client của từng cửa sổ.
- Xếp chồng các cửa sổ game tại cùng vị trí để nhìn như một cửa sổ, với thông số kích thước, tọa độ X/Y và độ lệch tùy chọn.
- Thanh xem trước thu nhỏ cập nhật trực tiếp cho toàn bộ cửa sổ game; bấm vào ảnh để đưa game tương ứng lên trên.
- Cửa sổ chính khởi động ở kích thước 605×454 pixel giống khung 360Auto tham chiếu; thanh tiêu đề dùng màu xanh và vẫn có thể kéo cạnh để thay đổi kích thước.
- Thanh dưới có nhãn tác giả **Nguyễn Đức Lộc** chữ trắng trên nền xanh lá.
- Thanh xem trước hiển thị tối đa 10 cửa sổ trên mỗi hàng và tự thêm hàng khi có nhiều hơn 10 cửa sổ.
- Sắp xếp/resize và thanh cửa sổ thu nhỏ luôn áp dụng cho toàn bộ cửa sổ game `ONLINE`, không cần tích checkbox; checkbox chỉ quyết định cửa sổ nhận thao tác đồng bộ.
- Ghi và phát lại một chuỗi thao tác trong phiên làm việc.
- Thanh công cụ theo bố cục phần mềm tham chiếu: nhận cửa sổ, mở nhiều cửa sổ và đồng bộ ở bên trái; bản ghi, sắp xếp, Proxy, cửa sổ thu nhỏ và Thiết lập ở bên phải.
- Thanh dưới đã bỏ ba nút quảng bá **Miễn phí**, **Hỗ trợ** và **Cộng đồng**; chỉ giữ thanh trạng thái chạy hết chiều ngang cửa sổ.
- Năm nút bên phải dùng biểu tượng GDI tự vẽ, không phụ thuộc font của máy, theo đúng thứ tự: `R` bản ghi, lưới sắp xếp, quả địa cầu Proxy, màn hình thu nhỏ và bánh răng Thiết lập.
- Nút bản ghi `R` mở cửa sổ **Quản lí bản ghi** với Bắt đầu/Kết thúc, số lần lặp, giãn cách, danh sách bản ghi và chi tiết từng sự kiện.
- Khi mới mở **Quản lí bản ghi**, danh sách luôn trống. Nhấp phải trong khung **Tên bản ghi** để **Thêm bản ghi**, **Xóa bản ghi** hoặc **Xóa tất cả**.
- **Thêm bản ghi** mở cửa sổ **Ghi lại thao tác**. Hộp chọn nguồn luôn liệt kê rõ `Cửa sổ 1`, `Cửa sổ 2`… theo đúng thứ tự danh sách; chỉ cửa sổ đang `ONLINE` mới bắt đầu ghi được.
- Sau khi bấm **Bắt đầu**, chương trình chỉ thu thao tác xảy ra bên trong cửa sổ nguồn đã chọn. Chuyển động con trỏ không được lưu; một click được lưu thành hai sự kiện có thời gian riêng `LEFT MOUSE DOWN` và `LEFT MOUSE UP`, đúng như phần mềm mẫu. Cách này giữ đúng thời gian nhấn-thả để game nhận click ổn định hơn.
- Khi bấm **Kết thúc**, cột **Giá trị** hiển thị tọa độ pixel rõ ràng theo dạng `X: …, Y: …` của từng click trong cửa sổ đã chọn; sự kiện bàn phím hiển thị mã phím. Hộp nguồn dùng font Unicode và luôn ghi đúng `Cửa sổ 1`, `Cửa sổ 2`…
- Bàn phím, cuộn chuột và các click tiếp theo vẫn được ghi theo đúng thứ tự/thời gian. Nhập tên rồi bấm **Lưu**; chỉ bản ghi đã lưu mới xuất hiện trong danh sách với đuôi `.json`.
- Trong cửa sổ quản lí, **Bắt đầu** phát bản ghi đang chọn theo số lần lặp/giãn cách và lập tức đổi thành **Tạm dừng**. Bấm **Tạm dừng** sẽ giữ nguyên vị trí phát và đổi thành **Tiếp tục**; bấm **Kết thúc** dừng hẳn và đưa nút về **Bắt đầu**.
- Giá trị **Lặp lại** từ 1 đến **99.999** và **Giãn cách** được lưu cho lần mở sau (ví dụ nhập `99999` thì lần sau vẫn hiện `99999`). Checkbox trong danh sách chọn đúng một bản ghi để auto-click; bản ghi vừa lưu được tự động chọn.
- Mở **Quản lí bản ghi** hoặc **Thêm bản ghi** sẽ tự tắt Đồng bộ. Ghi và phát auto-click hoạt động độc lập, không yêu cầu bật Đồng bộ và không phát thao tác trực tiếp trong lúc đang tạo bản ghi.
- Trước khi phát, chương trình chụp cố định bản ghi và danh sách mọi cửa sổ `ONLINE` đã tích checkbox (bao gồm cửa sổ nguồn nếu được tích), rồi luồng nền chỉ dùng bản chụp đó. Giao diện thay đổi trong lúc phát không còn làm sai thứ tự hoặc bỏ sót cửa sổ.
- Ở chế độ nền mặc định, chương trình khóa cố định control nhận input bên trong từng cửa sổ game và dùng `PostMessage` tại tọa độ đã ghi; không di chuyển con trỏ hoặc giành focus của game. Chế độ này phù hợp với ứng dụng chấp nhận thông điệp Win32.
- Trước mỗi `MOUSE DOWN` và `MOUSE UP`, chương trình gửi thêm `WM_MOUSEMOVE` ảo đến đúng tọa độ bản ghi trong riêng hàng đợi của game. Sự kiện này cập nhật điểm hover nội bộ (ví dụ nút **TRỢ GIÚP**) nhưng không làm xuất hiện hoặc kéo mũi tên chuột Windows về game; người dùng rê chuột thật sang nơi khác không đổi điểm auto-click.
- **Chế độ máy ảo (khóa chuột)** dùng `SendInput` trong Windows khách cho game không nhận `PostMessage`. Chương trình lần lượt kích hoạt từng cửa sổ game đã chọn, khóa con trỏ khách tại tọa độ đã ghi và phát một click nhấn-thả hoàn chỉnh. Nhấn **F8** để tạm dừng/tiếp tục; nhấn **F9** để kết thúc và mở khóa ngay.
- Chỉ bật chế độ máy ảo khi game và AutoSyncClean cùng chạy trong Windows khách (VirtualBox, VMware hoặc Hyper-V). Khi đó con trỏ của Windows khách bị điều khiển, còn chuột máy chính vẫn độc lập. Nếu bật trực tiếp trên máy chính, chuột máy chính cũng sẽ bị khóa và di chuyển theo bản ghi.
- Chế độ ghi được tách khỏi Đồng bộ nên thao tác lúc ghi không tự phát sang các cửa sổ khác. Khi không bật chế độ máy ảo, phát lại vẫn dùng thông điệp nền và không di chuyển hoặc khóa con trỏ thật.
- Nút `⚙` cho phép bật/tắt riêng chuyển động chuột trong khi vẫn giữ đồng bộ click và bàn phím.
- Cửa sổ **Thiết lập** có thanh FPS từ 1 đến 60, luôn khởi tạo ở mức mặc định 30 FPS. Giá trị này điều chỉnh tần suất truyền sự kiện di chuyển chuột, không can thiệp FPS render bên trong game.
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
3. Cửa sổ được thả và tất cả cửa sổ khác chạy cùng file `.exe` sẽ xuất hiện trong danh sách với trạng thái `ONLINE`.
4. Sau khi đã nhận cửa sổ đầu tiên, nếu mở thêm 4 cửa sổ cùng tên game thì danh sách tự tăng lên thành tổng cộng 5 cửa sổ.
5. Khi game đóng, dòng đó vẫn được giữ lại và chuyển thành `OFFLINE`. Khi cửa sổ cùng tên mở lại, phần mềm tự ghép và chuyển về `ONLINE`.
6. Dùng menu chuột phải **Xóa khỏi danh sách** nếu muốn xóa hẳn một dòng, kể cả dòng đang `OFFLINE`.

Để ghi chuỗi thao tác, bấm **Ghi thao tác**, thao tác trong cửa sổ chính, bấm **Dừng ghi**, sau đó dùng **Phát lại**.

Nút Proxy `◉` giải thích và dẫn người dùng tới phương thức cấu hình an toàn. Ứng dụng không giả lập việc áp proxy riêng cho từng game: chức năng đó chỉ hoạt động khi game/launcher hỗ trợ tham số proxy hoặc khi proxy đã được cấu hình trong Windows.

### Xếp chồng và di chuyển cửa sổ

1. Bấm nút biểu tượng lưới ở góc trên bên phải. Tất cả cửa sổ game `ONLINE` sẽ được áp dụng, không cần đánh dấu checkbox.
2. Chọn kích thước toàn bộ cửa sổ game trong danh sách xổ xuống: `960×540`, `1280×720`, `1600×900` hoặc `1920×1080`, sắp từ nhỏ đến lớn. Mặc định là `960×540`; kích thước bao gồm cả thanh tiêu đề và đường viền, giống cách phần mềm mẫu đo và hiển thị.
3. Đặt độ lệch mỗi cửa sổ `X = 0`, `Y = 0` để các cửa sổ chồng khít và nhìn như một cửa sổ duy nhất.
4. Có thể nhập độ lệch khác 0 nếu muốn nhìn thấy mép của từng cửa sổ.
5. Bấm **Xác nhận**. Cửa sổ chính sẽ được đưa lên trên cùng.

Bản Windows yêu cầu quyền Administrator khi mở để có thể di chuyển/đổi kích thước các cửa sổ game đang chạy quyền cao. Việc sắp xếp được đưa sang tác vụ nền và dùng lệnh di chuyển bất đồng bộ, nên hộp Sắp xếp đóng ngay và giao diện không bị `Not responding` khi game phản hồi chậm. Sau khi hoàn tất, thanh trạng thái sẽ báo số cửa sổ thực sự áp dụng thành công.

### Thanh cửa sổ thu nhỏ

1. Bấm nút biểu tượng thanh thu nhỏ `▤` ở góc trên bên phải.
2. Thanh **Xem cửa sổ thu nhỏ** sẽ mở sát đáy màn hình và tự hiển thị ảnh trực tiếp của mọi cửa sổ Doomsday.
   Thanh dùng một thanh tiêu đề tùy chỉnh cao 22 pixel chạy hết chiều ngang và một đường viền xanh 1 pixel bao quanh toàn bộ cửa sổ. Khung trắng `WS_THICKFRAME` của Windows đã được loại bỏ để chiều cao và màu sắc sát với 360Auto. Nền vùng thumbnail màu tối giống giao diện tham chiếu.
   Thumbnail được xếp theo lưới cố định 10 cửa sổ mỗi hàng, tỷ lệ 16:9 và khe 2 pixel; chiều cao cửa sổ tự ôm vừa số hàng nên không còn khoảng trống lớn phía trên, giữa các hàng hoặc phía dưới.
3. Bấm vào một ảnh thu nhỏ để khôi phục và đưa cửa sổ game đó lên trên cùng.
4. Kéo cạnh thanh để đổi kích thước; bấm lại nút `▤` hoặc nút đóng của thanh để ẩn.

Mỗi lần mở thanh thu nhỏ, danh sách được giữ nguyên thứ tự hiện tại và đánh số lại `Cửa sổ 1`, `Cửa sổ 2`… Các cửa sổ `ONLINE` được đổi luôn tiêu đề Windows, vì vậy tên trên thumbnail cũng đúng thứ tự. Dòng `OFFLINE` có thể nhấp phải và chọn **Xóa khỏi danh sách**; sau khi xóa, mở lại thanh thu nhỏ để đánh số liên tục từ đầu.

## Giới hạn kỹ thuật

- Ứng dụng dùng hook bàn phím/chuột cấp thấp và gửi thông điệp Win32 tới cửa sổ đích.
- Nếu ứng dụng đích chạy bằng quyền Administrator, AutoSync Clean cũng cần chạy cùng mức quyền.
- Một số game dùng Raw Input, DirectInput độc quyền hoặc cơ chế chống gian lận có thể không nhận thông điệp `PostMessage`. Dự án không can thiệp tiến trình, không tiêm DLL và không vượt cơ chế chống gian lận.
- Với các game đó, có thể thử **Chế độ máy ảo (khóa chuột)**. Chế độ này dùng API đầu vào Windows tiêu chuẩn trong máy khách, không tiêm mã vào tiến trình game.
- Các cửa sổ nên có cùng tỉ lệ khung hình để tọa độ chuột tương ứng chính xác nhất.
