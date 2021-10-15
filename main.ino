#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <OneButton.h>
#include "objLink.h"

// Định nghĩa đây là code cho master
// Master chỉ khác Slave 1 đoạn code như bên dưới : line 51-52
/*
    uint8_t pwm = obj->Value.INT >> 2;
    analogWrite(outPWM_PIN, pwm);
*/

#define _MASTER_

// Khai báo input
#define INPUT0 2
#define INPUT1 4
#define OUTPUT0 3
#define OUTPUT1 5
#define inADC_PIN A0
#define outPWM_PIN 6

objLink *oLink;

OneButton button0(INPUT0, true);
OneButton button1(INPUT1, true);

unsigned long time0 = 0;
int oldADC = 0;
objBase in0, in1, inADC;

/****************************************************************************************************/
// những hàm này sẽ được gọi đối tượng tương ứng khi có thay đổi giá trị
// cách 1 : dùng từng hàm riêng.
void in0Onchange(objBase *obj)
{
    digitalWrite(obj->PIN, obj->Value.BYTE);
}

void in1Onchange(objBase *obj)
{
    digitalWrite(obj->PIN, obj->Value.BYTE);
}

void ADCOnchange(objBase *obj)
{
#ifdef _MASTER_
    // nếu là Master thì không làm gì cả
#else
    // nếu là Slave thì wite PWM
    // mặc định ADC 10bit, PWM 8bit
    // nên cần phải ánh xạ 10bit từ 0-1023 thành 8bit từ 0-255
    // dùng hàm out = map(in, 0, 1023, 0, 255)
    // tương đương chia 4, chia 4 tương đương dịch trái 2 bit
    uint8_t pwm = obj->Value.INT >> 2;
    analogWrite(outPWM_PIN, pwm);
#endif
}

// cách 2: dùng 1 hàm chung và kiểm tra theo PIN, Type tùy theo mỗi người
/*
void allOnchange(objBase *obj)
{
    if(obj->PIN == ?? )
    {

    }
    if(obj->Type == ?? )
    {

    }
}
*/

// Hàm này được gọi khi button0 bấm
void button0Click()
{
    uint8_t b = in0.Value.BYTE ^ 1; // toggle
    in0.Set(b); // Set giá trị mới
    //in0.SyncState = SyncStates::POST; // đặt cờ thông báo giá trị đã thay đổi, gửi cho bên kia
}

// Hàm này được gọi khi button1 bấm
void button1Click()
{
    uint8_t b = in1.Value.BYTE ^ 1; // toggle
    in1.Set(b); // Set giá trị mới
    //in1.SyncState = SyncStates::POST; // đặt cờ thông báo giá trị đã thay đổi, gửi cho bên kia
}

//****************************************************************************************************//
int ReadADC(byte pin)
{
    // Đọc 16 lần để giảm sai số nhiễu
    int tmp = 0;
    for (byte i = 0; i < 16; i++)
    {
        tmp += analogRead(pin);
    }
    // Toán tử >> dịch bit qua phải, a >> n = a /(2^n) , a >> 1 = a/(2^1) = a / 2
    // Toán tử << dịch bit qua trái, a << n = a *(2^n) , a << 1 = a*(2^1) = a * 2
    // Dịch qua phải 4 bit, tmp >> 4 = tmp/(2^4) = tmp/16
    return tmp >> 4;
}

void setup()
{
    Serial.begin(57600);

    pinMode(OUTPUT0, OUTPUT);
    pinMode(OUTPUT1, OUTPUT);
    pinMode(outPWM_PIN, OUTPUT);
    button0.attachClick(button0Click);
    button1.attachClick(button1Click);

    // Khởi tạo các đối tượng cần đồng bộ dữ liệu
    in0.PIN = OUTPUT0;
    in0.Type = PacketType::PT_BYTE;
    in0.SyncState = SyncStates::GET; // Đặt cờ thông báo cần lấy giá trị từ bên kia
    in0.OnChange(&in0Onchange);

    in1.PIN = OUTPUT1;
    in1.Type = PacketType::PT_BYTE;
    in1.SyncState = SyncStates::GET; // Đặt cờ thông báo cần lấy giá trị từ bên kia
    in1.OnChange(&in1Onchange);

    inADC.PIN = inADC_PIN;
    inADC.Type = PacketType::PT_INT;
    inADC.SyncState = SyncStates::GET; // Đặt cờ thông báo cần lấy giá trị từ bên kia
    inADC.OnChange(&ADCOnchange);

    // Khởi tạo objLink với phương thức truyền nhận là Serial
    // Có thể thay thế phương thức truyền nhận bất kỳ tương thích với Stream
    oLink = new objLink(&Serial);
    // Thêm các đối tượng cần đồng bộ vào danh sách để tự động đồng bộ
    oLink->Add(&in0);
    oLink->Add(&in1);
    oLink->Add(&inADC);
}

void loop()
{
    // Đọc analog inADC_PIN sau 250ms
    if ((unsigned long)(millis() - time0) > 250)
    {
        int inA0 = ReadADC(inADC_PIN);
        // Nếu giá trị analog khác lần sau cùng thì gửi thông tin đồng bộ
        if (oldADC != inA0)
        {
            oldADC = inA0;
            inADC.Set(inA0);
#ifdef _MASTER_
            //inADC.SyncState = SyncStates::POST; 
#endif
        }
        time0 = millis();
    }

    oLink->Handle();
    button0.tick();
    button1.tick();
}