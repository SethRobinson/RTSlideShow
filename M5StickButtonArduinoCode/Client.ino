//Arduino code for the M5StickCPlus to send button presses to a server
//Note that it detects short button, double click, and long clicks on both
//of its buttons

//It has power saving features such as turning off the screen after a timeout

#include <M5StickCPlus.h>

#include <WiFi.h>
#include "AXP192.h"

const uint16_t deviceID = 0; //ID of the device, ID is sent in the button push string to differentiate devices
#define MOSFET_GATE_PIN 25 //to control the light
#define BUTTON_PIN 26  //external button 1
#define SECOND_BUTTON_PIN 32 //external button 2
const uint16_t port  = 8095; //port we communicate with Seth's app on


const char* ssid     = "C2 Private";
const char* password = "<password>";
const char* host     = "192.168.1.4";
const char* secondary_host = "192.168.1.5"; //oldware
const char* third_host = "192.168.1.6"; //lavie

String activeHost;

unsigned long lastButtonPress = 0;
unsigned long screenTimeout = 10000; // 20 seconds in milliseconds
bool bUseSleepMode = false; //battery might last eons, but you have to push button A to wake up
unsigned long ExternalButtonFlashingLightTime = 10*1000;
unsigned long ExternalButtonTimePerFlash = 300;
int secondButtonLastState = HIGH;

RTC_TimeTypeDef StartTime; // this will hold the start time

void SetStartTime() 
{
    M5.Rtc.GetTime(&StartTime);
}

void ShowTimeElapsed() 
{
    RTC_TimeTypeDef CurrentTime;
    M5.Rtc.GetTime(&CurrentTime);

    int elapsedHours = CurrentTime.Hours - StartTime.Hours;
    int elapsedMinutes = CurrentTime.Minutes - StartTime.Minutes;
    int elapsedSeconds = CurrentTime.Seconds - StartTime.Seconds;

    // if seconds or minutes are negative, adjust accordingly
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


int getBatteryLevel()
{
  float voltage = M5.Axp.GetBatVoltage();
  
  if(voltage > 4.2) 
    return 100;
  else if(voltage < 3.3)
    return 0;
  else
    return (int)((voltage - 3.3) / (4.2 - 3.3) * 100);
}


void ShowBatteryLevel()
{
  M5.Lcd.print("Bat: ");
  M5.Lcd.print(getBatteryLevel());
  M5.Lcd.println("%");
}

void ConnectToWifi(bool bForceDisconnect)
{

  if (WiFi.status() == WL_CONNECTED)
  {
    if (bForceDisconnect)
    {
      WiFi.disconnect();
    } else
    {
      return; //assuming whatever we have is fine
    }
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    M5.Lcd.print(".");
  }
  
  M5.Lcd.print("IP: ");
  M5.Lcd.println(WiFi.localIP());

  activeHost = host;
  WiFiClient client;

  if (!client.connect(activeHost.c_str(), port)) 
    {
      ClearScreen();
      M5.Lcd.println("Failed, trying secondary");
      client.stop(); // Close the connection
     
      if (!client.connect(secondary_host, port)) 
      {
        ClearScreen();
        M5.Lcd.println("Failed, trying third");
        client.stop(); // Close the connection
     
      if (!client.connect(third_host, port)) 
      {
         M5.Lcd.println("third failed, no host");
        return;
      } else
      {
      activeHost = third_host;
      }
      
      } else
      {
      activeHost = secondary_host;
      }
      
    }

      M5.Lcd.println("Host found.");

}

//Credit for this code goes to DDA:  https://community.m5stack.com/topic/1025/m5stickc-turn-off-screen-completely/11
void TurnScreenOff()
{
  Wire1.beginTransmission(0x34);
  Wire1.write(0x12);
  Wire1.write(0b01001011);  // LDO2, aka OLED_VDD, off
  Wire1.endTransmission();
}

void TurnScreenOn()
{
  Wire1.beginTransmission(0x34);
  Wire1.write(0x12);
  Wire1.write(0x4d); // Enable LDO2, aka OLED_VDD
  Wire1.endTransmission();
}
//End dda

void setup() 
{

  lastButtonPress = millis();
  M5.begin(true, true, false); // Initialize the M5Stack with all features (Serial, Lcd, I2C)
  M5.Lcd.setRotation(3);
  TurnScreenOn();
  M5.Lcd.setTextSize(2);
  SetStartTime();
  ShowBatteryLevel();
  ShowTimeElapsed();
  ConnectToWifi(false);

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure the pin as an input with pull-up resistor
  pinMode(MOSFET_GATE_PIN, OUTPUT); // Configure GPIO25 as an output

  pinMode(SECOND_BUTTON_PIN, INPUT_PULLUP); // Configure the pin as an input with pull-up resistor
 

}

void ClearScreen()
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
}


void SendMessage(int myDeviceID, String msg)
{
  lastButtonPress = millis();
  M5.Beep.tone(4000);
  delay(40);
  M5.Beep.mute();

  TurnScreenOn();
  ClearScreen();
  String message = "ID" + String(myDeviceID) + msg;

  M5.Lcd.println(message);
  ShowTimeElapsed();

  WiFiClient client;

 if (!client.connect(activeHost.c_str(), port)) 
    {
      M5.Lcd.println("Connection failed, reconnecting");
      client.stop(); // Close the connection
      ConnectToWifi(true);
      M5.Lcd.println("Sending again...");
      if (!client.connect(activeHost.c_str(), port)) 
      {
         M5.Lcd.println("Failed.");
        return;
      }
      
    }

 
  client.print(message);

  M5.Lcd.println("Sent.");
  client.stop(); // Close the connection
  ShowBatteryLevel();
 
}

void EnterLightSleepUntilAButtonIsPressed() 
{
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0); // Enable wake-up on button A press
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // Enable wake-up on button B press
  esp_light_sleep_start();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  TurnScreenOn();
  ConnectToWifi(false);
  ShowBatteryLevel();
  
  delay(500);
  lastButtonPress = millis();
  
}

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

void PushedExternalButton(String buttonName)
{
digitalWrite(MOSFET_GATE_PIN, HIGH);  // Turn on
  
   M5.Beep.tone(4000);
   delay(40);
  M5.Beep.mute();

  SendMessage(deviceID, buttonName);
  
  unsigned long waitTimer =  millis()+ ExternalButtonFlashingLightTime;

  while(waitTimer > millis())
  {
    digitalWrite(MOSFET_GATE_PIN, HIGH);
    delay(ExternalButtonTimePerFlash);                         // Wait for 1 second
    digitalWrite(MOSFET_GATE_PIN, LOW);  // Turn off the 12V device
    delay(ExternalButtonTimePerFlash);  
  }

}


void loop() 
{
    M5.update(); // Update the buttons state

    // Button A
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

 int buttonState = digitalRead(BUTTON_PIN); // Read the state of the button (LOW if pressed, HIGH if not)

  if (buttonState == LOW)
   {
    //Serial.println("Button is pressed");
     PushedExternalButton("_ButtonG26");
   } 

   //second button (if it's wired)


   buttonState = digitalRead(SECOND_BUTTON_PIN); // Read the state of the button (LOW if pressed, HIGH if not)

   if (secondButtonLastState == HIGH)
   {
      if (buttonState == LOW)
      {
        //Serial.println("Button is pressed");
        SendMessage(deviceID, "_ButtonG32");
      } 
   }

    secondButtonLastState = buttonState;


    if ((millis() - lastButtonPress) > screenTimeout) 
    {
        TurnScreenOff();
        if (bUseSleepMode)
        {
            EnterLightSleepUntilAButtonIsPressed();
        }
    }
  
}

