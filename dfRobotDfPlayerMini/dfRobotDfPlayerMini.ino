#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// SoftwareSerial(RX, TX)
// DFPlayer TX  -> Arduino A1
// DFPlayer RX  <- Arduino A2 üzerinden 1K direnç
SoftwareSerial dfSerial(A1, A2);

DFRobotDFPlayerMini dfPlayer;

void hataYazdir(uint8_t type, int value);

void setup() {
  Serial.begin(9600);
  dfSerial.begin(9600);

  delay(2000);

  Serial.println();
  Serial.println("DFPlayer Mini test basliyor...");
  Serial.println("Baglanti bekleniyor...");

  // ACK açık, modülü resetle
  if (!dfPlayer.begin(dfSerial, true, true)) {
    Serial.println("DFPlayer ile iletisim kurulamadi!");
    Serial.println("Kontrol listesi:");
    Serial.println("DFPlayer TX -> Arduino A1");
    Serial.println("Arduino A2 -> 1K direnc -> DFPlayer RX");
    Serial.println("VCC -> 5V");
    Serial.println("GND -> GND");
    Serial.println("SD kart FAT32 ve takili olmali");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("DFPlayer baglantisi basarili.");

  dfPlayer.setTimeOut(1000);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  delay(1000);

  dfPlayer.volume(25);   // 0-30
  delay(500);

  Serial.println("SD karttaki MP3/0001.mp3 oynatiliyor...");

  dfPlayer.playMp3Folder(1);
}

void loop() {
  if (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    int value = dfPlayer.read();

    hataYazdir(type, value);
  }
}

void hataYazdir(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println("HATA: Zaman asimi");
      break;

    case WrongStack:
      Serial.println("HATA: Yanlis veri paketi");
      break;

    case DFPlayerCardInserted:
      Serial.println("SD kart takildi");
      break;

    case DFPlayerCardRemoved:
      Serial.println("SD kart cikartildi");
      break;

    case DFPlayerCardOnline:
      Serial.println("SD kart hazir");
      break;

    case DFPlayerPlayFinished:
      Serial.print("Muzik bitti. Dosya: ");
      Serial.println(value);
      break;

    case DFPlayerError:
      Serial.print("DFPlayer hata kodu: ");

      switch (value) {
        case Busy:
          Serial.println("SD kart bulunamadi");
          break;

        case Sleeping:
          Serial.println("Modul uyku modunda");
          break;

        case SerialWrongStack:
          Serial.println("Seri veri paketi hatali");
          break;

        case CheckSumNotMatch:
          Serial.println("Checksum hatasi");
          break;

        case FileIndexOut:
          Serial.println("Dosya numarasi gecersiz");
          break;

        case FileMismatch:
          Serial.println("Dosya bulunamadi");
          break;

        case Advertise:
          Serial.println("Reklam modu hatasi");
          break;

        default:
          Serial.println(value);
          break;
      }
      break;

    default:
      Serial.print("Durum kodu: ");
      Serial.print(type);
      Serial.print(" Deger: ");
      Serial.println(value);
      break;
  }
}