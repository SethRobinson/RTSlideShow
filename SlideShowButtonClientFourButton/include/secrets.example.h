#pragma once

// -----------------------------------------------------------------------------
// SETUP: Copy this file to "secrets.h" in this same folder, then fill in your
// real values below.  secrets.h is gitignored, so your WiFi password and IPs
// will NOT be committed to git.
//
//   1. Copy secrets.example.h  ->  secrets.h   (same include/ folder)
//   2. Edit secrets.h with your real SSID, password, and RTSlideShow PC IP(s)
//   3. Build / upload as usual
// -----------------------------------------------------------------------------

#define WIFI_SSID      "Your WiFi SSID"
#define WIFI_PASSWORD  "Your WiFi password"

// The PC(s) running RTSlideShow.  The device tries these in order (and remembers
// the last one that worked, trying it first next time).  If you only use one PC,
// just point all three at the same IP.
#define SERVER_HOST    "192.168.1.5"
#define SERVER_HOST_2  "192.168.1.75"
#define SERVER_HOST_3  "192.168.1.72"
