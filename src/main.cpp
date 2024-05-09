#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <U8x8lib.h>
#include <FreeRTOS_SAMD21.h>
#include <FlashStorage.h>

TaskHandle_t TaskPulse_Handler;
TaskHandle_t TaskDisplay_Handler;

// SSD1306 display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);  // 0.91"

// Pin configuration
const int pulsePin = 0;   // D0 pin
const int ModeButton = 1; // D1 pin
const int PButton = 2;    // D2 pin
const int MButton = 3;    // D3 pin
const int TriggerPin = 6; // D6 pin

// Pulse timing
volatile unsigned long hiTime = 2000; // Hi time in milliseconds
volatile unsigned long loTime = 500; // Lo time in milliseconds
volatile unsigned long hiTime_start = 0;
volatile unsigned long loTime_start = 0;
volatile uint8_t countmode = 0;

// Flash storage structure
struct TimeSettings {
  unsigned long hiTime;
  unsigned long loTime;
};
FlashStorage(time_storage, TimeSettings);

void TaskPulse(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    digitalWrite(pulsePin, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    hiTime_start = millis();
    countmode = 1;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(hiTime));

    digitalWrite(pulsePin, LOW);
    digitalWrite(LED_BUILTIN, HIGH);
    loTime_start = millis();
    countmode = 2;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(loTime));
  }
}

void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  bool save = false;
  unsigned long saveTime;
  bool ModeButtonPressed = false;
  unsigned long pressStartTimeMode = 0;
  uint8_t mode = 0;
  unsigned long hibuf;
  unsigned long lobuf;
  for (;;) {
    unsigned long currentTime = millis();

    if (!digitalRead(ModeButton)) {
      if (!ModeButtonPressed) {
        ModeButtonPressed = true;
        pressStartTimeMode = currentTime;
      } else if (currentTime - pressStartTimeMode > 1000) { // 長押し判定
        // TimeSettings settings = {hiTime, loTime};
        // time_storage.write(settings);
        mode = 2;
        save = true;
        saveTime = currentTime;
        pressStartTimeMode = currentTime; // Reset start time to prevent multiple increments
      }
    } else if (ModeButtonPressed) {
      if (currentTime - pressStartTimeMode < 1000) {        // 短押し判定
        mode++;
        if (mode > 2){ mode = 0; }
      }
      ModeButtonPressed = false;
    }

    if (mode == 1 && !digitalRead(PButton)) {
      hiTime = min(20000, hiTime + 100);
    }
    if (mode == 1 && !digitalRead(MButton)) {
      hiTime = max(1000, hiTime - 100);
    }

    if (mode == 2 && !digitalRead(PButton)) {
      loTime = min(2000, loTime + 100);
    }
    if (mode == 2 && !digitalRead(MButton)) {
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
    if (mode == 1) { u8x8.setInverseFont(1); }
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
    if (mode == 2) {u8x8.setInverseFont(1);}
    else { u8x8.setInverseFont(0); }
    u8x8.print(float(loTime/1000.0), 1);
    u8x8.setInverseFont(0);
    u8x8.print(" ");

    if (save) {
      if (currentTime - saveTime < 2000){
        u8x8.setInverseFont(1);
        u8x8.setCursor(0, 2);
        u8x8.print("SAVE!");
      } else {
        u8x8.setInverseFont(0);
        u8x8.setCursor(0, 2);
        u8x8.print("     ");
        save = false;
      }
    }
    u8x8.setInverseFont(0);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
  }
}

/*
void Suspend() {
  if (digitalRead(TriggerPin)) {  // トリガーピンに3.3Vが入力された場合
    xTaskResumeAll();             // タスクを再開
  }
  else {
    vTaskSuspendAll();            // タスクを休止
  }
}
*/

void setup() {
  pinMode(pulsePin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ModeButton, INPUT_PULLUP);  
  pinMode(PButton, INPUT_PULLUP);
  pinMode(MButton, INPUT_PULLUP);
  pinMode(TriggerPin, INPUT_PULLDOWN);

  digitalWrite(pulsePin, LOW);
  digitalWrite(LED_BUILTIN, HIGH);

  Wire.begin();
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  //u8x8.setFont(u8x8_font_profont29_2x3_r);
  u8x8.setInverseFont(0);
  u8x8.drawString(0, 0, "Hi:");
  u8x8.drawString(0, 1, "Lo:");

  // Load times from flash storage if available
  TimeSettings savedTimes = time_storage.read();
  if (savedTimes.hiTime > 0 && savedTimes.loTime > 0) {
    hiTime = savedTimes.hiTime;
    loTime = savedTimes.loTime;
  }

  //attachInterrupt(TriggerPin, Suspend, CHANGE);

  // FreeRTOS task creation
  xTaskCreate(TaskPulse, "Pulse", 128, NULL, tskIDLE_PRIORITY + 2, TaskPulse_Handler);
  xTaskCreate(TaskDisplay, "Display", 256, NULL, tskIDLE_PRIORITY + 1, TaskDisplay_Handler);

  vTaskStartScheduler();
}

void loop() {
  delay(1);
}