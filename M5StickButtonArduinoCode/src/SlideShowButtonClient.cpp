#include <Arduino.h>
#include <M5StickCPlus.h>
#include <WiFi.h>
#include "AXP192.h"

// Device and pin definitions
const uint16_t deviceID = 0;
#define MOSFET_GATE_PIN 25      // To control the light
#define BUTTON_PIN 26           // External button 1
#define SECOND_BUTTON_PIN 32    // External button 2
const uint16_t port  = 8095;    // Port to communicate with the server

// WiFi and host settings
const char* ssid     = "C2 Private";
const char* password = "your wifi password";
const char* host     = "192.168.1.5";
const char* secondary_host = "192.168.1.75";
const char* third_host = "192.168.1.72";

String activeHost;

// Timers and other globals
unsigned long lastButtonPress = 0;
unsigned long screenTimeout = 10000; // Screen timeout in ms
bool bUseSleepMode = false;          // Optionally enable sleep mode
unsigned long ExternalButtonFlashingLightTime = 10 * 1000;
unsigned long ExternalButtonTimePerFlash = 300;
int secondButtonLastState = HIGH;

// KeepAlive variables
unsigned long lastKeepAliveCheck = 0;
const unsigned long KEEP_ALIVE_INTERVAL = 10UL * 60 * 1000; // 10 minutes

// RTC time
RTC_TimeTypeDef StartTime;

// Button state machine definitions
enum ButtonState {
  IDLE,
  PRESSED,
  RELEASED,
  DOUBLE_PRESSED,
  LONG_PRESSED,
};

ButtonState stateA = IDLE;
uint32_t buttonPressTimeA;
uint32_t buttonReleaseTimeA;

ButtonState stateB = IDLE;
uint32_t buttonPressTimeB;
uint32_t buttonReleaseTimeB;

// Global persistent WiFiClient
WiFiClient client;

void ShowTimeElapsed();
void ConnectToWifi(bool bForceDisconnect);
void ClearScreen();
void KeepAliveChecker();
void SendMessage(int myDeviceID, String msg);

void SetStartTime() {
    M5.Rtc.GetTime(&StartTime);
}

void ShowTimeElapsed() {
    RTC_TimeTypeDef CurrentTime;
    M5.Rtc.GetTime(&CurrentTime);

    int elapsedHours = CurrentTime.Hours - StartTime.Hours;
    int elapsedMinutes = CurrentTime.Minutes - StartTime.Minutes;
    int elapsedSeconds = CurrentTime.Seconds - StartTime.Seconds;

    // Adjust if negative
    if (elapsedSeconds < 0) {
        elapsedSeconds += 60;
        elapsedMinutes--;
    }
    if (elapsedMinutes < 0) {
        elapsedMinutes += 60;
        elapsedHours--;
    }
    M5.Lcd.printf("Runtime: %02d:%02d:%02d\n", elapsedHours, elapsedMinutes, elapsedSeconds);
}

int getBatteryLevel() {
  float voltage = M5.Axp.GetBatVoltage();
  if(voltage > 4.2) 
    return 100;
  else if(voltage < 3.3)
    return 0;
  else
    return (int)((voltage - 3.3) / (4.2 - 3.3) * 100);
}

void ShowBatteryLevel() {
  M5.Lcd.print("Bat: ");
  M5.Lcd.print(getBatteryLevel());
  M5.Lcd.println("%");
}

// Modified to use the global WiFiClient
void ConnectToWifi(bool bForceDisconnect) {
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) 
  {
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          M5.Lcd.print(".");
      }
      M5.Lcd.print("IP: ");
      M5.Lcd.println(WiFi.localIP());
  }
  
  // If already connected via our client and not forcing a reconnect, just return.
  if (client.connected()) 
  {
      if (bForceDisconnect) 
      {
          client.stop();
      } else 
      {
      }
  } 

  if (!client.connected())
  {
    activeHost = host;
    if (!client.connect(activeHost.c_str(), port)) {
        ClearScreen();
        M5.Lcd.println("Failed, trying secondary");
        client.stop();
        if (!client.connect(secondary_host, port)) {
            ClearScreen();
            M5.Lcd.println("Failed, trying third");
            client.stop();
            if (!client.connect(third_host, port)) {
               M5.Lcd.println("Third failed, no host");
               return;
            } else {
               activeHost = third_host;
            }
        } else {
            activeHost = secondary_host;
        }
    }
  
  }
  
  // Try connecting to the primary host, or fall back if needed.
 
  SendMessage(deviceID, "_Dummy");

  M5.Lcd.println("Host found.");
}

void TurnScreenOff() {
  Wire1.beginTransmission(0x34);
  Wire1.write(0x12);
  Wire1.write(0b01001011);  // Turn OLED_VDD off
  Wire1.endTransmission();
}

void TurnScreenOn() {
  Wire1.beginTransmission(0x34);
  Wire1.write(0x12);
  Wire1.write(0x4d); // Enable OLED_VDD
  Wire1.endTransmission();
}

void setup() {
  lastButtonPress = millis();
  M5.begin(true, true, false);
  M5.Lcd.setRotation(3);
  TurnScreenOn();
  M5.Lcd.setTextSize(2);
  SetStartTime();
  ShowBatteryLevel();
  ShowTimeElapsed();
  ConnectToWifi(false);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOSFET_GATE_PIN, OUTPUT);
  pinMode(SECOND_BUTTON_PIN, INPUT_PULLUP);
}

void ClearScreen() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
}

// Modified to use the global client and not disconnect after sending
void SendMessage(int myDeviceID, String msg) 
{
  lastButtonPress = millis();
  //M5.Beep.tone(4000);
  //delay(40);
  //M5.Beep.mute();

  TurnScreenOn();
  ClearScreen();
  String message = "ID" + String(myDeviceID) + msg;
  M5.Lcd.println(message);
  ShowTimeElapsed();

  if (!client.connected())
   {
      M5.Lcd.println("Connection lost, reconnecting...");
      delay(500);
      ConnectToWifi(true);
      if (!client.connected())
       {
         M5.Lcd.println("Failed to reconnect. Message not sent.");
         return;
      }
    }
  
  client.print(message);
  M5.Lcd.println("Sent.");
  // Note: We leave the connection open.
  ShowBatteryLevel();
}

void EnterLightSleepUntilAButtonIsPressed() 
{
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0); // Wake up on button press
  esp_light_sleep_start();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  TurnScreenOn();
  ConnectToWifi(false);
  ShowBatteryLevel();
  delay(500);
  lastButtonPress = millis();
}

void PushedExternalButton(String buttonName) {
  digitalWrite(MOSFET_GATE_PIN, HIGH);
  //M5.Beep.tone(4000);
  //delay(40);
  //M5.Beep.mute();
  
  SendMessage(deviceID, buttonName);
  
  unsigned long waitTimer = millis() + ExternalButtonFlashingLightTime;
  while(waitTimer > millis()) {
    digitalWrite(MOSFET_GATE_PIN, HIGH);
    delay(ExternalButtonTimePerFlash);
    digitalWrite(MOSFET_GATE_PIN, LOW);
    delay(ExternalButtonTimePerFlash);
  }
}

// New keep-alive function to check and reconnect if needed.
void KeepAliveChecker() {
    if (!client.connected()) {
        M5.Lcd.println("KeepAlive: connection lost. Reconnecting...");
        SendMessage(deviceID, "_Dummy");
    
    } else {
        M5.Lcd.println("KeepAlive: connection is healthy.");
    }
}

void loop() {
    M5.update(); // Update button states

    // --- Button A state machine ---
    switch (stateA) {
        case IDLE:
            if (M5.BtnA.isPressed()) {
                stateA = PRESSED;
                buttonPressTimeA = millis();
            }
            break;
        case PRESSED:
            if (!M5.BtnA.isPressed()) {
                stateA = RELEASED;
                buttonReleaseTimeA = millis();
            } else if ((millis() - buttonPressTimeA) > 1000) {
                SendMessage(deviceID, "_ButtonA_Long");
                stateA = LONG_PRESSED;
            }
            break;
        case RELEASED:
            if (M5.BtnA.isPressed() && (millis() - buttonReleaseTimeA) < 300) {
                stateA = DOUBLE_PRESSED;
            } else if ((millis() - buttonReleaseTimeA) >= 300) {
                SendMessage(deviceID, "_ButtonA");
                stateA = IDLE;
            }
            break;
        case DOUBLE_PRESSED:
            if (!M5.BtnA.isPressed()) {
                SendMessage(deviceID, "_ButtonA_Double");
                stateA = IDLE;
            }
            break;
        case LONG_PRESSED:
            if (!M5.BtnA.isPressed()) {
                stateA = IDLE;
            }
            break;
    }

    // --- Button B state machine ---
    switch (stateB) {
        case IDLE:
            if (M5.BtnB.isPressed()) {
                stateB = PRESSED;
                buttonPressTimeB = millis();
            }
            break;
        case PRESSED:
            if (!M5.BtnB.isPressed()) {
                stateB = RELEASED;
                buttonReleaseTimeB = millis();
            } else if ((millis() - buttonPressTimeB) > 1000) {
                SendMessage(deviceID, "_ButtonB_Long");
                stateB = LONG_PRESSED;
            }
            break;
        case RELEASED:
            if (M5.BtnB.isPressed() && (millis() - buttonReleaseTimeB) < 300) {
                stateB = DOUBLE_PRESSED;
            } else if ((millis() - buttonReleaseTimeB) >= 300) {
                SendMessage(deviceID, "_ButtonB");
                stateB = IDLE;
            }
            break;
        case DOUBLE_PRESSED:
            if (!M5.BtnB.isPressed()) {
                SendMessage(deviceID, "_ButtonB_Double");
                stateB = IDLE;
            }
            break;
        case LONG_PRESSED:
            if (!M5.BtnB.isPressed()) {
                stateB = IDLE;
            }
            break;
    }

    // --- External button (BUTTON_PIN) ---
    int buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW) {
         PushedExternalButton("_ButtonG26");
    }

    // --- Second external button (SECOND_BUTTON_PIN) ---
    buttonState = digitalRead(SECOND_BUTTON_PIN);
    if (secondButtonLastState == HIGH && buttonState == LOW) {
        SendMessage(deviceID, "_ButtonG32");
    }
    secondButtonLastState = buttonState;

    // --- Run KeepAliveChecker every 10 minutes ---
    if (millis() - lastKeepAliveCheck >= KEEP_ALIVE_INTERVAL) {
        lastKeepAliveCheck = millis();
        //if wifi disconnected, reconnect

        ConnectToWifi(false);
        
        //KeepAliveChecker();
    }
    
    // --- Screen timeout and optional sleep ---
    if ((millis() - lastButtonPress) > screenTimeout) {
        TurnScreenOff();
        if (bUseSleepMode) {
            EnterLightSleepUntilAButtonIsPressed();
        }
    }
}
