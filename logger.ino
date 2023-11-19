#include <Arduino.h>

//*************************************
// การเดินสายไฟ / การจัดสรร PIN 
// 
//*************************************
// SD ใช้การแมปพินต่อไปนี้
// [CS,MOSI,MISO,CLK] = [D10,D11,D12,D13]


// ตั้งค่าการใช้งาน SD Card

#ifndef _DEBUG // ถ้าไม่ได้กำหนด _DEBUG ให้กำหนดเป็น _RELEASE
#define _DEBUG // กำหนด _DEBUG ให้เป็น _RELEASE
#endif

#define UPDATE_INTERVAL_ms 1000 // กำหนดระยะเวลาการอัพเดทข้อมูล 1000 ms = 1 วินาที

#define NO_SD_CARD      // ถ้าไม่มี SD Card ให้กำหนด NO_SD_CARD
#define SD_SAFE_WRITE   // ถ้าต้องการให้เขียนข้อมูลลง SD Card ให้กำหนด SD_SAFE_WRITE

#if (_DEBUG == 0)           // ถ้ากำหนด _DEBUG ให้เป็น _RELEASE
#define debugPrint(...)     // ถ้าไม่ได้กำหนด _DEBUG ให้ไม่มีการแสดงผล
#define debugPrintln(...)   // ถ้าไม่ได้กำหนด _DEBUG ให้ไม่มีการแสดงผล
#else
#define debugPrint(...) Serial.print(__VA_ARGS__)       // ถ้ากำหนด _DEBUG ให้มีการแสดงผล
#define debugPrintln(...) Serial.println(__VA_ARGS__)   // ถ้ากำหนด _DEBUG ให้มีการแสดงผล
#endif

// มาโครเพื่อรวม/ยกเว้นโค้ดดีบักได้อย่างง่ายดาย
uint32_t lastUpdate = 0;        // ตัวแปรเก็บค่าเวลาล่าสุดที่อัพเดทข้อมูล
uint32_t lastUpdateFirst = 0;   // ตัวแปรเก็บค่าเวลาล่าสุดที่อัพเดทข้อมูล


#include <SPI.h>    // ใช้งาน SPI
#include <SD.h>     // ใช้งาน SD Card

File dataFile;

uint16_t Numfiles = 0;              // จำนวนไฟล์ทั้งหมดใน SD Card
char LOG_FILENAME[13] = {"LOG.TXT"}; // ชื่อไฟล์ที่ใช้เก็บข้อมูล
bool SD_Card = false;               // ตัวแปรเก็บสถานะการเชื่อมต่อ SD Card

// การประกาศฟังก์ชัน SD

void logFilename(const uint16_t &_num, char (&_filename)[13]); // ฟังก์ชันสร้างชื่อไฟล์
uint16_t get_log_count(File dir);
void logTempSensor(float &temp_degC);

// เซ็นเซอร์อุณหภูมิ
#include <OneWire.h>    // https://github.com/PaulStoffregen/OneWire.git
OneWire ds(9);          // ใช้งานขา D9 สำหรับเซ็นเซอร์อุณหภูมิ
uint8_t present = 0;    // ตัวแปรเก็บสถานะการเชื่อมต่อเซ็นเซอร์อุณหภูมิ
uint8_t type_s;         // ตัวแปรเก็บประเภทของเซ็นเซอร์อุณหภูมิ
uint8_t data[12];       // ตัวแปรเก็บข้อมูลที่อ่านได้จากเซ็นเซอร์อุณหภูมิ
uint8_t addr[8];        // ตัวแปรเก็บข้อมูลที่อ่านได้จากเซ็นเซอร์อุณหภูมิ

//  ประกาศฟังก์ชันเซ็นเซอร์อุณหภูมิ

bool setupTempSensor();
float readTempSensor();

// การประกาศฟังก์ชัน


void setup() // ฟังก์ชัน setup จะทำงานครั้งเดียวเมื่อเริ่มต้นการทำงาน
{
        //  SD
    #ifndef NO_SD_CARD
        //  เริ่มต้นการ์ด SD
        if (SD.begin(SS))
        {
            SD_Card = true;
        }
        dataFile = SD.open("/");
        Numfiles = get_log_count(dataFile);
        dataFile.close();
    #else
        Numfiles = 0;
    #endif
        //  สร้างชื่อไฟล์บันทึกที่เพิ่มขึ้นอัตโนมัติล่วงหน้า
        logFilename(++Numfiles, LOG_FILENAME);
        //  เปิดไฟล์บันทึกใหม่เพื่อเขียน
    #ifndef NO_SD_CARD
        dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
        dataFile.print(",");
        #if SD_SAFE_WRITE
            dataFile.close();
    #endif
    #endif

    // ดีบัก
    // เริ่มพอร์ตอนุกรม (สำหรับการดีบัก)

    #if (_DEBUG > 0)
        Serial.begin(900000);
    
    debugPrint("Log File: ");
    debugPrintln(LOG_FILENAME);

    #endif

    // เซ็นเซอร์อุณหภูมิ

    // ติดตั้งเซ็นเซอร์อุณหภูมิ

    setupTempSensor();

    // บันทึกเซ็นเซอร์อุณหภูมิแรกอ่านที่นี่ (ตั้งค่าบันทึกแรก - lastUpdateFirst)

    lastUpdateFirst = millis();
    float temp_degC = readTempSensor();
    logTempSensor(temp_degC);
    lastUpdateFirst = lastUpdate;
}

// LOOP
void loop()
{
    if (millis() - lastUpdate >= UPDATE_INTERVAL_ms)
    {
        lastUpdate = millis();
        float temp_degC = readTempSensor();
        logTempSensor(temp_degC);
    }
}

// SD FUNCTION

void logFilename(const uint16_t &_num, char (&_filename)[13])
{
    uint16_t num_ = _num;
    const uint8_t th = num_ / 1000; num_ -= th*1000;
	const uint8_t hu = num_ / 100;  num_ -= hu*100;
	const uint8_t te = num_ / 10;  num_ -= te*10;

	_filename[0] = 'L';
	_filename[1] = 'O';
	_filename[2] = 'G';
	_filename[3] = '_';
	_filename[4] = '0'+th;
	_filename[5] = '0'+hu;
	_filename[6] = '0'+te;
	_filename[7] = '0'+num_;
	_filename[8] = '.';
	_filename[9] = 'T';
	_filename[10] = 'X';
	_filename[11] = 'T';
	_filename[12] = '\0'; 
}
uint16_t get_log_count(File dir)
{
    uint16_t count = 0, t_count;

    debugPrintln("+--------------");
	debugPrintln("| SD files");
	debugPrintln("+--------------");

    while(true)
	{
        File entry =  dir.openNextFile();
        if (! entry)
        {
            debugPrintln("+--------------");
			debugPrint("| ");
			debugPrint(count); debugPrintln(" log files");
			debugPrintln("+--------------");

            return count;
        }

        // ตรวจสอบว่าเป็นไฟล์บันทึกหรือไม่
        char * filename = entry.name();
        #if (_DEBUG == 1)
		debugPrint("| ");
        #endif
        if (strncmp(filename, "LOG_", 4) == 0)
        {
            t_count = filename[4] - '0'; // thousands
			t_count =10*t_count + (filename[5] - '0'); // hundreds
			t_count =10*t_count + (filename[6] - '0'); // tens
			t_count =10*t_count + (filename[7] - '0'); // units
			
			count = max(count, t_count);
			debugPrint("* ");
        }
        else
        {
            debugPrint("  ");
        }
        debugPrintln(entry.name());

        entry.close();
    }
}

void logTempSensor(float &temp_degC)
{
    #ifndef NO_SD_CARD
    #if SD_SAFE_WRITE
    dataFile = SD.open(LOG_FILENAME, O_APPEND); // หากไม่พบ O_APPEND ให้เปลี่ยนเป็น 0x04 (กำหนดบรรทัดที่ 65 ของ SdFat.h)
    #if SD_SAFE_WRITE
    dataFile.print(",");
    dataFile.printIn(temp_degC);
    #if SD_SAFE_WRITE
    dataFile.close();
    #endif
    #endif

    // ดีบัก
    debugPrint(millis() - lastUpdateFirst);
    debugPrint(",");
    debugPrintln(temp_degC);
}

// เซ็นเซอร์อุณหภูมิ FUNCTION

bool setupTempSensor()
{
    present = 0;

    // ค้นหาที่อยู่เซ็นเซอร์อุณหภูมิ
    if (!ds.search(addr))
    {
        debugPrintln("No more addresses.");
        debugPrintln();
        ds.reset_search();
        delay(250);
        return false;
    }
    // พิมพ์ที่อยู่เซ็นเซอร์อุณหภูมิ
    #if (_DEBUG > 0)
        debugPrint("ROM =");
    for(uint8_t i = 0; i < 8; ++i)
    {
        Serial.write(' ');
        debugPrint(addr[i], HEX);
    }
    #endif

    // CRC ตรวจสอบที่อยู่

    if (OneWire::crc8(addr, 7) != addr[7])
    {
        debugPrintln("CRC is not valid!");
        return false;
    }
    debugPrintln();

    // ระบุชิปเซ็นเซอร์อุณหภูมิ
    // ROM แรก uint8_t ระบุว่าชิปตัวใด

    switch (addr[0]) {
        case 0x10:
            debugPrintln("  Chip = DS18S20");  // or old DS1820
            type_s = 1;
            break;
        case 0x28:
            debugPrintln("  Chip = DS18B20");
            type_s = 0;
            break;
        case 0x22:
            debugPrintln("  Chip = DS1822");
            type_s = 0;
            break;
        default:
            debugPrintln("Device is not a DS18x20 family device.");
            return false;
    }
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);

    delay(1000); // บางที 750ms ก็เพียงพอแล้วอาจจะไม่

    return true;
}

float readTempSensor()
{
    /*
        * หมายเหตุ : อาจจำเป็นต้องอ่านแต่ละครั้งหรือไม่ ขึ้นอยู่กับการตั้งค่าการแปลง
        เริ่มการแปลงโดยเปิดพลังของปรสิตในตอนท้าย
        ds.รีเซ็ต();
        ds.select(เพิ่ม);
        ds.write(0x44, 1);
    
        ล่าช้า (1,000); // บางที 750ms ก็เพียงพอแล้วอาจจะไม่
        เราอาจทำ ds.depower() ที่นี่ แต่การรีเซ็ตจะจัดการมัน
    */

    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE); // อ่านสแครชแพด

    debugPrint("  Data = ");
    debugPrint(present, HEX);
    debugPrint(" ");

    for (uint8_t i = 0; i < 9; ++i)
    {
        data[i] = ds.read();
        debugPrint(data[i], HEX);
        debugPrint(" ");
    }
    debugPrint(" CRC=");
    debugPrint(OneWire::crc8(data, 8), HEX);
    debugPrintln();

    int16_t raw = (data[1] << 8) | data[0];
    if (type_s)
    {
        raw = raw << 3; // 9 บิต ความละเอียดเริ่มต้น
        if (data[7] == 0x10)
        {
        // "นับคงอยู่" ให้ความละเอียดเต็ม 12 บิต
        raw = (raw & 0xFFF0) + 12 - data[6];
        }
    }
    else
    {
        uint8_t cfg = (data[4] & 0x60);
        // ที่ความละเอียดต่ำ บิตต่ำจะไม่ได้กำหนดไว้ ดังนั้นให้มันเป็นศูนย์กัน
        if (cfg == 0x00) raw = raw & ~7;        // 9 บิต ความละเอียด 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3;   // 10 บิต ความละเอียด 187.5 มิลลิวินาที
        else if (cfg == 0x40) raw = raw & ~1;   // 11 บิต ความละเอียด 375 มิลลิวินาที
        // ค่าเริ่มต้นคือความละเอียด 12 บิต เวลาในการแปลง 750 มิลลิวินาที
    }
    float celsius = (float)raw / 16.0;
    // ลอยฟาเรนไฮต์ = เซลเซียส * 1.8 + 32.0;
    debugPrint("  Temperature = ");
    debugPrint(celsius);
    debugPrintln(" Celsius");
    
    return celsius;
}