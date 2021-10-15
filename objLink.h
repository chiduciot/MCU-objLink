#ifndef _OBJ_LINK_H_
#define _OBJ_LINK_H_

#include <Stream.h>
#include <crc8.h>
//#define _DEBUG_
//#define _READ_ANSYNC_

#define BeginSignal 0xAA
#define HeaderSignalIndex 0
#define HeaderLengthIndex 1
#define HeaderTypeIndex 2
#define HeaderSize 2
#define ObjectTypeIndex 2
#define ObjectPINIndex 3
#define ObjectSyncStateIndex 4
#define ObjectDataIndex 5
#define SubHeaderSize 3

#define BufferInSize 64  //
#define BufferOutSize 64 //

// use 1-250 only
enum PacketType : uint8_t
{
    PT_ARRAY = 0,
    PT_BYTE = 1, // sizeof(byte/bool/char) = 1
    PT_INT = 2,  // sizeof(int/uint) = 2
    PT_LONG = 4, // sizeof(long/double/float) = 4

    PT_ACK = 250 /****/
};

enum SyncStates : uint8_t
{
    Synchronized = 0,
    POST = 1,
    GET = 2
};

struct objBase;
typedef void objBaseCallBack(objBase *obj);
class objBase
{
public:
    uint8_t Type;
    uint8_t PIN;
    SyncStates SyncState;
    union
    {
        bool BOOL;
        uint8_t BYTE;
        char CHAR;
        int INT;
        long LONG;
        double FLOAT;
        double DOUBLE;
        uint8_t *ARRAY;
    } Value;

    template <typename T>
    void Set(T data)
    {
        T *tmp = (T *)&Value.BYTE;
        *tmp = data;
        _dataSize = sizeof(T);
        SyncState = SyncStates::POST; 
        if (_callBack)
        {
            _callBack(this);
        }
        //ChangeCallBack();
    }

    void Set(uint8_t *data, uint8_t size)
    {
        free(Value.ARRAY);
        _dataSize = size;
        Value.ARRAY = (uint8_t *)malloc(size);
        memcpy(Value.ARRAY, data, size);
        SyncState = SyncStates::POST; 
        if (_callBack)
        {
            _callBack(this);
        }
        //ChangeCallBack();
    }

    void OnChange(objBaseCallBack *callBack)
    {
        _callBack = callBack;
    };

    void ChangeCallBack()
    {
        if (_callBack)
        {
            _callBack(this);
        }
    }
    virtual uint8_t DataSize()
    {
        if (_dataSize == 0)
        _dataSize = Type;
        return _dataSize;
    };

private:
    objBaseCallBack *_callBack;
    uint8_t _dataSize = 0;
};

struct objNode
{
    objBase *Data;
    objNode *Next;
};

class objLink
{
private:
    uint8_t _inBuffer[BufferInSize];
    uint8_t _outBuffer[BufferOutSize];
    uint8_t _inPos;
    objNode *root;
    objNode *last;
    objNode *current;
    Stream *_stream;

public:
    objLink(Stream *str);
    void Handle();
    void Add(objBase *obj);
    objBase *Get(uint8_t pin);
    void Send(objBase *obj);
};

objLink::objLink(Stream *str)
{
    root = NULL;
    last = NULL;
    current = NULL;
    _inPos = 0;
    _stream = str;
};

void objLink::Add(objBase *obj)
{
    objNode *tmp = new objNode();
    tmp->Data = obj;
    tmp->Next = NULL;
    if (root)
    {
        // Already have elements inserted
        last->Next = tmp;
        last = tmp;
    }
    else
    {
        // First element being inserted
        root = tmp;
        last = tmp;
    }
}

objBase *objLink::Get(uint8_t pin)
{
    objNode *tmp = root;
    while (tmp)
    {
        if (tmp->Data->PIN == pin)
            return tmp->Data;
        tmp = tmp->Next;
    }
    return NULL;
}

void objLink::Send(objBase *obj)
{
    // 4 : Type(1) + PIN(1) + SyncState(1) + CRC(1)
    uint8_t size = obj->DataSize();
    uint8_t len = HeaderSize + size + SubHeaderSize + 1;
    _outBuffer[HeaderSignalIndex] = BeginSignal;
    _outBuffer[HeaderLengthIndex] = len;
    _outBuffer[HeaderSize] = obj->Type;
    _outBuffer[ObjectPINIndex] = obj->PIN;
    _outBuffer[ObjectSyncStateIndex] = obj->SyncState;
    if (obj->Type == PacketType::PT_ARRAY)
    {
        memcpy(&_outBuffer[HeaderSize + SubHeaderSize], obj->Value.ARRAY, size);
    }
    else
    {
        memcpy(&_outBuffer[HeaderSize + SubHeaderSize], &obj->Value.BYTE, size);
    }
    _outBuffer[len - 1] = CRC8(_outBuffer, len - 1);
#ifdef _DEBUG_
    Serial.print(F("Send len : "));
    Serial.println(len);
    for (uint8_t i = 0; i < len; i++)
    {
        if (_outBuffer[i] < 0x10)
            Serial.print("0");
        Serial.print(_outBuffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
#endif
    _stream->write(_outBuffer, len);
}

void objLink::Handle()
{
// read data
#ifdef _READ_ANSYNC_
    if (_stream->available())
#else
    while (_stream->available())
#endif
    {
        uint8_t c = _stream->read();
        if (_inPos == 0)
        {
            if (c == BeginSignal)
            {
                _inBuffer[HeaderSignalIndex] = c;
                _inPos = 1;
            }
        }
        else
        {
            _inBuffer[_inPos] = c;
            if (_inPos == HeaderLengthIndex)
            {
                if ((c > BufferInSize) || (c < (HeaderSize + 1)))
                {
#ifdef _DEBUG_
                    Serial.println(F("Length invalid"));
#endif
                    _inPos = 0;
#ifndef _READ_ANSYNC_
                    break;
#endif
                }
                _inPos++;
            }
            else if (_inPos == _inBuffer[HeaderLengthIndex] - 1)
            {
                _inPos = 0;
                uint8_t crc = CRC8(_inBuffer, _inBuffer[HeaderLengthIndex] - 1);
                if (c == crc)
                {
                    objBase *a = this->Get(_inBuffer[ObjectPINIndex]);
#ifdef _DEBUG_
                    uint8_t len = _inBuffer[HeaderLengthIndex];
                    Serial.print(F("Received len : "));
                    Serial.println(len);
                    for (uint8_t i = 0; i < len; i++)
                    {
                        if (_inBuffer[i] < 0x10)
                            Serial.print("0");
                        Serial.print(_inBuffer[i], HEX);
                        Serial.print(" ");
                    }
                    Serial.println();
#endif
                    if (a != NULL)
                    {
                        if (_inBuffer[ObjectSyncStateIndex] == SyncStates::GET)
                        {
#ifdef _DEBUG_
                            Serial.println(F("GET"));
#endif
                            a->SyncState = SyncStates::POST;
#ifndef _READ_ANSYNC_
                            this->Send(a);
                            a->SyncState = SyncStates::Synchronized;
#endif
                        }
                        else if (_inBuffer[ObjectSyncStateIndex] == SyncStates::POST)
                        {
                            uint8_t size = _inBuffer[HeaderLengthIndex] - 6;

#ifdef _DEBUG_
                            Serial.print(F("POST data size : "));
                            Serial.println(size);
#endif
                            if (_inBuffer[ObjectTypeIndex] == PacketType::PT_ARRAY)
                            {
                                free(a->Value.ARRAY);
                                // 6 = HeaderSize + SubHeaderSize + CRC(1)

#ifdef _DEBUG_
                                Serial.println(F("ARRAY"));
#endif
                                a->Value.ARRAY = (uint8_t *)malloc(size);
                                memcpy(a->Value.ARRAY, &_inBuffer[ObjectDataIndex], size);
                            }
                            else
                            {
#ifdef _DEBUG_
                                Serial.println(F("INT"));
#endif
                                memcpy(&a->Value.BYTE, &_inBuffer[ObjectDataIndex], size);
                            }
#ifdef _DEBUG_
                            Serial.println(F("CallBack"));
#endif
                            a->SyncState = SyncStates::Synchronized;
                            a->ChangeCallBack();
                        }
                    }
#ifndef _READ_ANSYNC_
                    break;
#endif
                }
            }
            else
            {
                _inPos++;
            }
        }
    }

    // send changed data
    if (current != NULL)
    {
        if (current->Data->SyncState != SyncStates::Synchronized)
        {
            this->Send(current->Data);
            current->Data->SyncState = SyncStates::Synchronized;
        }
        current = current->Next;
    }
    else
    {
        current = root;
    }
}

#endif