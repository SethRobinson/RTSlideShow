#include <Arduino.h>
#include <M5StickCPlus2.h>
#include <WiFi.h>

// Device and pin definitions
const uint16_t deviceID = 2;
#define FIRST_BUTTON_PIN 26    //A    
#define SECOND_BUTTON_PIN 32   //B
#define THIRD_BUTTON_PIN 33    //C
#define FOURTH_BUTTON_PIN -1   //D (disabled due to pin 25/36 interference)
#define FIFTH_BUTTON_PIN 0     //E
const uint16_t port  = 8095;    // Port to communicate with the server

// WiFi and host settings
const char* ssid     = "C2 Private";
const char* password = "(password)";
const char* host     = "192.168.1.5";
const char* secondary_host = "192.168.1.75";
const char* third_host = "192.168.1.72";

String activeHost;

// Timers and other globals
unsigned long lastButtonPress = 0;
unsigned long screenTimeout = 10000; // Screen timeout in ms
bool bUseSleepMode = false;          // Optionally enable sleep mode

// KeepAlive variables
unsigned long lastKeepAliveCheck = 0;
const unsigned long KEEP_ALIVE_INTERVAL = 10UL * 60 * 1000; // 10 minutes

// RTC time
m5::rtc_datetime_t StartTime;

// Button state machine definitions
enum ButtonState {
  IDLE,
  PRESSED,
  LONG_PRESSED,
};

ButtonState stateA = IDLE;
uint32_t buttonPressTimeA;

ButtonState stateB = IDLE;
uint32_t buttonPressTimeB;

ButtonState stateC = IDLE;
uint32_t buttonPressTimeC;

ButtonState stateD = IDLE;
uint32_t buttonPressTimeD;

ButtonState stateE = IDLE;
uint32_t buttonPressTimeE;

// Global persistent WiFiClient
WiFiClient client;

void ShowTimeElapsed();
void ConnectToWifi(bool bForceDisconnect);
void ClearScreen();
void KeepAliveChecker();
void SendMessage(int myDeviceID, String msg);

void SetStartTime() {
    StartTime = M5.Rtc.getDateTime();
}

void ShowTimeElapsed() {
    m5::rtc_datetime_t CurrentTime = M5.Rtc.getDateTime();

    int elapsedHours = CurrentTime.time.hours - StartTime.time.hours;
    int elapsedMinutes = CurrentTime.time.minutes - StartTime.time.minutes;
    int elapsedSeconds = CurrentTime.time.seconds - StartTime.time.seconds;

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
  // M5StickCPlus2 uses M5.Power API instead of M5.Axp
  return M5.Power.getBatteryLevel();
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
  M5.begin();
  M5.Lcd.setRotation(3);
  TurnScreenOn();
  M5.Lcd.setTextSize(2);
  SetStartTime();
  ShowBatteryLevel();
  ShowTimeElapsed();
  ConnectToWifi(false);

  if (FIRST_BUTTON_PIN >= 0) pinMode(FIRST_BUTTON_PIN, INPUT_PULLUP);
  if (SECOND_BUTTON_PIN >= 0) pinMode(SECOND_BUTTON_PIN, INPUT_PULLUP);
  if (THIRD_BUTTON_PIN >= 0) pinMode(THIRD_BUTTON_PIN, INPUT_PULLUP);
  if (FOURTH_BUTTON_PIN >= 0) pinMode(FOURTH_BUTTON_PIN, INPUT_PULLUP);
  if (FIFTH_BUTTON_PIN >= 0) pinMode(FIFTH_BUTTON_PIN, INPUT_PULLUP);

  // Pin 36 configuration removed - Button D is disabled due to pin 25/36 interference issues
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
  
  // Send message with controlled line ending
  client.print(message);
  client.print("\n");
  client.flush();      // Ensure message is sent immediately
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

// New keep-alive function to check and reconnect if needed.
void KeepAliveChecker() {
    if (!client.connected()) {
        M5.Lcd.println("KeepAlive: connection lost. Reconnecting...");
        SendMessage(deviceID, "_Dummy");
    } else {
        M5.Lcd.println("KeepAlive: connection is healthy.");
    }
}

void handleButton(ButtonState& state, uint32_t& pressTime, bool isPressed, const String& buttonName) {
    switch (state) {
        case IDLE:
            if (isPressed) {
                state = PRESSED;
                pressTime = millis();
            }
            break;
        case PRESSED:
            if ((millis() - pressTime) > 1000) {
                SendMessage(deviceID, "_Button" + buttonName + "_Long");
                state = LONG_PRESSED;
            } else if (!isPressed) {
                // Released before long press duration
                SendMessage(deviceID, "_Button" + buttonName);
                state = IDLE;
            }
            break;
        case LONG_PRESSED:
            if (!isPressed) {
                state = IDLE;
            }
            break;
    }
}

void loop() {
    M5.update(); // Update button states

    if (FIRST_BUTTON_PIN >= 0) {
        handleButton(stateA, buttonPressTimeA, digitalRead(FIRST_BUTTON_PIN) == LOW, "A");
    }
    if (SECOND_BUTTON_PIN >= 0) {
        handleButton(stateB, buttonPressTimeB, digitalRead(SECOND_BUTTON_PIN) == LOW, "B");
    }
    if (THIRD_BUTTON_PIN >= 0) {
        handleButton(stateC, buttonPressTimeC, digitalRead(THIRD_BUTTON_PIN) == LOW, "C");
    }
    if (FOURTH_BUTTON_PIN >= 0) {
        handleButton(stateD, buttonPressTimeD, digitalRead(FOURTH_BUTTON_PIN) == LOW, "D");
    }
    if (FIFTH_BUTTON_PIN >= 0) {
        handleButton(stateE, buttonPressTimeE, digitalRead(FIFTH_BUTTON_PIN) == LOW, "E");
    }
    
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
