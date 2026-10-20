#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

LiquidCrystal_I2C myLcd(0x27, 16, 2);

// HC-SR04 MESAFE SENSORU PINLERI
const byte TRIG_PIN = 13;
const byte ECHO_PIN = 12;
const byte ENGEL_MESAFESI = 15; // cm cinsinden guvenli durma mesafesi

// L298N MOTOR SURUCU PINLERI
const byte ENA = 10;
const byte IN1 = A0;
const byte IN2 = A1;
const byte IN3 = A2;
const byte IN4 = A3;
const byte ENB = 11;

const byte MOTOR_HIZI = 250;
const byte DONUS_HIZI = 180;

const unsigned int HAREKET_SURESI = 1000;
const unsigned int DONUS_SURESI = 575;
const unsigned int KARE_KENAR_SURESI = 2250;

// ------- KAYAN MENU -------------
const char menuYazisi[] = "2:Ileri  8:Geri  4:Sol  6:Sag  0:Bekle  5:Baslat  *:Komutlari Temizle  A:Sil  B:Tekrar  C:Drift  D:Kare   ";
unsigned long sonKaydirma = 0;
const unsigned long KAYDIRMA_SURESI = 750;
unsigned int kaydirmaIndex = 0;
bool anaEkranAktif = false;

// ------ KEYPAD -------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --------------- ROTA HAFIZASI ----------------
const byte MAX_KOMUT = 50;

struct RotaAdimi {
  char islem;
  unsigned int sure;
};

RotaAdimi komutlar[MAX_KOMUT];
byte komutSayisi = 0;

RotaAdimi sonKomutlar[MAX_KOMUT];
byte sonKomutSayisi = 0;

// Fonksiyon prototipleri
void arabaDur();
void anaEkran();
void kayanMenuGuncelle(bool hemenYaz);
void motorHizKontrol();
void sureliBeklemeEkle();
void komutKaydet(char komut, unsigned int sure);
void komutlariCalistir(bool tekrarMi);
const __FlashStringHelper* komutIsmiGetir(char k);
void kareCiz();
void dansEt();
void ekranaYaz(const __FlashStringHelper* satir1, const __FlashStringHelper* satir2);
void motorHareket(char yon);
void hareketEt(char yon, byte solHiz, byte sagHiz);
void motorSur(bool in1, bool in2, bool in3, bool in4, byte hizA, byte hizB);
int mesafeOlc();
bool bekleVeKontrolEt(unsigned int sureMs);

void setup() {
  Serial.begin(9600);

  // Sensor pinleri
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Motor surucu pinleri
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  myLcd.init();
  myLcd.backlight();

  arabaDur();
  anaEkran();
  delay(2000);
}

void loop() {
  kayanMenuGuncelle(false);

  char key = keypad.getKey();
  if (!key) return;

  if (key == '1' || key == '3' || key == '7' || key == '9' || key == '#') {
    return;
  }

  anaEkranAktif = false;

  Serial.print(F("Algilanan tus: "));
  Serial.println(key);

  switch (key) {

    case '*': // KOMUT HAFIZASINI TEMIZLE
      komutSayisi = 0;
      ekranaYaz(F("    ROBO-IZCI   "), F("Hafiza Silindi!"));
      delay(1000);
      anaEkran();
      break;

    case '5': // ROTAYI CALISTIR
      if (komutSayisi == 0) {
        ekranaYaz(F("    ROBO-IZCI   "), F("Hafiza Bos!"));
        delay(1000);
        anaEkran();
      } else {
        ekranaYaz(F("    ROBO-IZCI   "), F("Rota Calisiyor"));
        delay(800);
        komutlariCalistir(false);
      }
      break;

    case 'B': // SON ROTAYI TEKRARLA
      if (sonKomutSayisi == 0) {
        ekranaYaz(F("    ROBO-IZCI   "), F("Kayitli Rota Yok"));
        delay(1000);
        anaEkran();
      } else {
        ekranaYaz(F("    ROBO-IZCI   "), F("Tekrar Calisiyor"));
        delay(800);
        komutlariCalistir(true);
      }
      break;

    case 'A': // SON KOMUTU SIL
      if (komutSayisi > 0) {
        komutSayisi--;

        myLcd.clear();
        myLcd.setCursor(0, 0);
        myLcd.print(F("Son Adim Silindi"));

        myLcd.setCursor(0, 1);
        myLcd.print(F("Kalan Adim: "));
        myLcd.print(komutSayisi);

        delay(1000);
      } else {
        ekranaYaz(F("    ROBO-IZCI   "), F("Silinecek Yok!"));
        delay(800);
      }

      anaEkran();
      break;

    case 'C': // DRIFT / GOSTERI
      dansEt();
      break;

    case 'D': // KARE CIZ
      kareCiz();
      break;

    case '0': // BEKLEME KOMUTU
      sureliBeklemeEkle();
      break;

    case '2':
    case '4':
    case '6':
    case '8':
      komutKaydet(key, 0);
      break;

    default:
      // Tanimsiz tusa basilirsa kayan menu devam etsin
      anaEkranAktif = true;
      break;
  }
}
// ---------------- MESAFE OLCUMU ----------------
int mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long sure = pulseIn(ECHO_PIN, HIGH, 25000); // 25ms timeout (~4m limit)
  if (sure == 0) return 999;                  // Engel yoksa guvenli deger don
  return sure * 0.034 / 2;
}

// Ileri giderken sure boyunca mesafeyi denetler
bool bekleVeKontrolEt(unsigned int sureMs) {
  unsigned long baslangic = millis();
  while (millis() - baslangic < sureMs) {
    int mesafe = mesafeOlc();
    if (mesafe > 0 && mesafe <= ENGEL_MESAFESI) {
      arabaDur();
      ekranaYaz(F("!! ENGEL VAR !!" ), F("Motor Durdu"));
      delay(1500);
      return false; // Engel bulundu
    }
    delay(30);
  }
  return true; // Yol temiz tamamlandi
}

void anaEkran() {
  myLcd.clear();
  myLcd.setCursor(0, 0);
  myLcd.print(F("    ROBO-IZCI   "));
  kaydirmaIndex = 0;
  sonKaydirma = millis();
  anaEkranAktif = true;
  kayanMenuGuncelle(true);
}

void kayanMenuGuncelle(bool hemenYaz) {
  if (!anaEkranAktif) return;

  if (hemenYaz || millis() - sonKaydirma >= KAYDIRMA_SURESI) {
    sonKaydirma = millis();
    const unsigned int uzunluk = strlen(menuYazisi);

    myLcd.setCursor(0, 1);
    for (byte i = 0; i < 16; i++) {
      unsigned int index = (kaydirmaIndex + i) % uzunluk;
      myLcd.print(menuYazisi[index]);
    }

    kaydirmaIndex++;
    if (kaydirmaIndex >= uzunluk) {
      kaydirmaIndex = 0;
    }
  }
}

void motorHizKontrol() {
  int solHiz = 0;
  int sagHiz = 0;
  unsigned int calismaSuresi = 0;

  myLcd.clear();
  myLcd.print(F("Sol Motor Hizi"));
  myLcd.setCursor(0, 1);
  myLcd.print(F("0-255 + #"));

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k >= '0' && k <= '9') {
      solHiz = (solHiz * 10) + (k - '0');
      if (solHiz > 255) solHiz = 255;
      myLcd.setCursor(0, 1);
      myLcd.print(F("                "));
      myLcd.setCursor(0, 1);
      myLcd.print(F("Sol: "));
      myLcd.print(solHiz);
    } else if (k == '#') {
      break;
    } else if (k == '*') {
      arabaDur();
      ekranaYaz(F("Islem Iptal"), F(""));
      delay(800);
      anaEkran();
      return;
    }
  }

  delay(300);
  myLcd.clear();
  myLcd.print(F("Sag Motor Hizi"));
  myLcd.setCursor(0, 1);
  myLcd.print(F("0-255 + #"));

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k >= '0' && k <= '9') {
      sagHiz = (sagHiz * 10) + (k - '0');
      if (sagHiz > 255) sagHiz = 255;
      myLcd.setCursor(0, 1);
      myLcd.print(F("                "));
      myLcd.setCursor(0, 1);
      myLcd.print(F("Sag: "));
      myLcd.print(sagHiz);
    } else if (k == '#') {
      break;
    } else if (k == '*') {
      arabaDur();
      ekranaYaz(F("Islem Iptal"), F(""));
      delay(800);
      anaEkran();
      return;
    }
  }

  delay(300);
  myLcd.clear();
  myLcd.print(F("Kac Sn Calissin?"));
  myLcd.setCursor(0, 1);
  myLcd.print(F("Sure + #"));

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k >= '0' && k <= '9') {
      calismaSuresi = (calismaSuresi * 10) + (k - '0');
      myLcd.setCursor(0, 1);
      myLcd.print(F("                "));
      myLcd.setCursor(0, 1);
      myLcd.print(calismaSuresi);
      myLcd.print(F(" saniye"));
    } else if (k == '#') {
      if (calismaSuresi > 0) break;
      myLcd.setCursor(0, 1);
      myLcd.print(F("Sure Giriniz!   "));
    } else if (k == '*') {
      arabaDur();
      ekranaYaz(F("Islem Iptal"), F(""));
      delay(800);
      anaEkran();
      return;
    }
  }

  hareketEt('I', solHiz, sagHiz);

  for (int kalan = calismaSuresi; kalan > 0; kalan--) {
    myLcd.clear();
    myLcd.setCursor(0, 0);
    myLcd.print(F("L:"));
    myLcd.print(solHiz);
    myLcd.print(F(" R:"));
    myLcd.print(sagHiz);
    myLcd.setCursor(0, 1);
    myLcd.print(F("Kalan: "));
    myLcd.print(kalan);
    myLcd.print(F(" sn"));

    if (!bekleVeKontrolEt(1000)) {
      anaEkran();
      return;
    }
  }

  arabaDur();
  ekranaYaz(F("Motor Durdu"), F("Sure Tamamlandi"));
  delay(1200);
  anaEkran();
}

void sureliBeklemeEkle() {
  ekranaYaz(F("Kac Sn. Dursun?"), F("Bitince # Bas"));

  unsigned int girilenSure = 0;
  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k >= '0' && k <= '9') {
      girilenSure = (girilenSure * 10) + (k - '0');
      myLcd.setCursor(0, 1);
      myLcd.print(F("                "));
      myLcd.setCursor(0, 1);
      myLcd.print(girilenSure);
      myLcd.print(F(" saniye"));
    } else if (k == '#') {
      if (girilenSure > 0) {
        komutKaydet('0', girilenSure);
      } else {
        ekranaYaz(F("    ROBO-IZCI   "), F("Sure Girmediniz"));
        delay(1000);
        anaEkran();
      }
      return;
    } else if (k == '*') {
      ekranaYaz(F("    ROBO-IZCI   "), F("Islem Iptal!"));
      delay(1000);
      anaEkran();
      return;
    }
  }
}

void komutKaydet(char komut, unsigned int sure) {
  if (komutSayisi >= MAX_KOMUT) {
    ekranaYaz(F("    ROBO-IZCI   "), F("Hafiza Dolu!"));
    delay(1000);
    anaEkran();
    return;
  }

  komutlar[komutSayisi].islem = komut;
  komutlar[komutSayisi].sure = sure;
  komutSayisi++;

  myLcd.clear();
  myLcd.setCursor(0, 0);
  myLcd.print(F("Komut: "));

  if (komut == '0') {
    myLcd.print(sure);
    myLcd.print(F("sn Bekle"));
  } else {
    myLcd.print(komutIsmiGetir(komut));
  }

  myLcd.setCursor(0, 1);
  myLcd.print(F("Adim Sayisi: "));
  myLcd.print(komutSayisi);
}

void komutlariCalistir(bool tekrarMi) {
  byte calisacakSayi;
  RotaAdimi *calisacakDizi;

  if (tekrarMi) {
    calisacakSayi = sonKomutSayisi;
    calisacakDizi = sonKomutlar;
  } else {
    sonKomutSayisi = komutSayisi;
    for (byte i = 0; i < komutSayisi; i++) {
      sonKomutlar[i] = komutlar[i];
    }
    calisacakSayi = komutSayisi;
    calisacakDizi = komutlar;
  }

  for (byte i = 0; i < calisacakSayi; i++) {
    char aktifKomut = calisacakDizi[i].islem;
    unsigned int aktifSure = calisacakDizi[i].sure;

    myLcd.clear();
    myLcd.setCursor(0, 0);
    if (tekrarMi) {
      myLcd.print(F("Tekrar "));
    } else {
      myLcd.print(F("Adim "));
    }
    myLcd.print(i + 1);
    myLcd.print(F("/"));
    myLcd.print(calisacakSayi);

    myLcd.setCursor(0, 1);
    if (aktifKomut == '0') {
      myLcd.print(F("Bekle: "));
      myLcd.print(aktifSure);
      myLcd.print(F(" sn"));
    } else {
      myLcd.print(komutIsmiGetir(aktifKomut));
    }

    motorHareket(aktifKomut);

    if (aktifKomut == '2') { // ILERI GIDERKEN ENGEL KONTROLU
      if (!bekleVeKontrolEt(HAREKET_SURESI)) {
        if (!tekrarMi) komutSayisi = 0;
        anaEkran();
        return;
      }
    } else if (aktifKomut == '4' || aktifKomut == '6') {
      delay(DONUS_SURESI);
    } else if (aktifKomut == '0') {
      for (int kalan = aktifSure; kalan > 0; kalan--) {
        myLcd.setCursor(0, 1);
        myLcd.print(F("                "));
        myLcd.setCursor(0, 1);
        myLcd.print(F("Bekle: "));
        myLcd.print(kalan);
        myLcd.print(F(" sn"));
        delay(1000);
      }
    } else {
      delay(HAREKET_SURESI);
    }

    arabaDur();
    delay(500);
  }

  arabaDur();
  if (tekrarMi) {
    ekranaYaz(F("    ROBO-IZCI   "), F("Tekrar Bitti!"));
  } else {
    ekranaYaz(F("    ROBO-IZCI   "), F("Gorev Tamam!"));
  }

  delay(1200);
  if (!tekrarMi) {
    komutSayisi = 0;
  }
  anaEkran();
}

const __FlashStringHelper* komutIsmiGetir(char k) {
  switch (k) {
    case '2': return F("Ileri");
    case '8': return F("Geri");
    case '4': return F("Sola Don");
    case '6': return F("Saga Don");
    case '0': return F("Bekle");
    default:  return F("Bilinmiyor");
  }
}

void kareCiz() {
  ekranaYaz(F("Dikdortgen Gorev"), F("Basliyor..."));
  delay(1200);

  // 1) 2 ILERI
  myLcd.clear();
  myLcd.print(F("2 Ileri"));

  hareketEt('I', MOTOR_HIZI, MOTOR_HIZI);
  if (!bekleVeKontrolEt(HAREKET_SURESI * 2UL)) {
    anaEkran();
    return;
  }
  arabaDur();
  delay(500);

  // 2) 1 SAG
  myLcd.clear();
  myLcd.print(F("1 Sag"));

  hareketEt('D', DONUS_HIZI, DONUS_HIZI);
  delay(DONUS_SURESI);
  arabaDur();
  delay(500);

  // 3) 1 ILERI
  myLcd.clear();
  myLcd.print(F("1 Ileri"));

  hareketEt('I', MOTOR_HIZI, MOTOR_HIZI);
  if (!bekleVeKontrolEt(HAREKET_SURESI)) {
    anaEkran();
    return;
  }
  arabaDur();
  delay(500);

  // 4) 1 SAG
  myLcd.clear();
  myLcd.print(F("1 Sag"));

  hareketEt('D', DONUS_HIZI, DONUS_HIZI);
  delay(DONUS_SURESI);
  arabaDur();
  delay(500);

  // 5) 2 ILERI
  myLcd.clear();
  myLcd.print(F("2 Ileri"));

  hareketEt('I', MOTOR_HIZI, MOTOR_HIZI);
  if (!bekleVeKontrolEt(HAREKET_SURESI * 2UL)) {
    anaEkran();
    return;
  }
  arabaDur();
  delay(500);

  // 6) 1 SAG
  myLcd.clear();
  myLcd.print(F("1 Sag"));

  hareketEt('D', DONUS_HIZI, DONUS_HIZI);
  delay(DONUS_SURESI);
  arabaDur();
  delay(500);

  // 7) 1 ILERI
  myLcd.clear();
  myLcd.print(F("1 Ileri"));

  hareketEt('I', MOTOR_HIZI, MOTOR_HIZI);
  if (!bekleVeKontrolEt(HAREKET_SURESI)) {
    anaEkran();
    return;
  }
  arabaDur();
  delay(500);

  // 8) 1 SAG
  myLcd.clear();
  myLcd.print(F("1 Sag"));

  hareketEt('D', DONUS_HIZI, DONUS_HIZI);
  delay(DONUS_SURESI);
  arabaDur();

  ekranaYaz(F("    ROBO-IZCI   "), F("Gorev Tamam!"));
  delay(2000);

  anaEkran();
}

void dansEt() {
  ekranaYaz(F("!!! SHOW ZAMANI"), F("Kemerleri Bagla"));
  delay(1500);

  for (byte i = 0; i < 3; i++) {
    hareketEt('S', 200, 200);
    delay(200);
    hareketEt('D', 200, 200);
    delay(200);
  }

  arabaDur();
  delay(300);

  hareketEt('G', 255, 255);
  delay(600);
  arabaDur();
  delay(300);

  hareketEt('D', 255, 255);
  delay(DONUS_SURESI * 2UL);
  arabaDur();
  delay(300);

  hareketEt('I', 255, 255);
  if (!bekleVeKontrolEt(700)) {
    anaEkran();
    return;
  }

  arabaDur();
  delay(300);

  ekranaYaz(F("!!  DRIFT  !!"), F("Sifir Ciziyor"));
  hareketEt('D', 255, 255);
  delay(5500);

  arabaDur();
  ekranaYaz(F("   Vuhhuu!     "), F("Nasil ama? :)"));
  delay(2500);
  anaEkran();
}

void ekranaYaz(const __FlashStringHelper* satir1, const __FlashStringHelper* satir2) {
  myLcd.clear();
  myLcd.setCursor(0, 0);
  myLcd.print(satir1);
  myLcd.setCursor(0, 1);
  myLcd.print(satir2);
}

void motorHareket(char yon) {
  switch (yon) {
    case '2': hareketEt('I', MOTOR_HIZI, MOTOR_HIZI); break;
    case '8': hareketEt('G', MOTOR_HIZI, MOTOR_HIZI); break;
    case '4': hareketEt('S', DONUS_HIZI, DONUS_HIZI); break;
    case '6': hareketEt('D', DONUS_HIZI, DONUS_HIZI); break;
    case '0':
    default:  arabaDur(); break;
  }
}

void hareketEt(char yon, byte solHiz, byte sagHiz) {
  // Ileri giderken anlik on kontrol
  if (yon == 'I') {
    int mesafe = mesafeOlc();
    if (mesafe > 0 && mesafe <= ENGEL_MESAFESI) {
      arabaDur();
      return;
    }
  }

  switch (yon) {
    case 'I': motorSur(LOW, HIGH, LOW, HIGH, solHiz, sagHiz); break;
    case 'G': motorSur(HIGH, LOW, HIGH, LOW, solHiz, sagHiz); break;
    case 'S': motorSur(HIGH, LOW, LOW, HIGH, solHiz, sagHiz); break;
    case 'D': motorSur(LOW, HIGH, HIGH, LOW, solHiz, sagHiz); break;
    default:  motorSur(LOW, LOW, LOW, LOW, 0, 0);             break;
  }
}

void motorSur(bool in1, bool in2, bool in3, bool in4, byte hizA, byte hizB) {
  digitalWrite(IN1, in1);
  digitalWrite(IN2, in2);
  digitalWrite(IN3, in3);
  digitalWrite(IN4, in4);
  analogWrite(ENA, hizA);
  analogWrite(ENB, hizB);
}

void arabaDur() {
  motorSur(LOW, LOW, LOW, LOW, 0, 0);
}