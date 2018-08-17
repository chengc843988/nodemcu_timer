#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
// #include <NTPClient.h>
#include <WiFiUdp.h>

bool lcdBacklight();

// LCD顯示器
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_Millis rtc;
char datetime[48];

DateTime _now;
uint32_t _ts;
uint32_t _todaySeconds;

//定義wifi
WiFiUDP ntpUDP;


const char *ssid[] = {"MOA-TC", "TVTC-MOA", "TT49"};
const char *password[] = {"22927399", "22592566", "22592566"};
uint8_t currentNetwork = 2;
const uint8_t NETCFG_LEN = 3;


// 透過RTC模組, 讀取時間... 不用一直叫 WIFI+NTP去讀
void takeCurrentTime() {
  _now = rtc.now();
  _ts = _now.unixtime();
  _todaySeconds = _ts % 86400;
}

// 將RTC轉換為字串
char *getDateTimeString() {
  takeCurrentTime();
  sprintf(datetime, "%04d/%02d/%02d %02d:%02d:%02d",
          _now.year(), _now.month(), _now.day(),
          _now.hour(), _now.minute(), _now.second());
  return datetime;
}


// 在LCD上顯示時間
void printDateTimeInfo() {
  getDateTimeString();
  lcd.setCursor ( 0, 0 ); lcd.print( datetime);
}

// 啟動WIFI, 跟SMAPLE相同
bool startWifi(uint8_t networkNo, uint32_t timeout) {
  WiFi.begin(ssid[networkNo], password[networkNo]);  //wifi開始連接
  timeout = millis() + (timeout * 1000);

  Serial.print("start wifi");
  Serial.print(":"); Serial.print(ssid[networkNo]);
  //  Serial.print("/"); Serial.print(password[networkNo]);
  Serial.print(";"); Serial.print(timeout);
  Serial.print(";"); Serial.print(millis());

  while (timeout > millis() && WiFi.status() != WL_CONNECTED ) {
    printDateTimeInfo();
    delay ( 1000 ); Serial.print ( "." );
    // lcd.print('.');
  }
  Serial.print(";"); Serial.print(WiFi.status() != WL_CONNECTED);
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}

// 停用WIFI
bool endWifi() {
  if (WiFi.status() == WL_CONNECTED ) {
    WiFi.mode(WIFI_OFF);
  }
}
###############################################3
# 讀取 NTP值
###############################################
WiFiUDP Udp;

const int NTP_PACKET_SIZE = 48;

byte packetBuffer[ NTP_PACKET_SIZE];
//char timeServer[] = "time.nist.gov";
const unsigned int localPort = 2390;
// IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServer(103, 18, 128, 60); // tw.pool.ntp.org NTP server

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}

// 轉換NTP
uint32_t retriveNtp(uint32_t timeout) {
  bool success;

  sendNTPpacket(timeServer);
  success = false;
  timeout = millis() + timeout * 1000;
  Udp.begin(localPort);
  while (timeout > millis()) {
    if ((success = Udp.parsePacket())) {
      break;
    }
    Serial.print('.');
    delay(1000);
  }
  if (success) {
    Serial.println(" packet received; ");
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;

    Serial.print("Seconds since Jan 1 1900 = ");    Serial.print(secsSince1900);
    Serial.print(" Unix time = "); Serial.println(epoch);

    return epoch;
  }
  return 0;
}

// 將讀回來的NTP值, 寫人RTC中 
bool syncNtpDate() {
  lcd.clear();
  lcd.setCursor(0, 1); lcd.print("WIFI:");  lcd.print( ssid[currentNetwork] );
  bool success ;

  success = false;
  if (success == false) success = startWifi(currentNetwork, 10);
  if (success == false) success = startWifi(currentNetwork, 10);
  if (success == false) success = startWifi(currentNetwork, 10);

  lcd.setCursor(5, 1);
  lcd.print( (success) ? "OK      " : "fail    ");
  if (success == false) {
    currentNetwork = (currentNetwork + 1) % NETCFG_LEN;
    return false;
  }

  Serial.println("Sync NTP: ");
  lcd.setCursor(10, 1); lcd.print("NTP:");
  uint32_t epoch = 0;
  if (epoch == 0) epoch = retriveNtp(10);
  if (epoch == 0) epoch = retriveNtp(10);
  if (epoch == 0) epoch = retriveNtp(10);

  lcd.setCursor(15, 1);
  lcd.print( (epoch == 0) ? "fail" : "OK");

  if (epoch > 0) {
    rtc.adjust(epoch + (8 * 60 * 60));
    // rtc.adjust(epoch + (0 * 60 * 60));
    printDateTimeInfo();
    lcd.setCursor(0, 2);
    lcd.print(datetime);
  }
  endWifi();
  return success;
}


// 啟始設定
void setup() {
  // put your setup code here, to run once:
  rtc.begin(DateTime(F(__DATE__), F(__TIME__)));

  {
    Serial.begin(57600);
    Serial.setTimeout(5000);
    // pinMode(PIN_PIRout, INPUT);
    // digitalWrite(PIN_PIRout, LOW);
    Serial.println();
  }
  {
    //  const uint8_t scl = D6; // defalut I2c
    //  const uint8_t sda = D7;// defalut I2c
    const uint8_t scl = D1;
    const uint8_t sda = D2;
    Wire.begin(sda, scl);
  }
  {
    lcd.begin(20, 4);
    lcd.backlight();
    //  lcd.display();
    lcd.clear();
    printDateTimeInfo();
    Serial.println(datetime);
  }
  {
    syncNtpDate();
  }
}

// 程式主體
void loop() {
  // put your main code here, to run repeatedly:
  
  delay(1000);
  printDateTimeInfo();
  Serial.print(datetime);
  Serial.print(";ts="); Serial.print(_ts);
  Serial.print(";sec="); Serial.print(_todaySeconds);

  Serial.println();
  
}
