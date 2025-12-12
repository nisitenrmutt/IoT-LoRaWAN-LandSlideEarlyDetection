// ========= Sender Node: LoRa + Sensors =========
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include <esp_sleep.h>

// ---------- LoRa Pins ----------
#define ss   5    // LoRa NSS (CS)
#define rst  26   // LoRa RESET
#define dio0 17   // LoRa DIO0 (IRQ)

// ---------- DHT ----------
#define DHTPIN 16
#define DHTTYPE DHT22

Adafruit_MPU6050 mpu;
DHT dht(DHTPIN, DHTTYPE);

// ---------- Sensors (Analog) ----------
int raindropPin = 0;    // Rain sensor (ADC)
int analogPin1  = 25;   // Soil 20 cm
int analogPin2  = 32;   // Soil 40 cm
int analogPin3  = 12;   // Soil 60 cm
int visensorPin = 4;    // Vibration sensor

int Soil20cm = 0;
int Soil40cm = 0;
int Soil60cm = 0;
int rd = 0;
int Vi = 0;

// ---------- LoRa CONFIG (Must math MasterNode) ----------
long LORA_FREQ      = 920E6;   // 920 MHz 
int  LORA_SF        = 7;
long LORA_BW        = 125E3;
int  LORA_CR        = 5;
int  LORA_TX_POWER  = 20;      // dBm
long LORA_PREAMBLE  = 8;
byte LORA_SYNCWORD  = 0xF3;
bool LORA_USE_CRC   = true;

// ====== Scheduler Deep Sleep ======
const float    SEND_INTERVAL_MINUTES = 0.1;        // 0.1 = 6 sec
const uint32_t SEND_INTERVAL_SECONDS = SEND_INTERVAL_MINUTES * 60;
const bool     USE_DEEP_SLEEP        = true;

// ========= Packet counter on RTC memory =========
RTC_DATA_ATTR int counter = 0;  

// Node Name
const char *deviceName = "Node2"; //For example Node2

// 
void printLoRaConfig() {
  Serial.println("===== LoRa Current Config (Sender) =====");
  Serial.print("Freq     : "); Serial.print(LORA_FREQ / 1E6); Serial.println(" MHz");
  Serial.print("SF       : SF"); Serial.println(LORA_SF);
  Serial.print("BW       : "); Serial.print(LORA_BW / 1000); Serial.println(" kHz");
  Serial.print("CR       : 4/"); Serial.println(LORA_CR);
  Serial.print("TX Power : "); Serial.print(LORA_TX_POWER); Serial.println(" dBm");
  Serial.print("Preamble : "); Serial.println(LORA_PREAMBLE);
  Serial.print("Syncword : 0x"); Serial.println(LORA_SYNCWORD, HEX);
  Serial.print("CRC      : "); Serial.println(LORA_USE_CRC ? "ON" : "OFF");
  Serial.print("Counter  : "); Serial.println(counter);
  Serial.println("========================================");
}

void setupLoRa() {
  LoRa.setPins(ss, rst, dio0);
  Serial.println("Initializing LoRa (Sender)...");
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed!");
    while (1) delay(10);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setPreambleLength(LORA_PREAMBLE);
  LoRa.setSyncWord(LORA_SYNCWORD);
  if (LORA_USE_CRC) LoRa.enableCrc(); else LoRa.disableCrc();
  LoRa.setTxPower(LORA_TX_POWER);
  Serial.println("LoRa Initializing OK (Sender)!");
  printLoRaConfig();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  dht.begin();

  Wire.begin();
  if (!mpu.begin(0x68)) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  pinMode(raindropPin, INPUT);
  pinMode(visensorPin, INPUT);
  pinMode(analogPin1, INPUT);
  pinMode(analogPin2, INPUT);
  pinMode(analogPin3, INPUT);

  setupLoRa();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // rotation angle (degree)
  float rotationXDegrees = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  float rotationYDegrees = atan2(a.acceleration.x, a.acceleration.z) * 180.0 / PI;
  float rotationZDegrees = atan2(a.acceleration.x, a.acceleration.y) * 180.0 / PI;

  int h = dht.readHumidity();
  int t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    h = -999;
    t = -999;
  }

  // Sensors read
  int raindropValue   = analogRead(raindropPin);
  rd = map(raindropValue, 0, 4095, 100, 0);

  int vibrationValue  = analogRead(visensorPin);
  Vi = map(vibrationValue, 0, 4095, 0, 100);

  int soilval1 = analogRead(analogPin1);
  Soil20cm = map(soilval1, 0, 4095, 100, 0);

  int soilval2 = analogRead(analogPin2);
  Soil40cm = map(soilval2, 0, 4095, 100, 0);

  int soilval3 = analogRead(analogPin3);
  Soil60cm = map(soilval3, 0, 4095, 100, 0);

  // Debug serial (ย่อ)
  Serial.println("==== Sender ====");
  Serial.print("Count: "); Serial.println(counter);
  Serial.print("RotX: "); Serial.println(rotationXDegrees);
  Serial.print("RotY: "); Serial.println(rotationYDegrees);
  Serial.print("RotZ: "); Serial.println(rotationZDegrees);
  Serial.print("Ax: "); Serial.println(g.gyro.x);
  Serial.print("Ay: "); Serial.println(g.gyro.y);
  Serial.print("Az: "); Serial.println(g.gyro.z);
  Serial.print("Soil20: "); Serial.println(Soil20cm);
  Serial.print("Soil40: "); Serial.println(Soil40cm);
  Serial.print("Soil60: "); Serial.println(Soil60cm);
  Serial.print("Rain: "); Serial.println(rd);
  Serial.print("Vib: "); Serial.println(Vi);
  Serial.print("Hum: "); Serial.println(h);
  Serial.print("Temp: "); Serial.println(t);

  // 
  Serial.println("Sending packet...");
  LoRa.beginPacket();

  // mapping key:
  // dn = deviceName, c = count
  // rx,ry,rz = rotation, ax,ay,az = angular rate
  // s20,s40,s60 = soil, rd = raindrop, vi = vibration, h = hum, t = temp
  LoRa.print("{\"dn\":\"");
  LoRa.print(deviceName);          // string
  LoRa.print("\",\"c\":");
  LoRa.print(counter);             // int (RTC counter)
  LoRa.print(",\"rx\":");
  LoRa.print(rotationXDegrees, 1); // float 1 dec
  LoRa.print(",\"ry\":");
  LoRa.print(rotationYDegrees, 1);
  LoRa.print(",\"rz\":");
  LoRa.print(rotationZDegrees, 1);
  LoRa.print(",\"ax\":");
  LoRa.print(g.gyro.x, 2);
  LoRa.print(",\"ay\":");
  LoRa.print(g.gyro.y, 2);
  LoRa.print(",\"az\":");
  LoRa.print(g.gyro.z, 2);
  LoRa.print(",\"s20\":");
  LoRa.print(Soil20cm);
  LoRa.print(",\"s40\":");
  LoRa.print(Soil40cm);
  LoRa.print(",\"s60\":");
  LoRa.print(Soil60cm);
  LoRa.print(",\"rd\":");
  LoRa.print(rd);
  LoRa.print(",\"vi\":");
  LoRa.print(Vi);
  LoRa.print(",\"h\":");
  LoRa.print(h);
  LoRa.print(",\"t\":");
  LoRa.print(t);
  LoRa.print("}");

  LoRa.endPacket();

 
  counter++;

  // Deep Sleep scheduler
  if (USE_DEEP_SLEEP) {
    Serial.print("Deep sleep ");
    Serial.print(SEND_INTERVAL_SECONDS);
    Serial.println(" s");
    esp_sleep_enable_timer_wakeup((uint64_t)SEND_INTERVAL_SECONDS * 1000000ULL);
    esp_deep_sleep_start();
  } else {
    delay(SEND_INTERVAL_SECONDS * 1000UL);
  }
}
