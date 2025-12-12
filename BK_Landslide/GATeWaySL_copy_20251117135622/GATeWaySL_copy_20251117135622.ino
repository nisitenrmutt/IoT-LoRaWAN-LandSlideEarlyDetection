// ========= MasterNode (Receiver) =========
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>     // OLED
#include <Adafruit_SH110X.h>  // OLED
#include <SD.h>               // SD card
#include <NTPClient.h>        // ดึงเวลาจาก Internet
#include <WiFiUdp.h>

#define ss   5   // LoRa
#define rst  14  // LoRa
#define dio0 2   // LoRa

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // Oled

// WiFi & Google Script & Line
const char *ssid = "K-Boonyarit";
const char *password = "88888888";
const char *scriptUrl = "https://script.google.com/macros/s/AKfycbx_T6yzRmdq8TGvktIMMkUVtOnun_s427yomvO5K1EQC5tZwbpYuVGdqPyNCsd5pRzw9Q/exec";
const char *lineNotifyToken = "BGR69oe8oaqptbEURA87jChxilunXKKWoJBPYRHU5LJ";

bool wifiConnected = false;
String deviceName;     // ใช้ String เก็บชื่อ node
const int CS = 4;      // SD card CS PIN

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000); // +7 ชม.

// ---------- สำหรับเก็บ RSSI / Count ล่าสุด และ timeout ----------
int lastRssi = 0;
int lastPacketCount = 0;
int cumulativeLostPackets = 0;      // สะสมจำนวน packet ที่หล่นทั้งหมด
unsigned long lastPacketMillis = 0;
bool hasLastPacket = false;
bool linkTimeoutShown = false;
const unsigned long PACKET_TIMEOUT_MS = 300000UL; // 5 นาที

// ฟังก์ชันเพื่อแปลง Unix time เป็นวันเดือนปี
String getFormattedDateTime() {
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  unsigned long minutes = (rawTime % 3600) / 60;
  unsigned long seconds = rawTime % 60;

  int year = 1970;
  unsigned long days = rawTime / 86400L;
  while ((days >= 365 && !(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) ||
         (days >= 366 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))) {
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
      days -= 366;
    } else {
      days -= 365;
    }
    year++;
  }

  int month = 0;
  const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  while (days >= (daysInMonth[month] + (month == 1 && year % 4 == 0 &&
         (year % 100 != 0 || year % 400 == 0)))) {
    days -= (daysInMonth[month] + (month == 1 && year % 4 == 0 &&
           (year % 100 != 0 || year % 400 == 0)));
    month++;
  }

  int day = days + 1;

  char dateBuffer[11];
  sprintf(dateBuffer, "%04d-%02d-%02d", year, month + 1, day);

  char timeBuffer[9];
  sprintf(timeBuffer, "%02d:%02d:%02d", hours, minutes, seconds);

  return String(dateBuffer) + " " + String(timeBuffer);
}

// ฟังก์ชันสำหรับส่งข้อมูลไปยัง Line Notify
void sendLineNotify(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://notify-api.line.me/api/notify");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", "Bearer " + String(lineNotifyToken));

    String postData = "message=" + message;

    int httpResponseCode = http.POST(postData);

    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY ||
        httpResponseCode == HTTP_CODE_FOUND) {
      String newUrl = http.getLocation();
      Serial.print("Redirected to: ");
      Serial.println(newUrl);

      http.begin(newUrl);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      http.addHeader("Authorization", "Bearer " + String(lineNotifyToken));

      httpResponseCode = http.POST(postData);
      Serial.print("New HTTP Response Code: ");
      Serial.println(httpResponseCode);
    }

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error sending Line Notify: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// ===== บันทึก ลงใน SD card =====
void WriteAnalogValueToFile(const char * path,
                            float RoX, float RoY, float RoZ,
                            float AccX, float AccY, float AccZ,
                            int Rain, int Vi,
                            int Soil20cm, int Soil40cm, int Soil60cm,
                            float t, float h,
                            int packetCount, int rssi,
                            int lostPackets, int totalLost)
{
  File myFile = SD.open(path, FILE_APPEND);
  if (myFile) {
    myFile.print("DateTime: ");
    myFile.print(getFormattedDateTime());
    myFile.print("  DeviceName: ");
    myFile.print(deviceName);
    myFile.print("  count: ");
    myFile.print(packetCount);
    myFile.print("  RSSI: ");
    myFile.print(rssi);
    myFile.print(" dBm");
    myFile.print("  lostPackets: ");
    myFile.print(lostPackets);
    myFile.print("  totalLost: ");
    myFile.print(totalLost);
    myFile.print("  Rotation_x: ");
    myFile.print(RoX);
    myFile.print("  Rotation_y: ");
    myFile.print(RoY);
    myFile.print("  Rotation_z: ");
    myFile.print(RoZ);
    myFile.print("  Acceleration_x: ");
    myFile.print(AccX);
    myFile.print("  Acceleration_y: ");
    myFile.print(AccY);
    myFile.print("  Acceleration_z: ");
    myFile.print(AccZ);
    myFile.print("  raindrop: ");
    myFile.print(Rain);
    myFile.print("  vibration: ");
    myFile.print(Vi);
    myFile.print("  soil20cm: ");
    myFile.print(Soil20cm);
    myFile.print("  soil40cm: ");
    myFile.print(Soil40cm);
    myFile.print("  soil60cm: ");
    myFile.print(Soil60cm);
    myFile.print("  temp: ");
    myFile.print(t);
    myFile.print("  Humidity: ");
    myFile.print(h);
    myFile.print("\n");
    myFile.close();
  } else {
    Serial.println("error opening file ");
    Serial.println(path);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // OLED
  Wire.begin(21, 22); // SDA, SCL
  if (!display.begin(0x3C, OLED_RESET)) {
    Serial.println(F("SH1106 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);    // <<< ปิดการขึ้นบรรทัดอัตโนมัติ กันข้อความซ้อน
  display.setCursor(16, 26);
  display.println("Initializing...");
  display.display();
  delay(2000);

  // LoRa แบบโค้ดเก่าที่เคยทำงานได้
  Serial.println("LoRa Receiver");
  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(920E6)) {  // ความถี่ที่ใช้ LoRa (ให้ตรงกับ Sender)
    Serial.println("LoRa init failed, retry...");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setSpreadingFactor(7);
  Serial.println("LoRa Initializing OK!");

  display.clearDisplay();
  display.setCursor(4, 26);
  display.println("LoRa Initializing OK");
  display.display();
  delay(2000);
  display.clearDisplay();

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    display.clearDisplay();
    display.setCursor(8, 26);
    display.println("Connecting to WiFi");
    display.display();
    delay(1500);
    display.clearDisplay();
  }
  wifiConnected = true;
  if (wifiConnected) {
    Serial.println("Connected to Wi-Fi!");
    display.clearDisplay();
    display.setCursor(8, 26);
    display.println("Connected to Wi-Fi");
    display.display();
    delay(2000);
    display.clearDisplay();
    timeClient.begin();
  } else {
    Serial.println("Failed to connect to Wi-Fi!");
  }

  // SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  display.clearDisplay();
  display.setCursor(22, 26);
  display.println("Setup Complete");
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  timeClient.update();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    int rssi = LoRa.packetRssi();

    while (LoRa.available()) {
      String jsonData = LoRa.readString();
      Serial.print("Received JSON: ");
      Serial.println(jsonData);

      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, jsonData);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      // ====== อ่านข้อมูลจาก JSON ฝั่ง Sender (payload ย่อ) ======
      deviceName = doc["dn"].as<String>();
      int packetCount = doc["c"];    // ตัวนับแพ็กเก็ต

      float RoX = doc["rx"];         // rotation deg
      float RoY = doc["ry"];
      float RoZ = doc["rz"];

      float AccX = doc["ax"];        // angular rate
      float AccY = doc["ay"];
      float AccZ = doc["az"];

      int Soil20cm = doc["s20"];
      int Soil40cm = doc["s40"];
      int Soil60cm = doc["s60"];
      int Rain     = doc["rd"];
      int Vi       = doc["vi"];
      int h        = doc["h"];
      int t        = doc["t"];

      // ====== คำนวณ packet หล่น จาก count ======
      int lostPackets = 0;
      if (hasLastPacket) {
        if (packetCount > lastPacketCount + 1) {
          lostPackets = packetCount - lastPacketCount - 1;
        } else if (packetCount <= lastPacketCount) {
          // ตัวส่ง reset หรือนับใหม่ → ถือว่าเริ่ม session ใหม่
          lostPackets = 0;
        }
      }
      cumulativeLostPackets += lostPackets;

      // อัปเดตสถานะล่าสุด
      lastRssi = rssi;
      lastPacketCount = packetCount;
      lastPacketMillis = millis();
      hasLastPacket = true;
      linkTimeoutShown = false;

      // ====== Debug Serial ======
      Serial.println(getFormattedDateTime());
      Serial.print("Device Name: ");
      Serial.println(deviceName);
      Serial.print("Packet count: ");
      Serial.println(packetCount);
      Serial.print("LostPackets(this): ");
      Serial.println(lostPackets);
      Serial.print("TotalLost: ");
      Serial.println(cumulativeLostPackets);

      Serial.print("Rotation X: ");
      Serial.print(RoX);
      Serial.print(" , Rotation Y: ");
      Serial.print(RoY);
      Serial.print(" , Rotation Z: ");
      Serial.println(RoZ);

      Serial.print("Acceleration X: ");
      Serial.print(AccX);
      Serial.print(" , Acceleration Y: ");
      Serial.print(AccY);
      Serial.print(" , Acceleration Z: ");
      Serial.println(AccZ);

      Serial.print("Vibration: ");
      Serial.print(Vi);
      Serial.println(" %");
      Serial.print("Raindrop: ");
      Serial.print(Rain);
      Serial.println(" %");
      Serial.print("Soil20cm: ");
      Serial.print(Soil20cm);
      Serial.println(" %");
      Serial.print("Soil40cm: ");
      Serial.print(Soil40cm);
      Serial.println(" %");
      Serial.print("Soil60cm: ");
      Serial.print(Soil60cm);
      Serial.println(" %");
      Serial.print("Humidity = ");
      Serial.print(h);
      Serial.println(" %");
      Serial.print("Temperature = ");
      Serial.print(t);
      Serial.println(" °C");
      Serial.print("RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");

      // ====== Line Notify หลัก ======
      sendLineNotify(
        "Device: " + deviceName + "\n"
        "Count: " + String(packetCount) + "\n"
        "Rotation X: " + String(RoX) + " ํ\n"
        "Rotation Y: " + String(RoY) + " ํ\n"
        "Rotation Z: " + String(RoZ) + " ํ\n"
        "Ax: " + String(AccX) + "\n"
        "Ay: " + String(AccY) + "\n"
        "Az: " + String(AccZ) + "\n"
        "Vibration: " + String(Vi) + " \n"
        "Raindrop: " + String(Rain) + " \n"
        "Soil 20cm: " + String(Soil20cm) + " \n"
        "Soil 40cm: " + String(Soil40cm) + " \n"
        "Soil 60cm: " + String(Soil60cm) + " \n"
        "Humidity: " + String(h) + " \n"
        "Temperature: " + String(t) + " ํC\n"
        "Lost(this): " + String(lostPackets) + " TotalLost: " + String(cumulativeLostPackets)
      );

      // ====== Logic มุมล้ม (เหมือนเดิม) ======
      float xd = 15;
      float rr = 5;
      float cc = 50;

      if (abs(RoX - 0) <= xd && abs(RoY - 0) <= xd) {
        sendLineNotify("อุปกรณ์ตั้งปกติ");
      }
      else if (abs(RoX + 30) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("เอียงขวาที่30องศา");
      }
      else if (abs(RoX + 40) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("เอียงขวาที่40องศา");
      }
      else if (abs(RoX + 50) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("เอียงขวาที่50องศา");
      }
      else if (abs(RoX + 60) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("เอียงขวาที่60องศา");
      }
      else if (abs(RoX + 70) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("ล้มขวาที่70องศา");
      }
      else if (abs(RoX + 80) <= rr && abs(RoY + 0) <= rr) {
        sendLineNotify("ล้มขวาที่80องศา");
      }
      else if (abs(RoX + 90) <= rr && abs(RoY - 120) <= cc) {
        sendLineNotify("ล้มขวาที่90องศา");
      }
      else if (abs(RoX - 30) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("เอียงซ้ายที่30องศา");
      }
      else if (abs(RoX - 40) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("เอียงซ้ายที่40องศา");
      }
      else if (abs(RoX - 50) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("เอียงซ้ายที่50องศา");
      }
      else if (abs(RoX - 60) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("เอียงซ้ายที่60องศา");
      }
      else if (abs(RoX - 70) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("ล้มซ้ายที่70องศา");
      }
      else if (abs(RoX - 80) <= rr && abs(RoY + 0) <= xd) {
        sendLineNotify("ล้มซ้ายที่80องศา");
      }
      else if (abs(RoX - 90) <= rr && abs(RoY - 120) <= cc) {
        sendLineNotify("ล้มซ้ายที่90องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 30) <= rr) {
        sendLineNotify("เอียงหน้าที่30องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 40) <= rr) {
        sendLineNotify("เอียงหน้าที่40องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 50) <= rr) {
        sendLineNotify("เอียงหน้าที่50องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 60) <= rr) {
        sendLineNotify("เอียงหน้าที่60องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 70) <= rr) {
        sendLineNotify("ล้มหน้าที่70องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY + 80) <= rr) {
        sendLineNotify("ล้มหน้าที่80องศา");
      }
      else if (abs(RoX + 120) <= cc && abs(RoY + 90) <= rr) {
        sendLineNotify("ล้มหน้าที่90องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 30) <= rr) {
        sendLineNotify("เอียงหลังที่30องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 40) <= rr) {
        sendLineNotify("เอียงหลังที่40องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 50) <= rr) {
        sendLineNotify("เอียงหลังที่50องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 60) <= rr) {
        sendLineNotify("เอียงหลังที่60องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 70) <= rr) {
        sendLineNotify("ล้มหลังที่70องศา");
      }
      else if (abs(RoX - 0) <= xd && abs(RoY - 80) <= rr) {
        sendLineNotify("ล้มหลังที่80องศา");
      }
      else if (abs(RoX + 120) <= cc && abs(RoY - 90) <= rr) {
        sendLineNotify("ล้มหลังที่90องศา");
      }

      // Reconnect WiFi ถ้าหลุด
      if (!wifiConnected) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          delay(1000);
          Serial.println("Connecting to WiFi...");
        }
        wifiConnected = true;
      }

      // ====== บันทึกลง SD Card ======
      WriteAnalogValueToFile("/test.txt",
                             RoX, RoY, RoZ,
                             AccX, AccY, AccZ,
                             Rain, Vi,
                             Soil20cm, Soil40cm, Soil60cm,
                             t, h,
                             packetCount, rssi,
                             lostPackets, cumulativeLostPackets);

      // ====== ส่งค่าลง Google Sheet ======
      String url = scriptUrl;
      url += "?deviceName=" + deviceName;
      url += "&Rotation_xNode1=" + String(RoX);
      url += "&Rotation_yNode1=" + String(RoY);
      url += "&Rotation_zNode1=" + String(RoZ);
      url += "&Acceleration_xNode1=" + String(AccX);
      url += "&Acceleration_yNode1=" + String(AccY);
      url += "&Acceleration_zNode1=" + String(AccZ);
      url += "&raindropNode1=" + String(Rain);
      url += "&vibrationNode1=" + String(Vi);
      url += "&soil%2020cmNode1=" + String(Soil20cm);
      url += "&soil%2040cmNode1=" + String(Soil40cm);
      url += "&soil%2060cmNode1=" + String(Soil60cm);
      url += "&Temperature_xNode1=" + String(t);
      url += "&Humidity_xNode1=" + String(h);
      url += "&CountNode1=" + String(packetCount);
      url += "&RSSINode1=" + String(rssi);
      url += "&LostPacketsNode1=" + String(lostPackets);
      url += "&TotalLostNode1=" + String(cumulativeLostPackets);

      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.println("Error in HTTP GET");
      }
      http.end();

      // ====== แสดงบน OLED (layout ใหม่ ไม่ซ้อน) ======
      display.clearDisplay();

      // คอลัมน์ซ้าย
      display.setCursor(0, 0);
      display.print("Name:");
      display.println(deviceName);

      display.setCursor(0, 8);
      display.print("Cnt:");
      display.println(packetCount);

      display.setCursor(0, 16);
      display.print("RSSI:");
      display.print(rssi);
      display.println("dB");

      display.setCursor(0, 24);
      display.print("L:");
      display.print(lostPackets);
      display.print(" T:");
      display.println(cumulativeLostPackets);

      display.setCursor(0, 32);
      display.print("Rx:");
      display.println(RoX, 1);

      display.setCursor(0, 40);
      display.print("Ry:");
      display.println(RoY, 1);

      display.setCursor(0, 48);
      display.print("Rz:");
      display.println(RoZ, 1);

           // คอลัมน์ขวา
      display.setCursor(64, 0);
      display.print("Rain:");
      display.println(Rain);

      display.setCursor(64, 8);
      display.print("Vib:");
      display.println(Vi);

      display.setCursor(64, 16);
      display.print("S20:");
      display.println(Soil20cm);

      display.setCursor(64, 24);
      display.print("S40:");
      display.println(Soil40cm);

      display.setCursor(64, 32);
      display.print("S60:");
      display.println(Soil60cm);

      display.setCursor(64, 40);
      display.print("Hum:");
      display.println(h);

      display.setCursor(64, 48);
      display.print("Temp:");
      display.println(t);

      display.display();
    }
  }

  // ถ้าเกินเวลาไม่ได้รับ packet -> แสดง last RSSI + last count
  if (hasLastPacket && !linkTimeoutShown) {
    unsigned long now = millis();
    if (now - lastPacketMillis > PACKET_TIMEOUT_MS) {
      Serial.println("No LoRa packets received within timeout.");
      Serial.print("Last RSSI: ");
      Serial.print(lastRssi);
      Serial.print(" dBm, Last Count: ");
      Serial.println(lastPacketCount);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("No LoRa packets!");
      display.setCursor(0, 16);
      display.print("Last RSSI: ");
      display.print(lastRssi);
      display.println(" dBm");
      display.setCursor(0, 28);
      display.print("Last Count: ");
      display.println(lastPacketCount);
      display.display();

      linkTimeoutShown = true;
    }
  }
}
