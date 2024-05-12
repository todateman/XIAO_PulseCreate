#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <U8x8lib.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// SSD1306 display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);  // 0.91"

// Pin configuration
#define  pulsePin D0   // D0 pin
#define  ModeButton D1 // D1 pin
#define  PButton D2    // D2 pin
#define  MButton D3    // D3 pin
#define  TriggerPin D6 // D6 pin

// Pulse timing
volatile unsigned long hiTime = 2000; // Hi time in milliseconds
volatile unsigned long loTime = 500; // Lo time in milliseconds
volatile unsigned long hiTime_start = 0;
volatile unsigned long loTime_start = 0;
volatile uint8_t countmode = 0;

// LED
#define POWER_PIN 11          //NeoPixelの電源
#define DIN_PIN 12            // NeoPixel　の出力ピン番号
#define LED_COUNT 1           // LEDの連結数
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);

void Suspend() {
  if (digitalRead(TriggerPin)){
    countmode = 1;
  }
  else {
    digitalWrite(pulsePin, LOW);
    pixels.clear();
    pixels.show();
    countmode = 0;
  }

}

//core0
void setup() {
  pinMode(pulsePin, OUTPUT);
  pinMode(TriggerPin, INPUT_PULLDOWN);

  digitalWrite(pulsePin, LOW);
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);

  pixels.begin();             //NeoPixel制御開始

  attachInterrupt(TriggerPin, Suspend, CHANGE);
}

void loop() {
  static uint8_t beforemode = 0;
  if (digitalRead(TriggerPin)) {
    if(countmode == 1) {
      if(countmode != beforemode) {
        hiTime_start = millis();
      }
      digitalWrite(pulsePin, HIGH);
      pixels.setPixelColor(0, pixels.Color(5, 5, 5));  // WHITE
      pixels.show();
      beforemode = countmode;
      if( millis() - hiTime_start > hiTime){
        countmode = 2;
      }
    }
    if (countmode == 2) {
      if(countmode != beforemode) {
        loTime_start = millis();
      }
      digitalWrite(pulsePin, LOW);
      pixels.clear();
      pixels.show();
      beforemode = countmode;
      if( millis() - loTime_start > loTime){
          countmode = 1;
      }
    }
  }
  else {
    countmode = 0;
    digitalWrite(pulsePin, LOW);
    pixels.clear();
    pixels.show();
    beforemode = countmode;
  }
  delay(10);
}

//core1
void setup1() {
  Serial.begin(115200);
  EEPROM.begin(128);

  pinMode(ModeButton, INPUT_PULLUP);  
  pinMode(PButton, INPUT_PULLUP);
  pinMode(MButton, INPUT_PULLUP);

  Wire.begin();
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  //u8x8.setFont(u8x8_font_profont29_2x3_r);
  u8x8.setInverseFont(0);
  u8x8.drawString(0, 0, "Hi:");
  u8x8.drawString(0, 1, "Lo:");

  // Read hiTime and loTime from EEPROM
  EEPROM.get(0, hiTime);
  EEPROM.get(sizeof(hiTime), loTime);
}

void loop1() {
  static bool save = false;
  static unsigned long saveTime;
  static bool ModeButtonPressed = false;
  static unsigned long pressStartTimeMode = 0;
  static uint8_t setmode = 0;
  unsigned long hibuf;
  unsigned long lobuf;
  unsigned long currentTime = millis();

  if (!digitalRead(ModeButton)) {
    if (!ModeButtonPressed) {
      ModeButtonPressed = true;
      pressStartTimeMode = currentTime;
    } else if (currentTime - pressStartTimeMode > 1000) { // 長押し判定
      EEPROM.put(0, hiTime);
      EEPROM.put(sizeof(hiTime), loTime);
      Serial.println("Settings saved to EEPROM.");
      if (EEPROM.commit()) {
        Serial.println("EEPROM successfully committed");
      } else {
        Serial.println("ERROR! EEPROM commit failed");
      }
      setmode = 2;
      save = true;
      saveTime = currentTime;
      pressStartTimeMode = currentTime; // Reset start time to prevent multiple increments
    }
  } else if (ModeButtonPressed) {
    if (currentTime - pressStartTimeMode < 1000) {        // 短押し判定
      setmode++;
      if (setmode > 2){ setmode = 0; }
    }
    ModeButtonPressed = false;
  }

  if (setmode == 1 && !digitalRead(PButton)) {
    hiTime = min(20000, hiTime + 100);
  }
  if (setmode == 1 && !digitalRead(MButton)) {
    hiTime = max(1000, hiTime - 100);
  }

  if (setmode == 2 && !digitalRead(PButton)) {
    loTime = min(2000, loTime + 100);
  }
  if (setmode == 2 && !digitalRead(MButton)) {
    loTime = max(500, loTime - 100);
  }

  // 画面表示
  u8x8.setInverseFont(0);
    
  u8x8.setCursor(3, 0);
  hibuf = hiTime - (millis() - hiTime_start);
  if (countmode != 1) {hibuf = hiTime;}
  if (hibuf/1000 < 10) {u8x8.print(" "); }
  u8x8.print(float(hibuf/1000.0), 1);
  u8x8.print(" ");

  u8x8.setCursor(8, 0);
  u8x8.print("/");

  u8x8.setCursor(9, 0);
  if (hiTime/1000 < 10) {u8x8.print(" "); }
  if (setmode == 1) { u8x8.setInverseFont(1); }
  else { u8x8.setInverseFont(0); }
  u8x8.print(float(hiTime/1000.0), 1);
  u8x8.setInverseFont(0);
  u8x8.print(" ");

    
  u8x8.setCursor(3, 1);
  lobuf = loTime - (millis() - loTime_start);
  if (countmode != 2) { lobuf = loTime;}
  if (lobuf/1000 < 10) {u8x8.print(" "); }
  u8x8.print(float(lobuf/1000.0), 1);
  u8x8.print(" ");

  u8x8.setCursor(8, 1);
  u8x8.print("/");

  u8x8.setCursor(9, 1);
  if (loTime/1000 < 10) {u8x8.print(" "); }
  if (setmode == 2) {u8x8.setInverseFont(1);}
  else { u8x8.setInverseFont(0); }
  u8x8.print(float(loTime/1000.0), 1);
  u8x8.setInverseFont(0);
  u8x8.print(" ");

  if (save) {
    if (currentTime - saveTime < 2000){
      u8x8.setInverseFont(1);
      u8x8.setCursor(0, 2);
      u8x8.print("SAVE!");
    }
    else {
      u8x8.setInverseFont(0);
      u8x8.setCursor(0, 2);
      u8x8.print("     ");
      save = false;
    }
  }
  u8x8.setInverseFont(0);

  //Serial.print(outputmode);
  Serial.print(countmode);
  Serial.print(", ");
  Serial.print(hiTime);
  Serial.print(", ");
  Serial.println(loTime);

  delay(100);
}