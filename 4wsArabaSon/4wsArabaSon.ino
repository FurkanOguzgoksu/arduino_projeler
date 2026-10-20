#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>

LiquidCrystal_I2C myLcd(0x27, 16, 2);

// L298N MOTOR SÜRÜCÜ PİNLERİ
const int ENA = 11;
const int IN1 = 10;
const int IN2 = 9;

const int IN3 = 8;
const int IN4 = 7;
const int ENB = 6;

// HC-SR04 PİNLERİ
const int TRIG_PIN = 4;
const int ECHO_PIN = 5;

// Araç bu mesafeden daha yakında engel görürse durur
const int ENGEL_MESAFESI = 20;

// Sensörün kaç milisaniyede bir kontrol edileceği
const unsigned long SENSOR_KONTROL_SURESI = 100;

unsigned long sonSensorKontrolZamani = 0;
int sonMesafe = 999;

// DFPLAYER MINI
// Arduino pin 2: RX  -> DFPlayer TX
// Arduino pin 3: TX  -> DFPlayer RX
SoftwareSerial dfPlayerSerial(2, 3);
DFRobotDFPlayerMini myDFPlayer;

bool dfPlayerHazir = false;
bool muzikOynuyor = false;

int motorHizi = 250;
int donusHizi = 170;
int yavasMotorHizi = 50;

// Son hareket komutu
char aktifHareket = 'S';

// Bluetooth'tan gelen komut
char gelenKomut;

// BAĞLANTI GÜVENLİĞİ
// Uygulama, araç hareket ederken her 300 ms'de bir '.' gönderir.
// Bu süre boyunca hiç veri gelmezse bağlantı kopmuş sayılır ve araç durdurulur
// (telefon uzaklaştı, uygulama kapandı, Bluetooth koptu...).
const unsigned long BAGLANTI_ZAMAN_ASIMI = 1000;
unsigned long sonVeriZamani = 0;

// FAR
// Farın bağlı olduğu pin projeden projeye değişebildiği için uygulamadan ayarlanır:
// uygulama bağlanınca "#12" veya "#A0" gibi bir satır gönderir. Pin EEPROM'a kaydedilir.
const int EEPROM_FAR_IMZA_ADRESI = 0;
const int EEPROM_FAR_PIN_ADRESI = 1;
const byte EEPROM_FAR_IMZASI = 0xA5; // EEPROM'daki pin bilgisinin bu koda ait olduğunu gösterir

int farPini = -1; // -1: henüz ayarlanmadı
bool farAcik = false;

// Uygulamadan gelen ayar satırı: '#' ile başlar, satır sonuyla biter
const byte AYAR_MAKS_UZUNLUK = 3; // en uzun pin yazısı: "A15"
char ayarTamponu[AYAR_MAKS_UZUNLUK + 1];
byte ayarUzunlugu = 0;
bool ayarOkunuyor = false;

void setup() {
  Serial.begin(9600);

  // DFPlayer seri iletişimi
  dfPlayerSerial.begin(9600);

  // Motor pinleri
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // HC-SR04 pinleri
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

  arabaDur();
  farPininiYukle();

  myLcd.init();
  myLcd.backlight();

  myLcd.setCursor(0, 0);
  myLcd.print("KIRLANGIC FATIH");

  myLcd.setCursor(0, 1);
  myLcd.print("Sistem Aciliyor");

  delay(1000);

  dfPlayerBaslat();

  delay(500);

  ekranaYaz("Hazir");
}


void loop() {
  bluetoothKomutKontrol();
  baglantiKontrol();
  engelKontrol();
}


void bluetoothKomutKontrol() {
  if (Serial.available() > 0) {
    gelenKomut = Serial.read();
    sonVeriZamani = millis();

    // Uygulamadan gelen ayar satırının devamı
    if (ayarOkunuyor && ayarKarakteriniIsle(gelenKomut)) {
      return;
    }

    // Satır sonu karakterlerini dikkate alma
    if (gelenKomut == '\n' || gelenKomut == '\r') {
      return;
    }

    // Ayar satırı başlangıcı (ör. "#12": far pini)
    if (gelenKomut == '#') {
      ayarOkunuyor = true;
      ayarUzunlugu = 0;
      return;
    }

    komutuCalistir(gelenKomut);
  }
}

// Ayar satırının bir karakterini işler. Satır sonu gelmeden ayara ait olmayan bir karakter
// gelirse ayar okumayı bırakır ve false döner; o karakter normal komut olarak işlenir.
bool ayarKarakteriniIsle(char karakter) {
  if (karakter == '\n' || karakter == '\r') {
    ayarTamponu[ayarUzunlugu] = '\0';
    ayarOkunuyor = false;
    farPininiAyarla(ayarTamponu);
    return true;
  }

  if (isDigit(karakter) || karakter == 'A' || karakter == 'a') {
    if (ayarUzunlugu == AYAR_MAKS_UZUNLUK) {
      // Bu kadar uzun bir pin olamaz; satırın kalanı yok sayılır (rakamlar komut değildir)
      ayarOkunuyor = false;
      ekranaYaz("Gecersiz Pin");
      Serial.println("Gecersiz far pini");
      return true;
    }

    ayarTamponu[ayarUzunlugu++] = karakter;
    return true;
  }

  ayarOkunuyor = false;
  return false;
}

void baglantiKontrol() {
  // Araç zaten duruyorsa kontrol etmeye gerek yok
  if (aktifHareket == 'S') {
    return;
  }

  if (millis() - sonVeriZamani > BAGLANTI_ZAMAN_ASIMI) {
    arabaDur();
    aktifHareket = 'S';
    ekranaYaz("Baglanti Koptu");
  }
}

void komutuCalistir(char komut) {
  switch (komut) {

    case 'F':
      ileriKomutunuUygula();
      break;

    case 'B':
      geri();
      aktifHareket = 'B';
      ekranaYaz("Geri");
      break;

    case 'L':
      solaDon();
      aktifHareket = 'L';
      ekranaYaz("Sola Donuyor");
      break;

    case 'R':
      sagaDon();
      aktifHareket = 'R';
      ekranaYaz("Saga Donuyor");
      break;

    case 'G':
      ileriSolKomutunuUygula();
      break;

    case 'I':
      ileriSagKomutunuUygula();
      break;

    case 'H':
      geriSol();
      aktifHareket = 'H';
      ekranaYaz("Geri Sol");
      break;

    case 'J':
      geriSag();
      aktifHareket = 'J';
      ekranaYaz("Geri Sag");
      break;

    case 'S':
      arabaDur();
      aktifHareket = 'S';
      ekranaYaz("Durdu");
      break;

    case 'W':
      farAc();
      break;

    case 'w':
      farKapat();
      break;

    case 'P':
      muzigiOynat();
      break;

    case 'O':
      muzigiDuraklat();
      break;

    case 'C':
      muzigeDevamEt();
      break;

    case 'N':
      sonrakiSarki();
      break;

    case 'V':
      oncekiSarki();
      break;

    default:
      // '.' (uygulamanın bağlantı sinyali) gibi diğer karakterler yok sayılır
      break;
  }
}

// --------------------İLERİ KOMUTLARINDA ENGEL KONTROLÜ------------------
void ileriKomutunuUygula() {
  sonMesafe = mesafeOlc();

  if (engelVarMi()) {
    engelNedeniyleDur();
    return;
  }

  ileri();
  aktifHareket = 'F';
  ekranaYaz("Ileri");
}

void ileriSolKomutunuUygula() {
  sonMesafe = mesafeOlc();

  if (engelVarMi()) {
    engelNedeniyleDur();
    return;
  }

  ileriSol();
  aktifHareket = 'G';
  ekranaYaz("Ileri Sol");
}

void ileriSagKomutunuUygula() {
  sonMesafe = mesafeOlc();

  if (engelVarMi()) {
    engelNedeniyleDur();
    return;
  }

  ileriSag();
  aktifHareket = 'I';
  ekranaYaz("Ileri Sag");
}

// ------------------SÜREKLİ ENGEL KONTROLÜ----------------------------------
void engelKontrol() {
  if (millis() - sonSensorKontrolZamani < SENSOR_KONTROL_SURESI) {
    return;
  }

  sonSensorKontrolZamani = millis();

  // Yalnızca araç ileri yönde hareket ediyorsa kontrol et
  if (!ileriYondeHareketEdiyor()) {
    return;
  }

  sonMesafe = mesafeOlc();

  if (engelVarMi()) {
    engelNedeniyleDur();
  }
}

bool ileriYondeHareketEdiyor() {
  // İleri yöndeki hareketler: F (ileri), G (ileri sol), I (ileri sağ)
  return aktifHareket == 'F' ||
         aktifHareket == 'G' ||
         aktifHareket == 'I';
}

bool engelVarMi() {
  return sonMesafe > 0 && sonMesafe <= ENGEL_MESAFESI;
}

void engelNedeniyleDur() {

  arabaDur();
  aktifHareket = 'S';

  myLcd.setCursor(0, 0);
  myLcd.print("KIRLANGIC FATIH");

  myLcd.setCursor(0, 1);
  myLcd.print("                ");

  myLcd.setCursor(0, 1);
  myLcd.print("Engel: ");
  myLcd.print(sonMesafe);
  myLcd.print(" cm");

}

// -----------------HC-SR04 MESAFE ÖLÇÜMÜ-----------------------------------
int mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  // Maksimum yaklaşık 4 metre için zaman aşımı
  unsigned long sure = pulseIn(ECHO_PIN, HIGH, 25000UL);

  // Ölçüm alınamazsa uzak kabul et
  if (sure == 0) {
    return 999;
  }

  int mesafe = sure * 0.0343 / 2;
  return mesafe;
}

// ---------------DÜZ HAREKETLER----------------------------------
void ileri() {
  solMotorGeri(motorHizi);
  sagMotorGeri(motorHizi);
}

void geri() {
  solMotorIleri(motorHizi);
  sagMotorIleri(motorHizi);
}

void sagaDon() {
  solMotorGeri(donusHizi);
  sagMotorIleri(donusHizi);
}

void solaDon() {
  solMotorIleri(donusHizi);
  sagMotorGeri(donusHizi);
}

// -----------------ÇAPRAZ HAREKETLER-----------------------------
void ileriSol() {
  // İleri giderken sola yönel
  // Sol motor yavaş, sağ motor hızlı
  solMotorGeri(yavasMotorHizi);
  sagMotorGeri(motorHizi);
}

void ileriSag() {
  // İleri giderken sağa yönel
  // Sol motor hızlı, sağ motor yavaş
  solMotorGeri(motorHizi);
  sagMotorGeri(yavasMotorHizi);
}

void geriSol() {
  // Geri giderken sola yönel
  // Sol motor yavaş, sağ motor hızlı
  solMotorIleri(yavasMotorHizi);
  sagMotorIleri(motorHizi);
}

void geriSag() {
  // Geri giderken sağa yönel
  // Sol motor hızlı, sağ motor yavaş
  solMotorIleri(motorHizi);
  sagMotorIleri(yavasMotorHizi);
}

void arabaDur() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void solMotorIleri(int hiz) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  analogWrite(ENA, hiz);
}

void solMotorGeri(int hiz) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  analogWrite(ENA, hiz);
}

void sagMotorIleri(int hiz) {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENB, hiz);
}

void sagMotorGeri(int hiz) {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENB, hiz);
}

// ---------------------DFPLAYER FONKSİYONLARI-------------------------------

void dfPlayerBaslat() {
  if (myDFPlayer.begin(dfPlayerSerial)) {
    dfPlayerHazir = true;

    // Ses seviyesi: 0-30
    myDFPlayer.volume(30);

    myLcd.setCursor(0, 1);
    myLcd.print("Ses Sistemi OK ");
  } else {
    dfPlayerHazir = false;

    myLcd.setCursor(0, 1);
    myLcd.print("DFPlayer Hata  ");
  }
}

void muzigiOynat() {
  if (!dfPlayerHazir) {
    ekranaYaz("DFPlayer Yok");
    return;
  }

  myDFPlayer.play(1);

  muzikOynuyor = true;
  ekranaYaz("Muzik Oynuyor");
}

void muzigiDuraklat() {
  if (!dfPlayerHazir) {
    return;
  }

  myDFPlayer.pause();
  muzikOynuyor = false;
  ekranaYaz("Muzik Durdu");
}

void muzigeDevamEt() {
  if (!dfPlayerHazir) {
    return;
  }

  myDFPlayer.start();
  muzikOynuyor = true;
  ekranaYaz("Muzik Devam");
}

void sonrakiSarki() {
  if (!dfPlayerHazir) {
    ekranaYaz("DFPlayer Yok");
    return;
  }

  if (!muzikOynuyor) {
    ekranaYaz("Once Muzik Ac");
    return;
  }

  myDFPlayer.next();
  ekranaYaz("Sonraki Sarki");
}

void oncekiSarki() {
  if (!dfPlayerHazir) {
    ekranaYaz("DFPlayer Yok");
    return;
  }

  if (!muzikOynuyor) {
    ekranaYaz("Once Muzik Ac");
    return;
  }

  myDFPlayer.previous();
  ekranaYaz("Onceki Sarki");
}

// ---------------------FAR-------------------------------
void farAc() {
  if (farPini < 0) {
    ekranaYaz("Far Pini Yok");
    return;
  }

  farAcik = true;
  digitalWrite(farPini, HIGH);
  ekranaYaz("Far Acik");
}

void farKapat() {
  farAcik = false;

  if (farPini >= 0) {
    digitalWrite(farPini, LOW);
  }

  ekranaYaz("Far Kapali");
}

void farPininiYukle() {
  if (EEPROM.read(EEPROM_FAR_IMZA_ADRESI) != EEPROM_FAR_IMZASI) {
    return;
  }

  int kayitliPin = EEPROM.read(EEPROM_FAR_PIN_ADRESI);
  if (farPiniUygunMu(kayitliPin)) {
    farPiniKullan(kayitliPin);
  }
}

void farPininiAyarla(const char* yazi) {
  int pin = pinYazisiniCevir(yazi);

  if (!farPiniUygunMu(pin)) {
    ekranaYaz("Gecersiz Pin");
    Serial.print("Gecersiz far pini: ");
    Serial.println(yazi);
    return;
  }

  farPiniKullan(pin);
  EEPROM.update(EEPROM_FAR_IMZA_ADRESI, EEPROM_FAR_IMZASI);
  EEPROM.update(EEPROM_FAR_PIN_ADRESI, pin);

  ekranaYaz(String("Far Pini: ") + yazi);
  Serial.print("Far pini ayarlandi: ");
  Serial.println(yazi);
}

void farPiniKullan(int pin) {
  // Önceki pini serbest bırak
  if (farPini >= 0 && farPini != pin) {
    digitalWrite(farPini, LOW);
    pinMode(farPini, INPUT);
  }

  farPini = pin;
  pinMode(farPini, OUTPUT);
  digitalWrite(farPini, farAcik ? HIGH : LOW);
}

// Bluetooth (0, 1), DFPlayer (2, 3), mesafe sensörü, motor sürücü ve LCD (I2C) pinleri
// far için kullanılamaz.
bool farPiniUygunMu(int pin) {
  if (pin < 0 || pin >= NUM_DIGITAL_PINS) {
    return false;
  }

  const int kullanilanPinler[] = {0, 1, 2, 3, TRIG_PIN, ECHO_PIN, ENA, IN1, IN2, IN3, IN4, ENB, SDA, SCL};
  for (int kullanilanPin : kullanilanPinler) {
    if (pin == kullanilanPin) {
      return false;
    }
  }

  return true;
}

// "12" veya "A0" biçimindeki pin yazısını pin numarasına çevirir; geçersizse -1 döner.
int pinYazisiniCevir(const char* yazi) {
  bool analog = (yazi[0] == 'A' || yazi[0] == 'a');
  const char* rakamlar = analog ? yazi + 1 : yazi;

  if (rakamlar[0] == '\0') {
    return -1;
  }

  int sayi = 0;
  for (const char* c = rakamlar; *c != '\0'; c++) {
    if (!isDigit(*c)) {
      return -1;
    }
    sayi = sayi * 10 + (*c - '0');
  }

  return analog ? A0 + sayi : sayi;
}

// ---------------------LCD-------------------------------
void ekranaYaz(String hareket) {
  myLcd.setCursor(0, 0);
  myLcd.print("KIRLANGIC FATIH");

  myLcd.setCursor(0, 1);
  myLcd.print("                ");

  myLcd.setCursor(0, 1);
  myLcd.print(hareket);
}