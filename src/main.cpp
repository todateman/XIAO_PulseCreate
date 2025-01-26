// core0はパルス出力, core1は設定変更と表示

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
#define  pulsePin D0                      // D0 pin パルス出力
#define  ModeButton D1                    // D1 pin モード切替ボタン
#define  PButton D2                       // D2 pin +ボタン
#define  MButton D3                       // D3 pin -ボタン
#define  TriggerPin D6                    // D6 pin トリガー入力

// Pulse configuration
volatile unsigned long hiTime = 200;      // パルス出力-Hi時間
volatile unsigned long pwmTime = 7800;    // パルス出力-PWM時間
volatile unsigned long loTime = 500;      // パルス出力-Lo時間
volatile unsigned long hiTime_start = 0;  // パルス出力-Hi時間開始時刻
volatile unsigned long pwmTime_start = 0; // パルス出力-PWM時間開始時刻
volatile unsigned long loTime_start = 0;  // パルス出力-Lo時間開始時刻
volatile uint8_t countmode = 0;           // 0:パルス出力-停止 1:パルス出力-Hi 2:パルス出力-PWM 3:パルス出力-Lo
volatile uint8_t DutyRatio = 25;          // Duty比率(%) 0-100

// LED
#define POWER_PIN 11                      // NeoPixelの電源
#define DIN_PIN 12                        // NeoPixel　の出力ピン番号
#define LED_COUNT 1                       // LEDの連結数
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);

// トリガー入力でパルス停止・開始切り替え
void Suspend() {
  if (digitalRead(TriggerPin)){           // トリガー入力がHIGHの時
    countmode = 1;                          // パルス開始
  }
  else {                                  // トリガー入力がLOWの時          
    digitalWrite(pulsePin, LOW);            // パルス停止
    pixels.clear();                         // NeoPixel消灯
    pixels.show();                          // NeoPixel更新 
    countmode = 0;                          // パルス停止
  }

}

//core0
void setup() {
  pinMode(pulsePin, OUTPUT);              // D0 pin パルス出力
  analogWriteFreq(25000);                 // PWM:25kHz
  analogWriteRange(4096);                 // PWM:12bit(100%)

  pinMode(TriggerPin, INPUT_PULLDOWN);    // D6 pin トリガー入力

  digitalWrite(pulsePin, LOW);            // パルス停止
  pinMode(POWER_PIN, OUTPUT);             // NeoPixelの電源  
  digitalWrite(POWER_PIN, HIGH);          // NeoPixelの電源ON

  pixels.begin();                         //NeoPixel制御開始

  attachInterrupt(TriggerPin, Suspend, CHANGE); // トリガー入力でパルス停止・開始切り替え
}

void loop() {
  static uint8_t beforemode = 0;
  unsigned long DutyOutput = 4096 * DutyRatio / 100 ; // PWM出力値

  if (digitalRead(TriggerPin)) {          // トリガー入力がHIGHの時
    if(countmode == 1) {                    // パルス出力-Hiの場合
      if(countmode != beforemode) {           // 出力モードが切り替わった時
        hiTime_start = millis();                // パルス出力-Hi時間開始時刻を記録
      }
      // digitalWrite(pulsePin, HIGH);
      analogWrite(pulsePin, 4096);            // 100%出力
      pixels.setPixelColor(0, pixels.Color(5, 5, 5));  // WHITE
      pixels.show();                          // NeoPixel更新 
      beforemode = countmode;                 // モードを記憶
      if( millis() - hiTime_start > hiTime){  // パルス出力-Hi時間を経過したら
        countmode = 2;                          // パルス出力-Dutyに切り替え
      }
    }
    if(countmode == 2) {                    // パルス出力-PWMの場合
      if(countmode != beforemode) {           // 出力モードが切り替わった時
        pwmTime_start = millis();               // PWM時間開始時刻を記録
      }
      analogWrite(pulsePin, DutyOutput);      // 設定したDuty比で出力
      pixels.setPixelColor(0, pixels.Color(2, 2, 2));  // WHITE
      pixels.show();                          // NeoPixel更新 
      beforemode = countmode;                 // モードを記憶
      if( millis() - pwmTime_start > pwmTime ){ // パルス出力-PWM時間を経過したら
        countmode = 3;                          // パルス出力-Loに切り替え
      }
    }
    if (countmode == 3) {                   // パルス出力-Loの場合
      if(countmode != beforemode) {           // 出力モードが切り替わった時
        loTime_start = millis();                // Lo time start
      }
      digitalWrite(pulsePin, LOW);            // パルス停止
      pixels.clear();                         // NeoPixel消灯
      pixels.show();                          // NeoPixel更新
      beforemode = countmode;                 // モードを記憶
      if( millis() - loTime_start > loTime){  // Lo timeを経過したら
          countmode = 1;                        // パルス出力-Hiに切り替え
      }
    }
  }
  else {                                  // トリガー入力がLOWの時            
    countmode = 0;                          // パルス出力-停止に切り替え
    digitalWrite(pulsePin, LOW);            // パルス停止
    pixels.clear();                         // NeoPixel消灯  
    pixels.show();                          // NeoPixel更新
    beforemode = countmode;                 // モードを記憶
  }
  delay(10);
}

//core1
void setup1() {
  Serial.begin(115200);
  EEPROM.begin(128);

  pinMode(ModeButton, INPUT_PULLUP);      // D1 pin モード切替ボタン
  pinMode(PButton, INPUT_PULLUP);         // D2 pin +ボタン;
  pinMode(MButton, INPUT_PULLUP);         // D3 pin -ボタン

  Wire.begin();                           // I2C通信開始     
  u8x8.begin();                           // SSD1306初期化
  u8x8.setPowerSave(0);                   // SSD1306電源ON
  u8x8.setFont(u8x8_font_chroma48medium8_r);  // フォント設定
  //u8x8.setFont(u8x8_font_profont29_2x3_r);  // フォント設定
  u8x8.setInverseFont(0);                 // 文字色反転OFF
  u8x8.drawString(0, 0, "Hi  :");         // 画面表示
  u8x8.drawString(0, 1, "PWM :");         // 画面表示
  u8x8.drawString(0, 2, "Lo  :");         // 画面表示
  u8x8.drawString(0, 3, "Duty:");         // 画面表示

  if (EEPROM.read(0) != 255) {            // EEPROMに保存された値がある場合
    EEPROM.get(0, hiTime);                  // EEPROMからパルス出力-Hi時間を読み込み
    EEPROM.get(sizeof(hiTime), pwmTime);    // EEPROMからパルス出力-Hi時間のうち、PWM出力に切り替わる時間を読み込み
    EEPROM.get(sizeof(hiTime)
      + sizeof(pwmTime), loTime  );         // EEPROMからパルス出力-Lo時間を読み込み
    EEPROM.get(sizeof(hiTime)
      + sizeof(pwmTime)
      + sizeof(loTime), DutyRatio);         // EEPROMからDuty比率読み込み
  }
}

void loop1() {
  static bool save = false;               // 保存完了表示
  static unsigned long saveTime;          // 保存完了表示時間 
  static bool ModeButtonPressed = false;  // モード切替ボタンが押された状態
  static unsigned long pressStartTimeMode = 0;  // ボタンが押された時刻
  static uint8_t setmode = 0;             // 0:表示 1:パルス出力-Hi時間設定 2:パルス出力-PWM時間設定 3:パルス出力-Lo時間設定 4:Duty比率設定
  unsigned long hibuf;                    // パルス出力-Hi残り時間
  unsigned long pwmbuf;                   // パルス出力-PWM残り時間
  unsigned long lobuf;                    // パルス出力-Lo残り時間
  unsigned long currentTime = millis();   // 現在時刻

  if (!digitalRead(ModeButton)) {         // モード切替ボタンが押された時
    if (!ModeButtonPressed) {               // ボタンが押された瞬間  
      ModeButtonPressed = true;               // ボタンが押された状態にする
      pressStartTimeMode = currentTime;       // ボタンが押された時刻を記録
    } else if (currentTime - pressStartTimeMode > 1000) { // 長押し判定
      EEPROM.put(0, hiTime);                  // EEPROMにパルス出力-Hi時間を保存
      EEPROM.put(sizeof(hiTime), pwmTime);    // EEPROMにPWM出力切替時間を保存
      EEPROM.put(sizeof(hiTime)
        + sizeof(pwmTime), loTime);           // EEPROMにパルス出力-Lo時間を保存
      EEPROM.put(sizeof(hiTime)
        + sizeof(pwmTime)
        + sizeof(loTime), DutyRatio);         // EEPROMにDuty比率を保存
      Serial.println("Settings saved to EEPROM.");
      if (EEPROM.commit()) {                  // EEPROMに保存 
        Serial.println("EEPROM successfully committed");
      } else {                                // EEPROM保存失敗   
        Serial.println("ERROR! EEPROM commit failed");
      }
      setmode = 4;                            // モードを設定変更に切り替え
      save = true;                            // 保存完了表示   
      saveTime = currentTime;                 // 保存完了表示時間を記録
      pressStartTimeMode = currentTime;       // ボタンが押された時刻を記録
    }
  } else if (ModeButtonPressed) {           // ボタンが離された時
    if (currentTime - pressStartTimeMode < 1000) {        // 短押し判定
      setmode++;                              // モードを次に切り替え 
      if (setmode > 4){ setmode = 0; }        // 最後のモードなら最初のモードに戻す
    }
    ModeButtonPressed = false;              // ボタンが押されていない状態にする
  }

  if (setmode == 1 && !digitalRead(PButton)) {  // パルス出力-Hi時間設定で+ボタンが押された時
    hiTime = min(2000, hiTime + 100);            // パルス出力-Hi時間を増加
  }
  if (setmode == 1 && !digitalRead(MButton)) {  // パルス出力-Hi時間設定で-ボタンが押された時
    hiTime = max(100, hiTime - 100);             // パルス出力-Hi時間を減少
  }

  if (setmode == 2 && !digitalRead(PButton)) {  // PWM出力切替時間設定で+ボタンが押された時
    pwmTime = min(20000, pwmTime + 100);          // PWM出力切替時間を増加
  }
  if (setmode == 2 && !digitalRead(MButton)) {  // PWM出力切替時間設定で-ボタンが押された時
    pwmTime = max(0, pwmTime - 100);              // PWM出力切替時間を減少
  }

  if (setmode == 3 && !digitalRead(PButton)) {  // パルス出力-Lo時間設定で+ボタンが押された時
    loTime = min(2000, loTime + 100);             // パルス出力-Lo時間を増加
  }
  if (setmode == 3 && !digitalRead(MButton)) {  // パルス出力-Lo時間設定で-ボタンが押された時
    loTime = max(500, loTime - 100);              // パルス出力-Lo時間を減少
  }

  if (setmode == 4 && !digitalRead(PButton)) {  // Duty比率設定設定で+ボタンが押された時
    DutyRatio = min(95, DutyRatio + 5);             // Duty比率を増加
  }
  if (setmode == 4 && !digitalRead(MButton)) {  // Duty比率設定設定で-ボタンが押された時
    DutyRatio = max(5, DutyRatio - 5);              // Duty比率を減少
  }

  // 画面表示
  u8x8.setInverseFont(0);                       // 文字色反転OFF
    
  u8x8.setCursor(5, 0);                         // 画面表示位置設定
  hibuf = hiTime - (millis() - hiTime_start);   // パルス出力-Hiの残り時間を算出
  if (countmode != 1) {hibuf = hiTime;}         // パルス出力-Hiモード以外の場合は設定時間を表示
  if (hibuf/1000 < 10) {u8x8.print(" "); }      // パルス出力-Hiの残り時間が1桁の場合は空白を表示
  u8x8.print(float(hibuf/1000.0), 1);           // パルス出力-Hiの残り時間を表示
  u8x8.print(" ");

  u8x8.setCursor(10, 0);
  u8x8.print("/");

  u8x8.setCursor(11, 0);
  if (hiTime/1000 < 10) {u8x8.print(" "); }     // パルス出力-Hi時間が1桁の場合は空白を表示
  if (setmode == 1) { u8x8.setInverseFont(1); } // 設定変更中は文字色反転ON
  else { u8x8.setInverseFont(0); }              // 設定変更中以外は文字色反転OFF
  u8x8.print(float(hiTime/1000.0), 1);          // パルス出力-Hi時間を表示 
  u8x8.setInverseFont(0);                       // 文字色反転OFF
  u8x8.print(" ");

  u8x8.setCursor(5, 1);                         // 画面表示位置設定
  pwmbuf = pwmTime - (millis() - pwmTime_start);  // パルス出力-PWMの残り時間を算出
  if (countmode != 2) {pwmbuf = pwmTime;}       // パルス出力-PWMモード以外の場合は設定時間を表示
  if (pwmbuf/1000 < 10) {u8x8.print(" "); }     // パルス出力-PWMの残り時間が1桁の場合は空白を表示
  u8x8.print(float(pwmbuf/1000.0), 1);          // 残り時間を表示
  u8x8.print(" ");

  u8x8.setCursor(10, 1);
  u8x8.print("/");

  u8x8.setCursor(11, 1);
  if (pwmTime/1000 < 10) {u8x8.print(" "); }    // PWM出力切替時間が1桁の場合は空白を表示
  if (setmode == 2) {u8x8.setInverseFont(1);}   // 設定変更中は文字色反転ON
  else { u8x8.setInverseFont(0); }              // 設定変更中以外は文字色反転OFF
  u8x8.print(float(pwmTime/1000.0), 1);         // PWM出力切替時間を表示
  u8x8.setInverseFont(0);                       // 文字色反転OFF
  u8x8.print(" ");
    
  u8x8.setCursor(5, 2);                         // 画面表示位置設定
  lobuf = loTime - (millis() - loTime_start);   // パルス出力-Loの残り時間を算出
  if (countmode != 3) { lobuf = loTime;}        // パルス出力-Loモード以外の場合は設定時間を表示
  if (lobuf/1000 < 10) {u8x8.print(" "); }      // パルス出力-Loの残り時間が1桁の場合は空白を表示
  u8x8.print(float(lobuf/1000.0), 1);           // 残り時間を表示
  u8x8.print(" ");

  u8x8.setCursor(10, 2);
  u8x8.print("/");

  u8x8.setCursor(11, 2);
  if (loTime/1000 < 10) {u8x8.print(" "); }     // パルス出力-Lo時間が1桁の場合は空白を表示
  if (setmode == 3) {u8x8.setInverseFont(1);}   // 設定変更中は文字色反転ON
  else { u8x8.setInverseFont(0); }              // 設定変更中以外は文字色反転OFF
  u8x8.print(float(loTime/1000.0), 1);          // パルス出力-Lo時間を表示
  u8x8.setInverseFont(0);                       // 文字色反転OFF
  u8x8.print(" ");

  u8x8.setCursor(5, 3);
  if (DutyRatio < 10) {u8x8.print(" "); }       // Duty比率が1桁の場合は空白を表示
  if (setmode == 4) {u8x8.setInverseFont(1);}   // 設定変更中は文字色反転ON
  else { u8x8.setInverseFont(0); }              // 設定変更中以外は文字色反転OFF
  u8x8.print(DutyRatio);                        // Duty比率を表示
  u8x8.setInverseFont(0);                       // 文字色反転OFF
  u8x8.print(" ");

  if (save) {                                   // 保存完了表示
    if (currentTime - saveTime < 2000){           // 保存完了表示時間未満なら
      u8x8.setInverseFont(1);                       // 文字色反転ON  
      u8x8.setCursor(0, 2);                         // 画面表示位置設定
      u8x8.print("SAVE!");                          // 画面表示
    }
    else {                                        // 保存完了表示時間経過後
      u8x8.setInverseFont(0);                       // 文字色反転OFF
      u8x8.setCursor(0, 2);                         // 画面表示位置設定
      u8x8.print("     ");                          // 画面表示
      save = false;                                 // 保存完了表示OFF
    }
  }
  u8x8.setInverseFont(0);                       // 文字色反転OFF

  //Serial.print(outputmode);
  Serial.print(countmode);
  Serial.print(", ");
  Serial.print(hiTime);
  Serial.print(", ");
  Serial.print(pwmTime);
  Serial.print(", ");
  Serial.print(loTime);
  Serial.print(", ");
  Serial.print(DutyRatio);
  Serial.print(" / ");
  Serial.println("4098");

  delay(100);
}