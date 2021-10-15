Để truyền dữ liệu giữa các vi điều khiển, chúng ta thường dùng các giao tiếp như SPI, I2C, UART, 1-Wire, v.v..
Nhưng hầu hết chúng ta đều chỉ sử dụng để truyền dữ liệu dạng thô (raw) và không kiểm tra lỗi truyền nhận. Và trong các giao tiếp truyền dữ liệu, UART là giao tiếp thông dụng và đơn giản nhất mà hầu hết các vi điều khiển đều có phần cứng UART.
Trong trường hợp vi điều khiển không có phần cứng UART, thì softUART cũng rất đơn giản. Và giao tiếp UART với máy tính hay các thiết bị ngoại vi khác cũng yêu cầu phần cứng đơn giản và có sẵn.
UART có một nhược điểm là có tỉ lệ sai sót khi truyền nhận, tỉ lệ này phụ thuộc vào tốc độ truyền nhận tương ứng với tốc độ xung nhịp của vi xử lý.
Trong hầu hết các trường hợp thì dữ liệu truyền đi là chính xác.
Nhưng có nhiều ứng dụng quan trọng, yêu cầu dữ liệu truyền đi phải chính xác tuyệt đối đó là lý do các thiết bị điều khiển công nghiệp thường truyền với tốc độ rất thấp 9600 baud, 19200 baud (PLC, máy CNC). Và thậm chí còn có thêm các tính toán để kiểm tra lỗi, mà thông dụng nhất là tính tổng (Check Sum), tính chẵn lẻ (Parity), CRC (Cyclic Redundancy Check - cái này tôi không dịch ra tiếng Việt vì nó là từ chuyên ngành, dịch ra rất khó hiểu).
Việc gửi nhận dữ liệu qua UART thì quá đơn giản, nhưng vấn đề là chúng ta cần nhận biết dữ liệu là gì, dữ liệu được truyền đi có đúng không .v.v...
Do đó tôi có viết 1 thư viện nhỏ để tự đồng đồng bộ dữ liệu giữa 2 vi điều khiển, dữ liệu truyền nhận được kiểm tra lỗi bằng CRC.

Sử dụng cực kỳ đơn giản :

    #include "objLink.h"

    //Khai báo 1 con trỏ objLink
    objLink *oLink;

    //Khai báo 1 objBase là đối tượng dữ liệu ta muốn đồng bộ
    objBase in0;

    //Hàm callback khi dữ liệu thay đổi giá trị
    //thay đổi này nhận được từ UART hoặc từ in0.Set(<data>);
    void in0Onchange(objBase *obj)
    {
    digitalWrite(obj->PIN, obj->Value.BYTE);
    }
    
    ...
    
    void setup()
    {

    //Khởi tạo 1 objLink với phương thức truyền nhận là Serial.
    oLink = new objLink(&Serial);
    
    //3 ID/PIN để định danh đối tượng đồng bộ giá trị từ 0 - 255, đơn giản là số PIN của Arduino tương ứng
    in0.PIN = 3;	

    //Kiểu của đối tượng là PT_BYTE (1 byte), PT_INT (2 byte), PT_LONG (4 byte), PT_ARRAY (mảng dữ liệu byte/ char)
    in0.Type = PacketType::PT_BYTE;
    
    // Đặt cờ thông báo cần lấy giá trị từ bên kia khi mới khởi động
    in0.SyncState = SyncStates::GET;
    
    // Thêm đối tượng cần đồng bộ vào danh sách để tự động đồng bộ
    oLink->Add(&in0);
    }

    //Khi muốn gán giá trị mới chúng ta dùng in0.Set(<data>);
    //ví dụ in0.Set(1);
    //khi đó hàm in0Onchange() sẽ dược gọi và obj->Value.BYTE sẽ là 1
    //và đương nhiên digitalWrite(obj->PIN, obj->Value.BYTE); sẽ là digitalWrite(3, 1);
    //đồng thời thay đổi này được gửi qua UART cho vi xử lý B
    //vi xử lý B nhận được dữ liệu và cũng gọi hàm in0Onchange() để xử lý dữ liệu nhận được nếu bạn khai báo như bên này

    void loop()
    {
    ...
    //Xử lý đồng bộ dữ liệu giữa 2 vi điều khiển
    oLink->Handle();
    }
