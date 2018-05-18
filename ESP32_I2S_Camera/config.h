#pragma once

/**
 * FAQ device configuration
 */

#define EEPROM_SIZE 2048

// configuration structure
typedef struct {
  char wifiSSID[32]; /* longest SSID can be 32 bytes */
  char wifiPass[64]; /* longest WiFI passphrase can be 64 bytes */
  char serverURL[1024]; /* let's reserve 1024 bytes for URL; should be enough */
  
} FAQ_DEVICE_CONFIG __attribute__ ((packed));

// default config; adjust to your needs
FAQ_DEVICE_CONFIG device_config {
  "PVL-LAB", /* SSID */
  "iot-lab-2018-06", /* password */
  "http://192.168.1.150:8008/index.html" /* server URL */
  
};
