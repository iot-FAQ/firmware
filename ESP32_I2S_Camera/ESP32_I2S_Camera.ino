#include "OV7670.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library

/* ESP32 WiFi Libraries */
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

/* Web Server */
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/* imaging and other processing */
#include "BMP.h"
#include "esp_deep_sleep.h"
#include "EEPROM.h"

/* default configuration */
#include "config.h"

/* CAMERA PINS */
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

/* DEVICE PINS */
// Let's use pin D26 as a configration pin
#define CONF_GPIO_PIN   GPIO_NUM_26

FAQ_DEVICE_CONFIG stored_device_config; // used to read the data from EEPROM
FAQ_DEVICE_CONFIG received_device_config; // used to capture the data from Web Configurator

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

OV7670 *camera;

WiFiMulti wifiMulti;
AsyncWebServer server(80);
String html_response;

unsigned char bmpHeader[BMP::headerSize];
int addr = 0;


/*
 * Read logical level on pin CONF_GPIO_PIN and return TRUE if level is high
 * which means that device is in the configuration mode
 */
bool isSetupModeEnabled() {

  gpio_set_direction(CONF_GPIO_PIN, GPIO_MODE_INPUT);

  if (gpio_get_level(CONF_GPIO_PIN)) return true;
  
  return false;
}

/*
 * Main routine
 */
void setup()
{
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  if (isSetupModeEnabled()) { // configuration
    Serial.println("Device configuration mode is ENABLED");

    // ... And here we do all the configuration

    // starting Software-controlled Wi-Fi access point
    char *conf_ssid = "FAQ-CONFIG";
    WiFi.mode( WIFI_AP );
    IPAddress ip( 192, 168, 1, 1 );
    IPAddress gateway( 192, 168, 1, 1 );
    IPAddress subnet( 255, 255, 255, 0 );
    WiFi.softAPConfig( ip, gateway, subnet );
    boolean result = WiFi.softAP(conf_ssid);
    if (!result) { // something went terribly wrong and we cannot continue; the system will halt
      Serial.println("ERROR: Unable to start configuration AP");
      return;
    }

    // print some useful information
    Serial.println("Started WiFi Access point for configuration purposes");
    Serial.println();
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    
    /* tell server to handle requests */
    Serial.println("DEBUG: Assigning WEB Server handlers ...");

    // handle values saving (POST) request
    server.on("/writeConfig", HTTP_POST, [](AsyncWebServerRequest *request){

      // get parameters count that were received from browser
      int params = request->params();
      // iterate over each parameter
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        
        if(p->isPost()){ // this is post parameter; that's fine
          Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          
          /* Parse params */
          // CONF_KEY_WIFISSID
          if (p->name().equals(CONF_KEY_WIFISSID)) {
            strcpy(received_device_config.wifiSSID,  p->value().c_str());
            continue;
          }
          // CONF_KEY_WIFIPASS
          if (p->name().equals(CONF_KEY_WIFIPASS)) {
            strcpy(received_device_config.wifiPass,  p->value().c_str());
            continue;
          }
          // CONF_KEY_SERVERURL
          if (p->name().equals(CONF_KEY_SERVERURL)) {
            strcpy(received_device_config.serverURL,  p->value().c_str());
            continue;
          }
          
        } else { // we do not support other parameters but will let know to user we've got it
          Serial.printf("Unsupported paameter _GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }

      // print configuration to Serial
      Serial.println("Received configuration:");
      print_config_structure(&received_device_config);
      

      // validate if the configuration is not empty
      if (!vaidate_config_struct(&received_device_config)) {
        Serial.println("Validation ERROR");
        request->send(400, "text/html", "Input validation error");
        return;
      }

      // write to EEPROM
      write_config_2_eeprom(&received_device_config);

      
      request->send(200, "text/html", "<p>Configuration written to EEPROM<p><p><a href=\"/\"> Back to Configurator</a></p>");
    });

    // handle request to server root
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){

      /* read existing configuration from EEPROM */
      loadStruct(&stored_device_config, sizeof(stored_device_config));
      Serial.println("Existing configuration found in EEPROM:");
      print_config_structure(&stored_device_config);
      
      Serial.println("DEBUG: Formatting output string ...");
      html_response = String((char *)config_form_HTML);
      html_response.replace("_FW_VERSION", get_fw_version());
      html_response.replace("_WIFISSID", String(stored_device_config.wifiSSID));
      html_response.replace("_WIFIPASS", String(stored_device_config.wifiPass));
      html_response.replace("_SERVERURL", String(stored_device_config.serverURL));
      
      Serial.println("DEBUG: HTML Template: ");
      Serial.println(html_response);
      
      // sending the HTML form
      request->send(200, "text/html", html_response);
    });

    

    // begin configuration server operation; http://SERVER_IP:80/
    server.begin();

    Serial.print("Server ready at http://");
    Serial.println(WiFi.softAPIP());
     
  } else { // normal operation


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
void write_config_2_eeprom(FAQ_DEVICE_CONFIG *conf) {

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM");
    return;
  }

  Serial.println("Flashing " + String(sizeof(*conf)) + " bytes to EEPROM");

  storeStruct(conf, sizeof(*conf)); 
}

/* Validators */
boolean vaidate_config_struct(FAQ_DEVICE_CONFIG *conf) {

  /*
   * TODO:
   * - check if fields are not empty
   * - check if serverURL field matches URL pattern
   */

  return true;
}

/* other struct routines */

// print device config to serial
void print_config_structure(FAQ_DEVICE_CONFIG *conf) {
  Serial.printf("WiFi SSID: %s\n", conf->wifiSSID);
  Serial.printf("WiFi Password: %s\n", conf->wifiPass);
  Serial.printf("Server URL: %s\n", conf->serverURL);
  
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
