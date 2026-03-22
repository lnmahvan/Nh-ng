// ===== GIỮ NGUYÊN LIB =====
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ===== CONFIG =====
#define SENSOR_ADDR 8
#define ACTUATOR_ADDR 9
#define BUZZER A2

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== RFID =====
#define SS_PIN 10
#define RST_PIN A0
MFRC522 rfid(SS_PIN, RST_PIN);

// ===== SENSOR DATA =====
byte sensor[6];

// ===== COMMAND =====
byte cmd[4] = {0,0,0,0};

// ===== STATE =====
bool securityMode = false;
unsigned long lastMotionTime = 0;

// ===== KEYPAD =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},
                         {'4','5','6','B'},
                         {'7','8','9','C'},
                         {'*','0','#','D'}};

byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8,9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== SETUP =====
void setup(){
  Serial.begin(9600);
  Wire.begin();
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  pinMode(BUZZER, OUTPUT);

  lcd.print("MASTER READY");
  delay(1000);
  lcd.clear();
}

// ===== LOOP =====
void loop(){

  readSensorI2C();

  handleSmartLogic();

  sendCommand();

  checkRFID();     // GIỮ NGUYÊN
  checkKeypad();   // GIỮ NGUYÊN
}

// ===== I2C READ =====
void readSensorI2C(){

  Wire.requestFrom(SENSOR_ADDR,6);

  for(int i=0;i<6;i++){
    if(Wire.available()) sensor[i]=Wire.read();
  }
}

// ===== SMART LOGIC =====
void handleSmartLogic(){

  int temp = sensor[0];
  int light = sensor[2];
  bool rain = sensor[3];
  bool motion = sensor[4];
  bool sound = sensor[5];

  // ===== SECURITY =====
  if(motion && sound && securityMode){
    lcd.clear();
    lcd.print("INTRUDER!");
    tone(BUZZER,1000,500);

    cmd[0] = 0; // lock door
  }

  // ===== WINDOW =====
  if(rain){
    cmd[1] = 0;
  }
  else if(securityMode){
    cmd[1] = 0;
  }
  else if(temp > 30){
    cmd[1] = 1;
  }
  else{
    cmd[1] = 0;
  }

  // ===== CURTAIN =====
  if(light > 180) cmd[2] = 0;
  else if(light < 80) cmd[2] = 1;

  // ===== ENERGY =====
  if(motion){
    lastMotionTime = millis();
    cmd[3] = 1;
  }

  if(millis() - lastMotionTime > 300000){
    cmd[3] = 0;
  }
}

// ===== SEND =====
void sendCommand(){
  Wire.beginTransmission(ACTUATOR_ADDR);
  Wire.write(cmd,4);
  Wire.endTransmission();
}
