#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

/* --- CẤU HÌNH --- */
#define SENSOR_ADDR 8
#define ACTUATOR_ADDR 9
#define BUZZER A2
#define MAX_CARDS 10

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SS_PIN 10
#define RST_PIN A0
MFRC522 rfid(SS_PIN, RST_PIN);

/* --- DỮ LIỆU --- */
byte masterUID[4] = {0x50, 0x07, 0x4B, 0x9B};
String adminPass = "9999";
String input = "";
float temp, hum;
int lightValue;
unsigned long lastSensorRead = 0;

/* --- KEYPAD --- */
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  Wire.begin();
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();

  if (EEPROM.read(0) > MAX_CARDS) EEPROM.write(0, 0);

  lcd.print("SMART HOME v2");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Cập nhật cảm biến mỗi 2 giây để không làm lag hệ thống
  if (millis() - lastSensorRead > 2000) {
    readSensor();
    lastSensorRead = millis();
  }
  checkRFID();
  checkKeypad();
}

/* --- HÀM CẢM BIẾN --- */
void readSensor() {
  Wire.requestFrom(SENSOR_ADDR, 32);
  String data = "";
  while (Wire.available()) { data += (char)Wire.read(); }
  
  if (data.length() > 0) {
    int i1 = data.indexOf(',');
    int i2 = data.indexOf(',', i1 + 1);
    if (i1 != -1 && i2 != -1) {
      temp = data.substring(0, i1).toFloat();
      hum = data.substring(i1 + 1, i2).toFloat();
      lightValue = data.substring(i2 + 1).toInt();
      displaySensor();
    }
  }
}

void displaySensor() {
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print((int)temp);
  lcd.print("C H:"); lcd.print((int)hum); lcd.print("%  ");
  lcd.setCursor(0, 1);
  lcd.print("Light: "); lcd.print(lightValue); lcd.print("    ");
}

/* --- HÀM RFID --- */
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  tone(BUZZER, 1000, 100);
  byte uid[4];
  for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];

  if (compareUID(uid, masterUID)) {
    enterAdminPassword();
  } else if (checkCard(uid)) {
    openDoor();
  } else {
    lcd.clear(); lcd.print("DENIED!");
    tone(BUZZER, 500, 800);
    delay(1500); lcd.clear();
  }
  rfid.PICC_HaltA();
}

bool compareUID(byte a[], byte b[]) {
  for (byte i = 0; i < 4; i++) { if (a[i] != b[i]) return false; }
  return true;
}

bool checkCard(byte uid[]) {
  int total = EEPROM.read(0);
  for (int i = 0; i < total; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (uid[j] != EEPROM.read(1 + i * 4 + j)) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

/* --- ADMIN FUNCTIONS --- */
void addCard() {
  lcd.clear(); 
  lcd.print("Scan New Card");

  unsigned long start = millis();

  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    if (millis() - start > 10000) {
      lcd.clear(); 
      lcd.print("Timeout!");
      delay(1000); 
      return;
    }
  }

  byte newUID[4];
  for (byte i = 0; i < 4; i++) 
    newUID[i] = rfid.uid.uidByte[i];

  // 🔥 CHECK TRÙNG UID
  if (checkCard(newUID)) {
    lcd.clear();
    lcd.print("Card Exists!");
    tone(BUZZER, 1500, 300);
    delay(1500);
    return;
  }

  int total = EEPROM.read(0);

  if (total >= MAX_CARDS) {
    lcd.clear(); 
    lcd.print("Full Memory!");
    delay(1500); 
    return;
  }

  for (int i = 0; i < 4; i++) 
    EEPROM.write(1 + total * 4 + i, newUID[i]);

  EEPROM.write(0, total + 1);

  lcd.clear(); 
  lcd.print("Added Success");
  tone(BUZZER, 1000, 200);
  delay(1500);
}

void deleteCard() {
  lcd.clear(); lcd.print("Scan to Delete");
  unsigned long start = millis();
  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    if (millis() - start > 10000) return;
  }

  byte uid[4];
  for (byte i = 0; i < 4; i++) uid[i] = rfid.uid.uidByte[i];
  int total = EEPROM.read(0);

  for (int i = 0; i < total; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (uid[j] != EEPROM.read(1 + i * 4 + j)) { match = false; break; }
    }
    if (match) {
      for (int k = i; k < total - 1; k++) {
        for (int j = 0; j < 4; j++) EEPROM.write(1 + k * 4 + j, EEPROM.read(1 + (k + 1) * 4 + j));
      }
      EEPROM.write(0, total - 1);
      lcd.clear(); lcd.print("Deleted OK"); delay(1500); return;
    }
  }
}

void listCards() {
  int total = EEPROM.read(0);
  lcd.clear(); lcd.print("User Cards:");
  lcd.setCursor(0, 1); lcd.print(total);
  delay(2000);
}

void enterAdminPassword() {
  lcd.clear(); lcd.print("Password:");
  String pass = "";
  while (true) {
    char key = keypad.getKey();
    if (!key) continue;
    if (key == '#') {
      if (pass == adminPass) adminMenu();
      else { lcd.clear(); lcd.print("Wrong!"); delay(1500); }
      return;
    }
    if (key == '*') { return; }
    pass += key;
    lcd.setCursor(0, 1); lcd.print(pass);
  }
}

void adminMenu() {

  int option = 0;
  const int maxOption = 3;

  String menuItems[4] = {
    "Add Card",
    "Delete Card",
    "List Cards",
    "Exit"
  };

  int lastOption = -1; // để kiểm tra thay đổi

  while (true) {

    // ✅ Chỉ update LCD khi option thay đổi
    if (option != lastOption) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(">" + menuItems[option]);

      lcd.setCursor(0, 1);
      lcd.print("A:Up B:Down #");

      lastOption = option;
    }

    char key = keypad.getKey();
    if (!key) continue;

    tone(BUZZER, 800, 50); // feedback

    if (key == 'A') {
      option--;
      if (option < 0) option = maxOption;
    }

    else if (key == 'B') {
      option++;
      if (option > maxOption) option = 0;
    }

    else if (key == '#') {

      lcd.clear();

      switch (option) {
        case 0: addCard(); break;
        case 1: deleteCard(); break;
        case 2: listCards(); break;
        case 3:
          lcd.print("Exit Admin");
          delay(1000);
          lcd.clear();
          return;
      }

      lastOption = -1; // force redraw sau khi quay lại menu
    }

    else if (key == '*') {
      lcd.clear();
      return;
    }

    delay(150); // ✅ chống dội phím + giảm lag LCD
  }
}

/* --- CƠ CẤU CHẤP HÀNH --- */
void openDoor() {
  lcd.clear(); lcd.print("DOOR OPENING");
  sendCommand(5); 
  delay(3000);
  sendCommand(6); 
  lcd.clear();
}

void sendCommand(int cmd) {
  Wire.beginTransmission(ACTUATOR_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}

void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  tone(BUZZER, 800, 50);

  if (key == '*') { input = ""; lcd.clear(); }
  else if (key == '#') {
    if (input == "1234") openDoor();
    else { lcd.clear(); lcd.print("Wrong Pass"); delay(1500); }
    input = ""; lcd.clear();
  } else {
    input += key;
    lcd.setCursor(0, 1); lcd.print(input);
  }
}
