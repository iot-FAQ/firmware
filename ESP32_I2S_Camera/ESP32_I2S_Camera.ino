
#include "OV7670.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "BMP.h"
#include "esp_deep_sleep.h"
#include "EEPROM.h"

#include "config.h"

const int SIOD = 21; //SDA
const int SIOC = 22; //SCL

const int VSYNC = 34;
const int HREF = 35;

const int XCLK = 32;
const int PCLK = 33;

const int D0 = 27;
const int D1 = 17;
const int D2 = 16;
const int D3 = 15;
const int D4 = 14;
const int D5 = 13;
const int D6 = 12;
const int D7 = 4;

FAQ_DEVICE_CONFIG stored_device_config;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

OV7670 *camera;

WiFiMulti wifiMulti;
WiFiServer server(80);

unsigned char bmpHeader[BMP::headerSize];
int addr = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  /** 
   *  This is here for testing purposes only. There has to be a separate FW to manage the settings in EEPROM 
   *  Uncomment this line to FLASH the memory once. That's enough.
   *  Configuration is stored in file "config.h"
  **/
  /*** BEGIN ***/
  // write configuration to eeprom
  write_config_2_eeprom();
  /*** END ***/

  // read configration from EEPROM
  loadStruct(&stored_device_config, sizeof(stored_device_config));

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  esp_deep_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");

  Serial.println("Using SSID and PASS from flash: ");
  Serial.println(String(stored_device_config.wifiSSID));
  Serial.println(String(stored_device_config.wifiPass));
  wifiMulti.addAP(stored_device_config.wifiSSID, stored_device_config.wifiPass);

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  camera = new OV7670(OV7670::Mode::QQVGA_RGB565, SIOD, SIOC, VSYNC, HREF, XCLK, PCLK, D0, D1, D2, D3, D4, D5, D6, D7);
  BMP::construct16BitHeader(bmpHeader, camera->xres, camera->yres);
  Serial.println("camera  connected");
  camera->oneFrame();
  send_image_to_server();

  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}


/*** WORKING WITH EEPROM ***/
// Store any struct
void storeStruct(void *data_source, size_t size)
{
  EEPROM.begin(size * 2);
  for(size_t i = 0; i < size; i++)
  {
    char data = ((char *)data_source)[i];
    EEPROM.write(i, data);
  }
  EEPROM.commit();
}

// load any struct
void loadStruct(void *data_dest, size_t size)
{
    EEPROM.begin(size * 2);
    for(size_t i = 0; i < size; i++)
    {
        char data = EEPROM.read(i);
        ((char *)data_dest)[i] = data;
    }
}

// write config to EEPROM
void write_config_2_eeprom() {

  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

  Serial.println("Flashing " + String(sizeof(device_config)) + " bytes to EEPROM");

  storeStruct(&device_config, sizeof(device_config));

  
}


/*** Sending image to the server ***/
void send_image_to_server()
{
  HTTPClient http;
  uint8_t *payload = NULL;
  int bmp_size = (camera->xres * camera->yres * 2) + BMP::headerSize;
  payload = (uint8_t*)malloc(bmp_size);
  
  http.begin(stored_device_config.serverURL); //HTTP

  for (int i = 0; i < BMP::headerSize; i++) {
    payload[i] = bmpHeader[i];
  }
  for (int i = BMP::headerSize, j = 0; j < camera->xres * camera->yres * 2; i++, j++) {
    payload[i] = camera->frame[j];
  }

  int httpCode = http.POST(payload, bmp_size);
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  free(payload);
}

/** loop is empty since we work in push mode **/
void loop()
{
}
