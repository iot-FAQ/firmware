
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

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

OV7670 *camera;

WiFiMulti wifiMulti;
WiFiServer server(80);

unsigned char bmpHeader[BMP::headerSize];
int addr = 0;
#define EEPROM_SIZE 64

void setup()
{
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

  //char * ssid = "Symbaa";
  int size = 7;
  //char * pass = "symba1986";
  int sizep = 10;
//  Serial.println("SSID:" + String(ssid));
//  Serial.println("PASS:" + String(pass));
//  for (int i = 0; i < size; i++) {
//    EEPROM.write(addr, ssid[i]);
//    addr++;
//  }
//  for (int i = 0; i < sizep; i++) {
//    EEPROM.write(addr, pass[i]);
//    addr++;
//  }
//  EEPROM.commit();

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  esp_deep_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");

  char ssid1 [size];
  char password1 [sizep];
  Serial.println("Reading SSID and PASS from flash: ");
  for (int i = 0; i < size; i++)
  {
    ssid1[i] = char(EEPROM.read(i));
  }
  Serial.println(String(ssid1));
  for (int i = size, j = 0; i < size + sizep, j < sizep; i++, j++)
  {
    password1[j] = char(EEPROM.read(i));
  }
  Serial.println(String(password1));
  wifiMulti.addAP(ssid1, password1);

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
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

void send_image_to_server()
{
  HTTPClient http;
  uint8_t *payload = NULL;
  int bmp_size = (camera->xres * camera->yres * 2) + BMP::headerSize;
  payload = (uint8_t*)malloc(bmp_size);
  http.begin("http://192.168.43.24/index.html"); //HTTP

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
void loop()
{
}
