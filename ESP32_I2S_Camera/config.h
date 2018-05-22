#pragma once

/**
 * FAQ device configuration
 */

 #define FAQ_VERSION "0.01"
 #define FAQ_BUILD "20180523"

#define EEPROM_SIZE 2048

// define structure keys
#define CONF_KEY_WIFISSID   "wifiSSID"
#define CONF_KEY_WIFIPASS   "wifiPass"
#define CONF_KEY_SERVERURL  "serverURL"

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

// HTML form code
// Note: this is a long and complex string so we will store it in PROGMEM
const char * config_form_HTML = "<h2>Device Configurator</h2><p>Firmware version: _FW_VERSION</p><form action=\"/writeConfig\" method=\"POST\"> <label class=\"label\">Network Name:</label> <input type=\"text\" name=\"wifiSSID\" value=\"_WIFISSID\" size=\"24\" maxlength=\"32\"/> <br/> <label>Password:</label> <input type=\"text\" name=\"wifiPass\" value=\"_WIFIPASS\" size=\"24\" maxlength=\"64\"/> <br/> <label>Server URL:</label> <input type=\"text\" name=\"serverURL\" value=\"_SERVERURL\" size=\"56\" maxlength=\"1024\"/> <br/> <input type=\"submit\" value=\"Submit\" /></form>";

// get version
String get_fw_version() {

  return String(FAQ_VERSION) + String(" ") + String(FAQ_BUILD);
}

