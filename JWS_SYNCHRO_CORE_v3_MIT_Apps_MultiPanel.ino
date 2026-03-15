/*
 * =======================================================================================
 * PROJECT: JWS SYNCHRO CORE - MASTER SIDE
 * VERSION: v1.5 (Full Features - Verified)
 * AUTHOR: Bapak Guru - SMK Electronics Engineering
 * =======================================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <RTClib.h>
#include <PrayerTimes.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>

struct ConfigLoc {
  double lat; double lon; uint32_t valid;
};
ConfigLoc userLoc;
#define EEPROM_ADDR 0
#define EEPROM_VALID_KEY 12345

#define PIXEL_PER_SEGMENT 2
#define PIXEL_DIGITS 4
#define PIXEL_PIN D6
#define PIXEL_DASH 1
#define SDA_PIN D2
#define SCL_PIN D1
#define DEFAULT_PANEL_X 1 

SoftwareSerial mp3Serial(D7, D5);
DFRobotDFPlayerMini myDFPlayer;
Adafruit_NeoPixel strip = Adafruit_NeoPixel((PIXEL_PER_SEGMENT * 7 * PIXEL_DIGITS) + (PIXEL_DASH * 2), PIXEL_PIN, NEO_GRB + NEO_KHZ800);

ESP8266WebServer server(80);
RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", 25200, 60000);

const char *ssid_ntp = "SSID-mu";  //sesuaikan dengan SSID internetmu
const char *pass_ntp = "PASW-mu";  //sesuaikan dengan pasword SSIDmu

int Hour = -1, Minute = -1, Second = -1;
double prayerTimes[7];
byte wheelPos = 0;
byte digits[] = { 0b1111110, 0b0011000, 0b0110111, 0b0111101, 0b1011001, 0b1101101, 0b1101111, 0b0111000, 0b1111111, 0b1111101 };

const char* daftarPesan[] = {
  "SMART PEOPLE NEVER FEEL STRONGEST",
  "I CAN AND I WILL",
  "TEKNIK ELEKTRONIKA SMK - DISIPLIN & KREATIF",
  "BELAJARLAH DENGAN GIAT UNTUK MASA DEPAN",
  "KESEHATAN ADALAH KEKAYAAN TERBESAR",
  "KERJAKAN DENGAN HATI, HASIL AKAN MENGIKUTI"
};

void update_Prayers() {
  DateTime now = rtc.now();
  set_calc_method(Karachi);
  set_asr_method(Shafii);
  set_fajr_angle(20.0);
  set_isha_angle(18.0);
  get_prayer_times(now.year(), now.month(), now.day(), userLoc.lat, userLoc.lon, 7, prayerTimes);
}

void sendDataToSlave(int h, int m, int s) {
  DateTime now = rtc.now();
  int sh, sm, dh, dm, ah, am, mh, mm, ih, im;
  get_float_time_parts(prayerTimes[0], sh, sm);
  get_float_time_parts(prayerTimes[2], dh, dm);
  get_float_time_parts(prayerTimes[3], ah, am);
  get_float_time_parts(prayerTimes[5], mh, mm);
  get_float_time_parts(prayerTimes[6], ih, im);

  sm += 3; dm += 4; am += 3; mm += 6; im += 2; 
  if (sm >= 60) { sh++; sm -= 60; }
  if (dm >= 60) { dh++; dm -= 60; }
  if (am >= 60) { ah++; am -= 60; }
  if (mm >= 60) { mh++; mm -= 60; }
  if (im >= 60) { ih++; im -= 60; }

  int imsakH = sh; int imsakM = sm - 10;
  if (imsakM < 0) { imsakH--; imsakM += 60; }

  char content[150];
  sprintf(content, "%d,%d,%d,%d,%d,%d,%02d:%02d,%02d:%02d,%02d:%02d,%02d:%02d,%02d:%02d,%02d:%02d",
          h, m, s, now.day(), now.month(), now.year(),
          imsakH, imsakM, sh, sm, dh, dm, ah, am, mh, mm, ih, im);

  byte cs = 0;
  for (int i = 0; content[i] != '\0'; i++) { cs ^= content[i]; }
  Serial.print("<"); Serial.print(content); Serial.print("*"); Serial.print(cs); Serial.println(">");
}

void handleUpdateLocation() {
  if (server.hasArg("Lat") && server.hasArg("Lon")) {
    userLoc.lat = server.arg("Lat").toDouble();
    userLoc.lon = server.arg("Lon").toDouble();
    userLoc.valid = EEPROM_VALID_KEY;
    EEPROM.put(EEPROM_ADDR, userLoc); EEPROM.commit();
    update_Prayers();
    server.send(200, "text/plain", "OK");
  }
}

void handleUpdateLayout() {
  if (server.hasArg("x") && server.hasArg("y")) {
    Serial.printf("<CONFIG,LAYOUT,%s,%s>\n", server.arg("x").c_str(), server.arg("y").c_str());
    server.send(200, "text/plain", "OK");
  }
}

void handleUpdatePesan() {
  if (server.hasArg("msg")) {
    Serial.print("<CONFIG,TYPE,");
    Serial.print(server.arg("msg"));
    Serial.println(">");
    server.send(200, "text/plain", "OK");
  }
}

uint32_t colorWheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if (WheelPos < 170) { WheelPos -= 85; return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3); }
  WheelPos -= 170; return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void writeDigitRainbow(int index, int val) {
  int margin = (index >= 2) ? (PIXEL_DASH * 2) : 0;
  int offset = index * (PIXEL_PER_SEGMENT * 7) + margin;
  byte digit = digits[val];
  for (int i = 0; i < 7; i++) {
    uint32_t segCol = colorWheel((wheelPos + (i * 15) + (index * 40)) & 255);
    if (digit & (0x40 >> i)) {
      for (int j = 0; j < PIXEL_PER_SEGMENT; j++) strip.setPixelColor(offset + (i * PIXEL_PER_SEGMENT) + j, segCol);
    }
  }
}

void checkTarhimAndSalam() {
  int waktuSholatIdx[] = { 0, 2, 3, 5, 6 };
  const char* nm[] = {"SUBUH", "DZUHUR", "ASHAR", "MAGHRIB", "ISYA"};
  for (int i = 0; i < 5; i++) {
    int sh, sm; get_float_time_parts(prayerTimes[waktuSholatIdx[i]], sh, sm);
    if (waktuSholatIdx[i] == 0) sm += 3; else if (waktuSholatIdx[i] == 2) sm += 4;
    else if (waktuSholatIdx[i] == 3) sm += 3; else if (waktuSholatIdx[i] == 5) sm += 6;
    else if (waktuSholatIdx[i] == 6) sm += 2;
    if (sm >= 60) { sh++; sm -= 60; }

    if (Hour == sh && Minute == sm && Second == 0) {
      Serial.printf("<CONFIG,MSG,ASSALAMU'ALAIKUM - WAKTU %s TELAH TIBA>\n", nm[i]);
    }

    int tarH = sh; int tarM = sm - 10;
    if (tarM < 0) { tarH--; tarM += 60; }
    if (Hour == tarH && Minute == tarM && Second == 0) myDFPlayer.play(1);
  }
}

// --- TAMBAHAN PADA SETUP (PROSES WIFI) ---
void wifiLoadingAnimation() {
  static byte pos = 0;
  strip.clear();
  // Animasi satu titik Cyan berputar di seluruh segmen
  strip.setPixelColor(pos, strip.Color(0, 255, 255)); 
  strip.show();
  pos++;
  if (pos >= strip.numPixels()) pos = 0;
  delay(50);
}

void setup() {
  Serial.begin(9600); mp3Serial.begin(9600);
  EEPROM.begin(512); EEPROM.get(EEPROM_ADDR, userLoc);
  if (userLoc.valid != EEPROM_VALID_KEY) {
    userLoc.lat = -**********; userLoc.lon = **********; userLoc.valid = EEPROM_VALID_KEY; //ubah tanda **, sesuaikan lat, long wilayahmu
  }

  strip.begin(); strip.setBrightness(120); strip.show();

  // PROSES KONEKSI WIFI DENGAN ANIMASI CYAN
  WiFi.begin(ssid_ntp, pass_ntp);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 40) { // Maks 20 detik
    wifiLoadingAnimation();
    attempt++;
  }

  WiFi.softAP("JWS-SMK-ENGINEERING", "12345678");
  server.on("/set", handleUpdateLocation);
  server.on("/layout", handleUpdateLayout);
  server.on("/pesan", handleUpdatePesan);
  server.begin();

  if (myDFPlayer.begin(mp3Serial, true, false)) { myDFPlayer.volume(25); }
  Wire.begin(SDA_PIN, SCL_PIN);
  if (rtc.begin()) { if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }
  
  update_Prayers();
  delay(1000);
  Serial.printf("<CONFIG,LAYOUT,%d,1>\n", DEFAULT_PANEL_X);
}

void loop() {
  server.handleClient();
  DateTime now = rtc.now();
  if (now.year() < 2025) return;

  if (Second != now.second()) {
    Second = now.second(); Minute = now.minute(); Hour = now.hour();
    sendDataToSlave(Hour, Minute, Second);
    checkTarhimAndSalam();

    if (Minute % 15 == 0 && Second == 0) {
      int idx = (Hour + Minute) % 6;
      Serial.print("<CONFIG,TYPE,"); 
      Serial.print(daftarPesan[idx]);
      Serial.println(">");
    }

    if (Hour == 0 && Minute == 0 && Second == 0) update_Prayers();
    if (Hour == 2 && Minute == 0 && Second == 0) {
       char kalender[30];
       sprintf(kalender, "TGL: %02d/%02d/%d", now.day(), now.month(), now.year());
       Serial.printf("<CONFIG,TYPE,%s>\n", kalender);
    }
  }

  static unsigned long lastUpdatePixel = 0;
  if (millis() - lastUpdatePixel > 20) {
    wheelPos++; strip.clear();
    writeDigitRainbow(0, Hour / 10); writeDigitRainbow(1, Hour % 10);
    writeDigitRainbow(2, Minute / 10); writeDigitRainbow(3, Minute % 10);
    if (Second % 2 == 0) {
      int base = 2 * (7 * PIXEL_PER_SEGMENT);
      for (int i = 0; i < PIXEL_DASH * 2; i++) strip.setPixelColor(base + i, colorWheel(wheelPos + 128));
    }
    strip.show(); lastUpdatePixel = millis();
  }
}
