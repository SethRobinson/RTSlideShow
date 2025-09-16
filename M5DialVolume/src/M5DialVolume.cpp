#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

// This code is written to work with the M5Dial, it allows volume control via wifi on the RTSlideShow app
// https://docs.m5stack.com/en/core/M5Dial

// Device and network settings
const uint16_t deviceID = 3;
const uint16_t port = 8095;
const char* ssid = "C2 Private";
const char* password = "(password)";
const char* host = "192.168.1.5";
const char* secondary_host = "192.168.1.75";
const char* third_host = "192.168.1.72";

// Volume control variables
int currentVolume = 70; 
int lastVolume = 50;
unsigned long lastVolumeChangeTime = 0;
const unsigned long VOLUME_SEND_DELAY = 50;  // 100ms delay before sending
bool volumeNeedsSending = false;
const int VOLUME_SENSITIVITY = 2;  // Volume change per encoder notch (in %)

// Audio feedback settings
const bool ENABLE_ENCODER_CLICK_SOUND = false;  // Set to true to enable clicking sounds when rotating encoder

// OLED screen brightness control
const int OLED_BRIGHTNESS = 20;  // Dim brightness (0-255, where 50 is dim)

// Encoder variables
volatile int32_t encoderPosition = 0;
int32_t oldPosition = 0;
uint8_t encoderPinALast = LOW;
uint8_t encoderPinA = 40;
uint8_t encoderPinB = 41;

// Connection variables
String activeHost;
WiFiClient client;

// Timers
unsigned long lastKeepAliveCheck = 0;
const unsigned long KEEP_ALIVE_INTERVAL = 10UL * 60 * 1000;  // 10 minutes

// Function declarations
void ConnectToWifi(bool bForceDisconnect);
void SendMessage(int myDeviceID, String msg);
void updateDisplay();
void playClickSound(bool isIncreasing);
void IRAM_ATTR encoderISR();

// Interrupt service routine for encoder
void IRAM_ATTR encoderISR() {
    uint8_t MSB = digitalRead(encoderPinA); // MSB = most significant bit
    uint8_t LSB = digitalRead(encoderPinB); // LSB = least significant bit

    uint8_t encoded = (MSB << 1) | LSB; // Converting the 2 pin value to single number
    static uint8_t lastEncoded = 0;
    uint8_t sum = (lastEncoded << 2) | encoded; // Adding it to the previous encoded value

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
        encoderPosition++;
    }
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
        encoderPosition--;
    }
    
    lastEncoded = encoded; // Store this value for next time
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("C2 Kyoto Controls");
    
    // Initialize M5 with proper configuration
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // Set up PWM for backlight control on GPIO9
    ledcSetup(0, 5000, 8);  // Channel 0, 5kHz frequency, 8-bit resolution (0-255)
    ledcAttachPin(9, 0);     // Attach GPIO9 to channel 0
    ledcWrite(0, OLED_BRIGHTNESS);  // Set backlight brightness
    Serial.print("Backlight brightness set to: ");
    Serial.println(OLED_BRIGHTNESS);
    
    // Initialize display
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setTextSize(2);
    
    // Show boot message
    M5.Lcd.setCursor(40, 60);
    M5.Lcd.println("C2 Kyoto");
    M5.Lcd.setCursor(60, 100);
    M5.Lcd.println("Controls");
    M5.Lcd.setCursor(90, 140);
    M5.Lcd.println("v1.0");
    
    // Initialize speaker for click sounds
    M5.Speaker.begin();
    M5.Speaker.setVolume(128);
    M5.Speaker.tone(1000, 200);  // Startup beep
    delay(2000);
    
    // Initialize encoder pins with interrupts
    pinMode(encoderPinA, INPUT_PULLUP);
    pinMode(encoderPinB, INPUT_PULLUP);
    
    // Read initial state
    encoderPinALast = digitalRead(encoderPinA);
    
    // Attach interrupts on both pins for best responsiveness
    attachInterrupt(digitalPinToInterrupt(encoderPinA), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPinB), encoderISR, CHANGE);
    
    encoderPosition = 0;
    oldPosition = 0;
    
    // Connect to WiFi with status display
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20, 100);
    M5.Lcd.println("Connecting WiFi...");
    ConnectToWifi(false);
    
    // Show connected status
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(40, 100);
    M5.Lcd.println("Connected!");
    M5.Lcd.setCursor(20, 140);
    M5.Lcd.print("Host: ");
    M5.Lcd.println(activeHost);
    delay(1500);
    
    // Show initial volume
    updateDisplay();
    
    Serial.println("Setup complete!");
}

void updateDisplay() {
    M5.Lcd.fillScreen(TFT_BLACK);
    
    // Draw volume number - larger and centered
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(6);
    
    // Calculate position to center the number
    String volStr = String(currentVolume);
    int textWidth = volStr.length() * 36;  // Approximate width per character at size 6
    int xPos = (240 - textWidth) / 2;
    
    M5.Lcd.setCursor(xPos, 85);
    M5.Lcd.print(currentVolume);
    
    // Draw percentage symbol
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(170, 110);
    M5.Lcd.print("%");
    
    // Draw a circular progress indicator
    int radius = 110;
    int centerX = 120;  // Center of 240x240 display
    int centerY = 120;
    float angle = (currentVolume / 100.0) * 360.0 - 90.0;  // Start from top
    
    // Draw background circle
    M5.Lcd.drawCircle(centerX, centerY, radius, TFT_DARKGREY);
    
    // Draw progress arc (simplified - draws dots along the arc)
    for (int i = -90; i < angle; i += 5) {
        float rad = i * PI / 180.0;
        int x = centerX + radius * cos(rad);
        int y = centerY + radius * sin(rad);
        M5.Lcd.fillCircle(x, y, 3, TFT_GREEN);
    }
    
    // Show connection status at bottom
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(5, 225);
    if (client.connected()) {
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.print("Connected: ");
        M5.Lcd.print(activeHost);
    } else {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.print("Disconnected");
    }
    
    Serial.print("Display updated - Volume: ");
    Serial.println(currentVolume);
}

void playClickSound(bool isIncreasing) {
    if (M5.Speaker.isEnabled()) {
        if (isIncreasing) {
            M5.Speaker.tone(2000, 50);  // Higher pitch for increase
        } else {
            M5.Speaker.tone(1000, 50);  // Lower pitch for decrease
        }
    }
}

void ConnectToWifi(bool bForceDisconnect) {
    // Ensure WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("Connecting to WiFi");
        WiFi.begin(ssid, password);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            M5.Lcd.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected!");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nWiFi connection failed!");
            M5.Lcd.println("\nFailed!");
            return;
        }
    }
    
    // Handle client connection
    if (client.connected()) {
        if (bForceDisconnect) {
            client.stop();
        } else {
            return;
        }
    }
    
    if (!client.connected()) {
        activeHost = host;
        Serial.print("Connecting to primary host: ");
        Serial.println(activeHost);
        
        if (!client.connect(activeHost.c_str(), port)) {
            Serial.println("Failed, trying secondary host");
            client.stop();
            activeHost = secondary_host;
            
            if (!client.connect(secondary_host, port)) {
                Serial.println("Failed, trying third host");
                client.stop();
                activeHost = third_host;
                
                if (!client.connect(third_host, port)) {
                    Serial.println("All hosts failed");
                    activeHost = "None";
                    return;
                }
            }
        }
        
        Serial.println("Connected to host!");
        SendMessage(deviceID, "_Dummy");
    }
}

void SendMessage(int myDeviceID, String msg) {
    String message = "ID" + String(myDeviceID) + msg;
    Serial.println("Sending: " + message);
    
    if (!client.connected()) {
        Serial.println("Connection lost, reconnecting...");
        ConnectToWifi(true);
        if (!client.connected()) {
            Serial.println("Failed to reconnect");
            updateDisplay();  // Update display to show disconnected status
            return;
        }
    }
    
    client.print(message);
    client.print("\n");
    client.flush();
    Serial.println("Sent.");
}

void handleEncoder() {
    // Check if encoder position has changed (using interrupt-driven value)
    if (encoderPosition != oldPosition) {
        // Calculate the change
        int32_t diff = encoderPosition - oldPosition;
        oldPosition = encoderPosition;
        
        // Store old volume to check if it actually changed
        int oldVolume = currentVolume;
        bool encoderMoved = false;
        
        // Update volume based on encoder movement
        // Note: On M5Dial, clockwise typically decreases, counter-clockwise increases
        if (diff > 0) {
            // Counter-clockwise rotation - increases volume
            encoderMoved = true;
            currentVolume = min(100, currentVolume + (VOLUME_SENSITIVITY * abs(diff)));
            if (ENABLE_ENCODER_CLICK_SOUND) {
                playClickSound(true);
            }
        } else if (diff < 0) {
            // Clockwise rotation - decreases volume
            encoderMoved = true;
            currentVolume = max(0, currentVolume - (VOLUME_SENSITIVITY * abs(diff)));
            if (ENABLE_ENCODER_CLICK_SOUND) {
                playClickSound(false);
            }
        }
        
        // Always update display and send volume when encoder moves
        if (encoderMoved) {
            updateDisplay();
            lastVolumeChangeTime = millis();
            volumeNeedsSending = true;
        }
    }
}

void handleButtons() {
    // Handle physical button (encoder button)
    if (M5.BtnA.wasPressed()) {
        Serial.println("Physical button pressed");
        SendMessage(deviceID, "_ButtonA");
        // Flash screen briefly for feedback
        M5.Lcd.fillScreen(TFT_BLUE);
        delay(100);
        updateDisplay();
    }
    
    // Handle touch
    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) {
        Serial.print("Touch detected at: ");
        Serial.print(t.x);
        Serial.print(", ");
        Serial.println(t.y);
        SendMessage(deviceID, "_ButtonTouch");
        // Show touch feedback
        M5.Lcd.fillCircle(t.x, t.y, 20, TFT_YELLOW);
        delay(100);
        updateDisplay();
    }
}

void loop() {
    static unsigned long lastDebugPrint = 0;
    
    // Update M5 state
    M5.update();
    
    // Print debug info every 10 seconds
    if (millis() - lastDebugPrint > 10000) {
        lastDebugPrint = millis();
        Serial.println("=== Status Update ===");
        Serial.print("WiFi Connected: ");
        Serial.println(WiFi.status() == WL_CONNECTED ? "Yes" : "No");
        Serial.print("Server Connected: ");
        Serial.println(client.connected() ? "Yes" : "No");
        Serial.print("Current Volume: ");
        Serial.println(currentVolume);
        Serial.print("Active Host: ");
        Serial.println(activeHost);
        Serial.println("====================");
    }
    
    // Handle encoder rotation
    handleEncoder();
    
    // Handle buttons
    handleButtons();
    
    // Send volume update after delay
    if (volumeNeedsSending && (millis() - lastVolumeChangeTime >= VOLUME_SEND_DELAY)) {
        String volumeMsg = "_Volume|" + String(currentVolume) + "|";
        SendMessage(deviceID, volumeMsg);
        lastVolume = currentVolume;
        volumeNeedsSending = false;
    }
    
    // Keep-alive check
    if (millis() - lastKeepAliveCheck >= KEEP_ALIVE_INTERVAL) {
        lastKeepAliveCheck = millis();
        Serial.println("Keep-alive check...");
        ConnectToWifi(false);
        if (client.connected()) {
            SendMessage(deviceID, "_KeepAlive");
        }
        updateDisplay();  // Refresh connection status
    }
}