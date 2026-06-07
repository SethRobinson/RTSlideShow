#include <Arduino.h>
#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <lwip/sockets.h> //for setsockopt() TCP keepalive on the raw socket fd
#include "secrets.h"      //WiFi/server credentials - copy secrets.example.h to secrets.h

// Device and pin definitions
const uint16_t deviceID = 2;
#define FIRST_BUTTON_PIN 26    //A    
#define SECOND_BUTTON_PIN 32   //B
#define THIRD_BUTTON_PIN 33    //C
#define FOURTH_BUTTON_PIN -1   //D (disabled due to pin 25/36 interference)
#define FIFTH_BUTTON_PIN 0     //E
const uint16_t port  = 8095;    // Port to communicate with the server
// DHCP hostname so this unit is easy to find in the Deco app (instead of "espressif"/"Unknown").
#define DEVICE_HOSTNAME "RTSlideShow-Buttons"

// WiFi and host settings (values come from secrets.h)
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* host     = SERVER_HOST;
const char* secondary_host = SERVER_HOST_2;
const char* third_host = SERVER_HOST_3;

String activeHost;

// Timers and other globals
unsigned long lastButtonPress = 0;
unsigned long screenTimeout = 10000; // Screen timeout in ms
bool bUseSleepMode = false;          // Optionally enable sleep mode

// Heartbeat / liveness variables
// Instead of a 10-minute "keep alive" that never actually sent anything (and so could never
// notice a half-open link), we send a tiny ping every few seconds.  Combined with TCP
// keepalive on the socket, this makes a dropped connection show up within seconds.
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 3000;  // send a ping every 3s when otherwise idle
const uint32_t CONNECT_TIMEOUT_MS = 3000;       // per-host connect timeout (initial setup); the
                                                // first connect right after WiFi association can
                                                // take >1s on a busy AP, so keep this generous
const uint32_t RECONNECT_TIMEOUT_MS = 600;      // bounded reconnect timeout (hot path)

// Require two consecutive missed heartbeats before reconnecting, so a single transient
// hiccup doesn't tear down and re-open the socket (which orphans a connection on the server).
int heartbeatMisses = 0;

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
uint32_t lastStateChangeTimeA = 0;

ButtonState stateB = IDLE;
uint32_t buttonPressTimeB;
uint32_t lastStateChangeTimeB = 0;

ButtonState stateC = IDLE;
uint32_t buttonPressTimeC;
uint32_t lastStateChangeTimeC = 0;

ButtonState stateD = IDLE;
uint32_t buttonPressTimeD;
uint32_t lastStateChangeTimeD = 0;

ButtonState stateE = IDLE;
uint32_t buttonPressTimeE;
uint32_t lastStateChangeTimeE = 0;

// Global persistent WiFiClient
WiFiClient client;

void ShowTimeElapsed();
void ConnectToWifi(bool bForceDisconnect);
void ClearScreen();
void SendMessage(int myDeviceID, String msg);
bool SendRaw(const String& message);
void EnableTcpKeepAlive();
bool TryConnectHost(const char* h);
bool ReconnectPreferred();
void StartWifiJoin();
bool ParseBssid(const char* s, uint8_t out[6]);
String WifiNodeTag();
void ShowIdentity();

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

// Turn on TCP keepalive for the live socket so the OS detects a dropped peer (we roamed
// away, AP rebooted, server died) within a few seconds instead of stalling until the much
// longer TCP retransmit timeout.  After this, client.connected() reliably goes false on a
// dead link.
void EnableTcpKeepAlive() {
  int fd = client.fd();
  if (fd < 0) return;

  int enable = 1;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

  int idle = 5;    // begin probing after 5s of silence
  int interval = 2; // probe every 2s
  int count = 3;    // give up (declare dead) after 3 failed probes
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
}

// Attempt one host with a bounded (non-blocking) timeout so a dead host can't freeze the
// whole device for many seconds.
bool TryConnectHost(const char* h) {
  client.stop();
  // Show which host we're attempting so the stick reports what it's actually doing - makes a
  // "No host found." far easier to diagnose (wrong IP? one host up, others down? all timing out?).
  M5.Lcd.print("Try ");
  M5.Lcd.print(h);
  if (client.connect(h, port, CONNECT_TIMEOUT_MS)) {
    activeHost = h;
    client.setNoDelay(true); // Disable Nagle's algorithm for low latency
    EnableTcpKeepAlive();
    M5.Lcd.println(" OK");
    return true;
  }
  M5.Lcd.println(" X");
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

// Short tag for the access point we're on (last byte of the BSSID) to show which mesh node.
String WifiNodeTag() {
  String b = WiFi.BSSIDstr();
  if (b.length() >= 2) return b.substring(b.length() - 2);
  return "??";
}

// Centralized WiFi association: friendly hostname, scan ALL channels and pick the STRONGEST
// node (default WIFI_FAST_SCAN grabs the first/possibly-far node), cap at WPA2 (dodge
// WiFi6E/WPA3/PMF handshake issues), and optionally HARD-LOCK to WIFI_BSSID if set in
// secrets.h (unset = automatic strongest-node, so the Deco-app-only approach works).
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
#endif

  if (haveBssid) {
    WiFi.begin(ssid, password, 0, bssid);
  } else {
    WiFi.begin(ssid, password);
  }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // max TX power for the best link
}

// Show this unit's identity (hostname/IP/MAC/RSSI/node) so it's easy to find in the Deco app.
void ShowIdentity() {
  ClearScreen();
  M5.Lcd.println(DEVICE_HOSTNAME);
  M5.Lcd.print("IP "); M5.Lcd.println(WiFi.localIP());
  M5.Lcd.print("MAC "); M5.Lcd.println(WiFi.macAddress());
  M5.Lcd.printf("%ddBm n:%s\n", WiFi.RSSI(), WifiNodeTag().c_str());
  Serial.printf("Hostname:%s IP:%s MAC:%s RSSI:%d BSSID:%s ch:%d\n",
                DEVICE_HOSTNAME, WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(),
                WiFi.RSSI(), WiFi.BSSIDstr().c_str(), WiFi.channel());
}

// Modified to use the global WiFiClient
void ConnectToWifi(bool bForceDisconnect) {
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) 
  {
      StartWifiJoin();
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          M5.Lcd.print(".");
      }
      M5.Lcd.print("IP: ");
      M5.Lcd.println(WiFi.localIP());
  }
  // Always disable WiFi power save for low latency (not just on the first connect) - modem
  // sleep is a common cause of laggy/intermittent responses.
  WiFi.setSleep(false);

  // If already connected via our client and not forcing a reconnect, nothing to do.
  if (client.connected() && !bForceDisconnect)
  {
      return;
  }

  if (bForceDisconnect)
  {
      client.stop();
  }

  // Build the candidate list with the last-known-good host FIRST.  At the cafe the dev box
  // (primary) is usually off, so always retrying it first wasted a full connect timeout on
  // every reconnect.  Remembering activeHost avoids that stall.
  // Use a stable local copy so the candidate pointer doesn't alias activeHost's own buffer
  // (which TryConnectHost reassigns).
  String preferred = activeHost;
  const char* candidates[3] = { host, secondary_host, third_host };
  if (preferred.length() > 0)
  {
      candidates[0] = preferred.c_str();
      int idx = 1;
      if (preferred != host)           candidates[idx++] = host;
      if (preferred != secondary_host) candidates[idx++] = secondary_host;
      if (preferred != third_host)     candidates[idx++] = third_host;
  }

  bool connected = false;
  for (int i = 0; i < 3 && !connected; i++)
  {
      if (candidates[i] == NULL) continue;
      connected = TryConnectHost(candidates[i]);
  }

  if (!connected)
  {
      M5.Lcd.println("No host found.");
      return;
  }

  SendMessage(deviceID, "_Dummy");
  M5.Lcd.print("Host: ");
  M5.Lcd.println(activeHost);
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
  Serial.begin(115200);
  ShowBatteryLevel();
  ShowTimeElapsed();
  ConnectToWifi(false);

  // Briefly show this unit's identity so it can be matched/pinned in the Deco app.
  if (WiFi.status() == WL_CONNECTED) {
    ShowIdentity();
    delay(2500);
  }

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

// Silent send used for heartbeats and the actual byte writes - never touches the screen so
// pings don't keep the display awake.  Returns false if the link looks dead.
bool SendRaw(const String& message)
{
  if (!client.connected()) return false;
  client.print(message);
  client.print("\n");
  // If the socket died (keepalive tripped, peer gone) this flips false right away.
  return client.connected();
}

// Bounded reconnect for the hot path: tries ONLY the last-known-good host with a short
// timeout, and never runs the blocking WiFi-join loop, so a button press can't freeze the
// device for seconds.  Returns true if reconnected.
bool ReconnectPreferred()
{
  if (WiFi.status() != WL_CONNECTED)
  {
      // Kick off (re)association without blocking; we'll retry on the next heartbeat.
      StartWifiJoin();
      return false;
  }
  WiFi.setSleep(false);
  client.stop();

  String h = (activeHost.length() > 0 && activeHost != "None") ? activeHost : String(host);
  if (client.connect(h.c_str(), port, RECONNECT_TIMEOUT_MS))
  {
      activeHost = h;
      client.setNoDelay(true);
      EnableTcpKeepAlive();
      SendRaw("ID" + String(deviceID) + "_Dummy");
      return true;
  }
  return false;
}

// User-facing send: push the bytes out (no blocking reconnect - the heartbeat handles that),
// then update the screen for feedback.  A button press never freezes the device.
void SendMessage(int myDeviceID, String msg) 
{
  lastButtonPress = millis();

  String message = "ID" + String(myDeviceID) + msg;

  bool sent = SendRaw(message);

  // Now the (comparatively slow) display update for user feedback.
  TurnScreenOn();
  ClearScreen();
  M5.Lcd.println(message);
  ShowTimeElapsed();
  if (!sent)
  {
      M5.Lcd.println("Not sent (reconnecting).");
  }
  else
  {
      M5.Lcd.println("Sent.");
  }
  // Note: We leave the connection open.
  ShowBatteryLevel();
  // Link quality (RSSI + which mesh node) so a bad/far node is visible during use.
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.printf("%ddBm n:%s\n", WiFi.RSSI(), WifiNodeTag().c_str());
  }
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

void handleButton(ButtonState& state, uint32_t& pressTime, uint32_t& lastStateChangeTime, bool isPressed, const String& buttonName) {
    // Debounce: ignore reads within 50ms of last state transition
    if (millis() - lastStateChangeTime < 50) return;

    switch (state) {
        case IDLE:
            if (isPressed) {
                state = PRESSED;
                pressTime = millis();
                lastStateChangeTime = millis();
            }
            break;
        case PRESSED:
            if ((millis() - pressTime) > 1000) {
                SendMessage(deviceID, "_Button" + buttonName + "_Long");
                state = LONG_PRESSED;
                lastStateChangeTime = millis();
            } else if (!isPressed) {
                // Released before long press duration
                SendMessage(deviceID, "_Button" + buttonName);
                state = IDLE;
                lastStateChangeTime = millis();
            }
            break;
        case LONG_PRESSED:
            if (!isPressed) {
                state = IDLE;
                lastStateChangeTime = millis();
            }
            break;
    }
}

void loop() {
    M5.update(); // Update button states

    if (FIRST_BUTTON_PIN >= 0) {
        handleButton(stateA, buttonPressTimeA, lastStateChangeTimeA, digitalRead(FIRST_BUTTON_PIN) == LOW, "A");
    }
    if (SECOND_BUTTON_PIN >= 0) {
        handleButton(stateB, buttonPressTimeB, lastStateChangeTimeB, digitalRead(SECOND_BUTTON_PIN) == LOW, "B");
    }
    if (THIRD_BUTTON_PIN >= 0) {
        handleButton(stateC, buttonPressTimeC, lastStateChangeTimeC, digitalRead(THIRD_BUTTON_PIN) == LOW, "C");
    }
    if (FOURTH_BUTTON_PIN >= 0) {
        handleButton(stateD, buttonPressTimeD, lastStateChangeTimeD, digitalRead(FOURTH_BUTTON_PIN) == LOW, "D");
    }
    if (FIFTH_BUTTON_PIN >= 0) {
        handleButton(stateE, buttonPressTimeE, lastStateChangeTimeE, digitalRead(FIFTH_BUTTON_PIN) == LOW, "E");
    }
    
    // Drain anything the server sent (e.g. heartbeat acks) so the RX buffer doesn't fill.
    while (client.available()) {
        client.read();
    }

    // --- Heartbeat: ping every few seconds.  Only after two consecutive misses do we
    // reconnect (bounded, non-blocking), to avoid orphaning the socket on a single transient.
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
            }
        }
    }
    
    // --- Screen timeout and optional sleep ---
    if ((millis() - lastButtonPress) > screenTimeout) {
        TurnScreenOff();
        if (bUseSleepMode) {
            EnterLightSleepUntilAButtonIsPressed();
        }
    }
}
