#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <lwip/sockets.h> //for setsockopt() TCP keepalive on the raw socket fd
#include "secrets.h"      //WiFi/server credentials - copy secrets.example.h to secrets.h

// This code is written to work with the M5Dial, it allows volume control via wifi on the RTSlideShow app
// https://docs.m5stack.com/en/core/M5Dial

// Device and network settings
const uint16_t deviceID = 3;
const uint16_t port = 8095;
// DHCP hostname so this unit is easy to find in the Deco app (instead of "espressif"/"Unknown").
#define DEVICE_HOSTNAME "RTSlideShow-VolumeDial"
// WiFi and host settings (values come from secrets.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* host = SERVER_HOST;
const char* secondary_host = SERVER_HOST_2;
const char* third_host = SERVER_HOST_3;

// Volume control variables
int currentVolume = 70; 
int lastVolume = 50;
unsigned long lastVolumeChangeTime = 0;
const unsigned long VOLUME_SEND_DELAY = 50;  // ms of stillness that counts as "settled"
bool volumeNeedsSending = false;
const int VOLUME_SENSITIVITY = 2;  // Volume change per encoder notch (in %)

// Live-send throttle: while the knob is being turned we send the latest value at most this
// often (instead of only after it settles), so the volume tracks the knob in real time.
unsigned long lastVolumeSentTime = 0;
const unsigned long VOLUME_LIVE_SEND_INTERVAL = 90;

// Display throttle: redrawing the whole 240x240 screen on every notch dominates the loop
// during a fast spin.  We just mark the screen dirty and repaint at most ~25fps.
bool displayDirty = false;
unsigned long lastDisplayDraw = 0;
const unsigned long DISPLAY_MIN_INTERVAL = 40;

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

// Heartbeat / liveness timers
// A tiny ping every few seconds (plus TCP keepalive on the socket) lets a dropped link be
// detected in seconds instead of stalling until the long TCP retransmit timeout.
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 3000;  // ping every 3s when otherwise idle
const uint32_t CONNECT_TIMEOUT_MS = 3000;       // per-host connect timeout (initial setup)
const uint32_t RECONNECT_TIMEOUT_MS = 600;      // bounded reconnect timeout (hot path)

// Require two consecutive missed heartbeats before tearing down and reconnecting, so a
// single transient hiccup doesn't orphan the socket (which the server then has to cull).
int heartbeatMisses = 0;

// Function declarations
void ConnectToWifi(bool bForceDisconnect);
void SendMessage(int myDeviceID, String msg);
bool SendRaw(const String& message);
void EnableTcpKeepAlive();
bool TryConnectHost(const char* h);
bool ReconnectPreferred();
void StartWifiJoin();
bool ParseBssid(const char* s, uint8_t out[6]);
String WifiNodeTag();
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
    
    // Show connected status + this unit's identity (so it's easy to match in the Deco app).
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(40, 30);
    M5.Lcd.println("Connected!");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 75);
    M5.Lcd.print("Name: "); M5.Lcd.println(DEVICE_HOSTNAME);
    M5.Lcd.setCursor(10, 95);
    M5.Lcd.print("IP:   "); M5.Lcd.println(WiFi.localIP());
    M5.Lcd.setCursor(10, 115);
    M5.Lcd.print("MAC:  "); M5.Lcd.println(WiFi.macAddress());
    M5.Lcd.setCursor(10, 135);
    M5.Lcd.printf("RSSI: %d dBm  node:%s", WiFi.RSSI(), WifiNodeTag().c_str());
    M5.Lcd.setCursor(10, 155);
    M5.Lcd.print("Host: "); M5.Lcd.println(activeHost);
    delay(2500);
    
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
    
    // Show connection quality at bottom: WiFi RSSI + which mesh node + server state.
    // Red when the link is weak (worse than -70 dBm) or down, so a bad node is obvious.
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(5, 225);
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        uint16_t col = (rssi <= -70) ? TFT_RED : TFT_GREEN;
        M5.Lcd.setTextColor(col, TFT_BLACK);
        M5.Lcd.printf("%ddBm n:%s %s", rssi, WifiNodeTag().c_str(), client.connected() ? "srvOK" : "noSrv");
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

// Turn on TCP keepalive so the OS detects a dropped peer within seconds instead of stalling
// until the much longer TCP retransmit timeout.  After this, client.connected() reliably
// goes false on a dead link.
void EnableTcpKeepAlive() {
    int fd = client.fd();
    if (fd < 0) return;

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

    int idle = 5;     // begin probing after 5s of silence
    int interval = 2; // probe every 2s
    int count = 3;    // declare dead after 3 failed probes
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
}

// Attempt one host with a bounded (non-blocking) timeout so a dead host can't freeze the
// device for many seconds.
bool TryConnectHost(const char* h) {
    client.stop();
    Serial.print("Connecting to host: ");
    Serial.println(h);
    if (client.connect(h, port, CONNECT_TIMEOUT_MS)) {
        activeHost = h;
        client.setNoDelay(true); // Disable Nagle's algorithm for low latency
        EnableTcpKeepAlive();
        Serial.println("Connected to host!");
        return true;
    }
    return false;
}

// Parse "aa:bb:cc:dd:ee:ff" into 6 bytes.  Returns false if the string isn't a valid MAC.
bool ParseBssid(const char* s, uint8_t out[6]) {
    if (!s) return false;
    unsigned int b[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
        return true;
    }
    return false;
}

// Short tag for the access point we're on (last byte of the BSSID), so the screen can show
// which mesh node we're actually associated with.
String WifiNodeTag() {
    String b = WiFi.BSSIDstr();  // "aa:bb:cc:dd:ee:ff"
    if (b.length() >= 2) return b.substring(b.length() - 2);
    return "??";
}

// Centralized WiFi association so every (re)join behaves the same:
//  - set a friendly hostname (findable in the Deco app)
//  - scan ALL channels and pick the STRONGEST node (default WIFI_FAST_SCAN grabs the FIRST
//    AP found, which on a multi-node mesh is often a far/weak one - a primary cause of the
//    drops/lag)
//  - cap minimum security at WPA2 to dodge WiFi6E/WPA3/PMF handshake issues
//  - optionally HARD-LOCK to a specific node's BSSID if WIFI_BSSID is set in secrets.h
//    (left unset = automatic strongest-node selection, so the Deco-app-only approach works).
void StartWifiJoin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.setSleep(false);
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);

    uint8_t bssid[6];
    bool haveBssid = false;
#ifdef WIFI_BSSID
    haveBssid = ParseBssid(WIFI_BSSID, bssid);
    if (haveBssid) {
        Serial.printf("WiFi: locking to BSSID %s\n", WIFI_BSSID);
    } else {
        Serial.println("WiFi: WIFI_BSSID set but invalid - using auto (strongest node)");
    }
#endif

    if (haveBssid) {
        WiFi.begin(ssid, password, 0, bssid);
    } else {
        WiFi.begin(ssid, password);
    }
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // max TX power for the best link
}

void ConnectToWifi(bool bForceDisconnect) {
    // Ensure WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("Connecting to WiFi");
        StartWifiJoin();
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            M5.Lcd.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected!");
            Serial.printf("Hostname: %s\n", DEVICE_HOSTNAME);
            Serial.print("IP: ");   Serial.println(WiFi.localIP());
            Serial.print("MAC: ");  Serial.println(WiFi.macAddress());
            Serial.printf("RSSI: %d dBm  BSSID(node): %s  ch:%d\n",
                          WiFi.RSSI(), WiFi.BSSIDstr().c_str(), WiFi.channel());
        } else {
            Serial.println("\nWiFi connection failed!");
            M5.Lcd.println("\nFailed!");
            return;
        }
    }
    // Always disable WiFi power save for low latency (not just on the first connect) - modem
    // sleep is a common cause of laggy/intermittent responses.
    WiFi.setSleep(false);

    // If already connected and not forcing a reconnect, nothing to do.
    if (client.connected() && !bForceDisconnect) {
        return;
    }

    if (bForceDisconnect) {
        client.stop();
    }

    // Try the last-known-good host first.  At the cafe the dev box (primary) is usually off,
    // so always retrying it first wasted a full connect timeout on every reconnect.
    String preferred = activeHost;
    const char* candidates[3] = { host, secondary_host, third_host };
    if (preferred.length() > 0 && preferred != "None") {
        candidates[0] = preferred.c_str();
        int idx = 1;
        if (preferred != host)           candidates[idx++] = host;
        if (preferred != secondary_host) candidates[idx++] = secondary_host;
        if (preferred != third_host)     candidates[idx++] = third_host;
    }

    bool connected = false;
    for (int i = 0; i < 3 && !connected; i++) {
        if (candidates[i] == NULL) continue;
        connected = TryConnectHost(candidates[i]);
    }

    if (!connected) {
        Serial.println("All hosts failed");
        activeHost = "None";
        return;
    }

    SendMessage(deviceID, "_Dummy");
}

// Silent send used for heartbeats and the actual byte writes - never redraws the screen.
// Returns false if the link looks dead.
bool SendRaw(const String& message) {
    if (!client.connected()) return false;
    client.print(message);
    client.print("\n");
    return client.connected();
}

// Bounded reconnect for the hot path: tries ONLY the last-known-good host with a short
// timeout, and never runs the blocking WiFi-join loop, so it can't freeze the device for
// seconds while you're turning the knob.  Returns true if reconnected.
bool ReconnectPreferred() {
    if (WiFi.status() != WL_CONNECTED) {
        // Kick off (re)association without blocking; we'll retry on the next heartbeat.
        StartWifiJoin();
        return false;
    }
    WiFi.setSleep(false);
    client.stop();

    String h = (activeHost.length() > 0 && activeHost != "None") ? activeHost : String(host);
    if (client.connect(h.c_str(), port, RECONNECT_TIMEOUT_MS)) {
        activeHost = h;
        client.setNoDelay(true);
        EnableTcpKeepAlive();
        SendRaw("ID" + String(deviceID) + "_Dummy");
        Serial.println("Reconnected to " + h);
        return true;
    }
    return false;
}

// Non-blocking send: if the link is down we simply skip (the heartbeat handles reconnects).
// This keeps the volume/spin path from ever freezing the loop.
void SendMessage(int myDeviceID, String msg) {
    String message = "ID" + String(myDeviceID) + msg;
    Serial.println("Sending: " + message);

    if (!SendRaw(message)) {
        Serial.println("Skipped (not connected); heartbeat will reconnect.");
    }
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
        
        // Mark the display for a (throttled) repaint and flag the volume to be sent.  We do
        // NOT redraw here - the loop repaints at ~25fps so a fast spin stays responsive.
        if (encoderMoved) {
            displayDirty = true;
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
        Serial.printf("RSSI: %d dBm  BSSID(node): %s  ch:%d\n",
                      WiFi.RSSI(), WiFi.BSSIDstr().c_str(), WiFi.channel());
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
    
    // Send volume updates.  While the knob is turning we send the latest value live (at most
    // every VOLUME_LIVE_SEND_INTERVAL) so it tracks in real time, and we always send the
    // final value once it settles.  Duplicate values are skipped.
    if (volumeNeedsSending) {
        bool settled = (millis() - lastVolumeChangeTime >= VOLUME_SEND_DELAY);
        bool liveDue = (millis() - lastVolumeSentTime >= VOLUME_LIVE_SEND_INTERVAL);

        if ((settled || liveDue) && currentVolume != lastVolume) {
            String volumeMsg = "_Volume|" + String(currentVolume) + "|";
            SendMessage(deviceID, volumeMsg);
            lastVolume = currentVolume;
            lastVolumeSentTime = millis();
        }

        if (settled) {
            volumeNeedsSending = false;
        }
    }

    // Repaint the screen at most ~25fps when something changed (keeps fast spins smooth).
    if (displayDirty && (millis() - lastDisplayDraw >= DISPLAY_MIN_INTERVAL)) {
        lastDisplayDraw = millis();
        displayDirty = false;
        updateDisplay();
    }

    // Drain anything the server sent (e.g. heartbeat acks) so the RX buffer doesn't fill.
    while (client.available()) {
        client.read();
    }

    // Heartbeat: send a tiny ping every few seconds.  Only after two consecutive misses do
    // we reconnect (bounded, non-blocking) - this avoids tearing down the socket on a single
    // transient and prevents the reconnect churn that orphans connections on the server.
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = millis();
        bool alive = client.connected() && SendRaw("ID" + String(deviceID) + "_Ping");
        if (alive) {
            heartbeatMisses = 0;
        } else {
            heartbeatMisses++;
            if (heartbeatMisses >= 2) {
                if (ReconnectPreferred()) {
                    heartbeatMisses = 0;
                }
                displayDirty = true;  // refresh connection status
            }
        }
    }
}