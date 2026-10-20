#include <QTRSensors.h>

const uint8_t ENA = 5;  
const uint8_t IN1 = 6;  
const uint8_t IN2 = 7;  
const uint8_t IN3 = 8;  
const uint8_t IN4 = 9;  
const uint8_t ENB = 10; 

// --- QTR SENSÖR AYARLARI ---
QTRSensors qtr;
const uint8_t SensorCount = 6; // A0'dan A5'e kadar 6 sensör kullanıyoruz
uint16_t sensorValues[SensorCount];

// --- PID KONTROL DEĞİŞKENLERİ ---
float Kp = 0.08; 
float Kd = 0.35;  
int lastError = 0;

// --- HIZ LİMİTLERİ ---
const int maxSpeed = 255;  
const int baseSpeed = 125; 

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  qtr.setTypeAnalog(); 
  qtr.setSensorPins((const uint8_t[]){A5, A4, A3, A2, A1, A0}, SensorCount);

  // Kalibrasyon (Robotu çizginin üzerinde sağa sola gezdir)
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 

  // --- KALİBRASYON SÜRESİ GÜNCELLENDİ (3 SANİYE) ---
  for (uint16_t i = 0; i < 120; i++) {
    qtr.calibrate();
  }
  
  digitalWrite(LED_BUILTIN, LOW); 
  delay(1000); 
}

void loop() {
  uint16_t position = qtr.readLineBlack(sensorValues);
  int error = position - 2500;

  int motorSpeed = (Kp * error) + (Kd * (error - lastError));
  lastError = error;

  int leftMotorSpeed = baseSpeed + motorSpeed;
  int rightMotorSpeed = baseSpeed - motorSpeed;

  leftMotorSpeed = constrain(leftMotorSpeed, -maxSpeed, maxSpeed);
  rightMotorSpeed = constrain(rightMotorSpeed, -maxSpeed, maxSpeed);

  setMotors(leftMotorSpeed, rightMotorSpeed);
}

void setMotors(int leftSpeed, int rightSpeed) {

  // --- SOL MOTOR ---
  if (leftSpeed >= 0) {
    // İLERİ
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } 
  else {
    // GERİ
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    leftSpeed = -leftSpeed;  // PWM için pozitif değere çevir
  }

  analogWrite(ENA, constrain(leftSpeed, 0, 255));


  // --- SAĞ MOTOR ---
  if (rightSpeed >= 0) {
    // İLERİ
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } 
  else {
    // GERİ
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    rightSpeed = -rightSpeed;
  }

  analogWrite(ENB, constrain(rightSpeed, 0, 255));
}