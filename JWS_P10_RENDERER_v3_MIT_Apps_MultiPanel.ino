#include <DMDESP.h>
#include <fonts/SystemFont5x7Ramping.h>
#include <fonts/Arial_Black_16.h>

int pnlX = 1;
int pnlY = 1;
DMDESP Disp(pnlX, pnlY);

int jam, menit, detik, tgl, bln, thn;
String jadwal[6] = { "00:00", "00:00", "00:00", "00:00", "00:00", "00:00" };
String namaSholat[6] = { "IMSAK", "SUBUH", "DZUHUR", "ASHAR", "MAGRIB", "ISYA" };

// --- MODIFIKASI VARIABEL PESAN ---
String pesanHP = "JWS SMK ELECTRONICS"; // Simpan input HP
String pesanAuto = "";                 // Simpan Motivasi/Kalender
String *activeMsg = &pesanHP;          // Pointer untuk memilih pesan yang tampil

char frame[160];
byte idx = 0;
bool mulai = false;
unsigned long lastScroll, lastMode, lastRX = 0;
int scrollX;
byte mode = 0; 

byte checksum(String s) {
  byte cs = 0;
  for (int i = 0; i < s.length(); i++) cs ^= s[i];
  return cs;
}

void parseData(char* data) {
  if (strncmp(data, "CONFIG,LAYOUT", 13) == 0) {
    char* p = data;
    strtok_r(p, ",", &p); strtok_r(p, ",", &p);
    pnlX = atoi(strtok_r(p, ",", &p));
    pnlY = atoi(strtok_r(p, ",", &p));
    Disp.start();
    Disp.setBrightness(40);
    return;
  }

  // UPDATE PESAN DARI HP (Tag MSG)
  if (strncmp(data, "CONFIG,MSG", 10) == 0) {
    char* p = data;
    strtok_r(p, ",", &p); strtok_r(p, ",", &p);
    pesanHP = String(p);
    pesanHP.toUpperCase();
    activeMsg = &pesanHP; // Set ke pesan HP
    mode = 1; 
    scrollX = pnlX * 32;
    lastMode = millis();
    return;
  }

  // UPDATE PESAN OTOMATIS/MOTIVASI (Tag TYPE)
  if (strncmp(data, "CONFIG,TYPE", 11) == 0) {
    char* p = data;
    strtok_r(p, ",", &p); strtok_r(p, ",", &p);
    pesanAuto = String(p);
    pesanAuto.toUpperCase();
    activeMsg = &pesanAuto; // Set ke pesan Otomatis
    mode = 1; 
    scrollX = pnlX * 32;
    lastMode = millis();
    return;
  }

  char* star = strchr(data, '*');
  if (!star) return;
  *star = '\0';
  byte csDiterima = atoi(star + 1);
  if (checksum(String(data)) != csDiterima) return;

  lastRX = millis();
  char* p = data;
  char* str;
  byte i = 0;
  while ((str = strtok_r(p, ",", &p)) != NULL) {
    if (i == 0) jam = atoi(str);
    else if (i == 1) menit = atoi(str);
    else if (i == 2) detik = atoi(str);
    else if (i == 3) tgl = atoi(str);
    else if (i == 4) bln = atoi(str);
    else if (i == 5) thn = atoi(str);
    else if (i >= 6 && i <= 11) {
      jadwal[i - 6] = String(str);
      jadwal[i - 6].trim();
    }
    i++;
  }
}

void bacaSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '<') { mulai = true; idx = 0; }
    else if (c == '>') { frame[idx] = '\0'; parseData(frame); mulai = false; }
    else if (mulai && idx < 159) { frame[idx++] = c; }
    yield();
  }
}

void setup() {
  Serial.begin(9600);
  Disp.start();
  Disp.setBrightness(40);
  scrollX = pnlX * 32;
}

void loop() {
  bacaSerial();
  Disp.loop();

  if (millis() - lastRX > 10000) {
    Disp.clear();
    Disp.setFont(SystemFont5x7Ramping);
    Disp.drawText((pnlX * 32 - Disp.textWidth("MENCARI")) / 2, 0, "MENCARI");
    Disp.drawText((pnlX * 32 - Disp.textWidth("MASTER")) / 2, 8, "MASTER");
  } else {
    if (mode == 0) {
      Disp.clear();
      Disp.setFont(SystemFont5x7Ramping);
      char s[15];
      sprintf(s, "%02d:%02d:%02d", jam, menit, detik);
      Disp.drawText((pnlX * 32 - Disp.textWidth(s)) / 2, 0, s);

      static byte sholatIdx = 0;
      static unsigned long lastChange = 0;
      if (millis() - lastChange > 3000) { sholatIdx++; if(sholatIdx > 5) sholatIdx = 0; lastChange = millis(); }
      
      String info = namaSholat[sholatIdx] + ":" + jadwal[sholatIdx];
      Disp.drawText((pnlX * 32 - Disp.textWidth(info)) / 2, 9, info);

      if (millis() - lastMode > 20000) { 
        activeMsg = &pesanHP; // Default kembali ke pesan HP setelah durasi Jam habis
        mode = 1; 
        lastMode = millis(); 
        scrollX = pnlX * 32; 
      }
    }
    else if (mode == 1) {
      Disp.setFont(Arial_Black_16);
      if (millis() - lastScroll > 30) {
        Disp.clear();
        Disp.drawText(scrollX, 0, *activeMsg); // Tampilkan pesan yang sedang aktif
        scrollX--;
        if (scrollX < -(Disp.textWidth(*activeMsg))) { mode = 0; lastMode = millis(); }
        lastScroll = millis();
      }
    }
  }
}