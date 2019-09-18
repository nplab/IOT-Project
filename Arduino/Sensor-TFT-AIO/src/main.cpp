#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_BME680.h>
#include <Adafruit_MMA8451.h>
#include <DHT.h>

///////////////////////// COMMON HEADER BEGIN
#include <PubSubClient.h>
#include <ArduinoJson.h>

#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif // OTA_ENABLED

#ifdef TARGET_ESP8266
#include <ESP8266WiFi.h>
#else // TARGET_ESP8266
#include <WiFi.h>
#endif // TARGET_ESP8266

String mqtt_client_id;
String mqtt_sensor_topic;
String mqtt_beacon_topic;
char mqtt_last_will_buffer[256];

// WiFi / MQTT
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

static bool common_setup(void);
static bool common_loop(void);
static void beacon_send(void);
static bool mqtt_connect(void);
static bool wifi_connect(void);
static bool json_send(const char *mqtt_topic, JsonObject json_data);
///////////////////////// COMMON HEADER END

#define SEALEVELPRESSURE_HPA (1013.25)

#ifdef TFT_OUTPUT
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
// For the breakout board, you can use any 2 or 3 pins.
// These pins will also work for the 1.8" TFT shield.
#define TFT_CS    5
#define TFT_RST   4 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC    2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

static void tft_print_weather(double temperature, double humidity, double altitude);
static void tft_print_status(void);

#define TFT_FAILURE_WIFI 1
#define TFT_FAILURE_MQTT 2
#endif // TFT_OUTPUT

bool bme_present    = false;
bool sht_present    = false;
bool tcs_present    = false;
bool mma_present    = false;
bool dht_present    = false;

// Sensors
Adafruit_BME680 bme; // I2C
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);
Adafruit_MMA8451 mma = Adafruit_MMA8451();

#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

static void bme_setup(void);
static void tcs_send(void);
static void mma_send();
static void bme_send(void);
static void dht_send(void);
static void sht_send(void);

void setup(void) {
  common_setup();

  // Initialize SHT
  if (sht31.begin(0x44) && sht31.readStatus() != 65535) {   // Set to 0x45 for alternate i2c addr
    sht_present = true;
    Serial.print(F("SHT31 detected"));
  }

  // Initialize BME
  if (bme.begin()) {
    bme_present = true;
    bme_setup();
    Serial.print(F("BME680 detected"));
  }

  // Initialize TCS
  if (tcs.begin()) {
    tcs_present = true;
    Serial.print(F("TCS46725 detected"));
  }

  // Initialize MMA
  if (mma.begin()) {
    mma_present = true;
    mma.setRange(MMA8451_RANGE_4_G);
    Serial.print(F("MMA8451 detected"));
  }

  // Initialize DHTXX
  dht.begin();
  delay(1000);
  if (dht.read()) {
    dht_present = true;
    Serial.print(F("DHTxx detected"));
  }

#ifdef TFT_OUTPUT
  // Use this initializer if using a 1.8" TFT screen:
  tft.initR();      // Init ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK);
#endif

  delay(500);
}

void loop(void) {
  static long last_value_sent;

  if (!common_loop()) {
    // Something went wrong, skip further execution
    return;
  }

  // Send sensor values every SENSOR_INTERVAL
  if (last_value_sent == 0 || SENSOR_INTERVAL == 0 || (millis() - last_value_sent) >= SENSOR_INTERVAL) {
    last_value_sent = millis();

    if (bme_present) {
      bme_send();
    }

    if (sht_present) {
      sht_send();
    }

    if (tcs_present) {
      tcs_send();
    }

    if (mma_present) {
      mma_send();
    }

    if (dht_present) {
      dht_send();
    }
  }

#ifdef TFT_OUTPUT
  static long last_display_change = 0;
  static uint8_t display_page = 0;
  if (last_display_change == 0 || (millis() - last_display_change) > DISPLAY_INTERVAL) {
#ifdef DEBUG_OUTPUT
    Serial.println(F("Changing TFT..."));
#endif
    last_display_change = millis();
    display_page = (display_page + 1 ) % 2;
    if (display_page == 0 && bme_present) {
      tft_print_weather(bme.temperature, bme.humidity, bme.readAltitude(SEALEVELPRESSURE_HPA));
    } else {
      tft_print_status();
    }
  }
#endif
}

static void sht_send(void) {
  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"] = "sht31";
  json_data["temp"]   = sht31.readTemperature();
  json_data["humi"]   = sht31.readHumidity();

  json_send(mqtt_sensor_topic.c_str(), json_data);
}

static void bme_setup(void) {
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  //bme.setGasHeater(320, 150); // 320*C for 150 ms
  bme.setGasHeater(0, 0); // 320*C for 150 ms
}

static void bme_send(void) {
  if (!bme.performReading()) {
    Serial.println(F("bme_send() - bme.performReading() failed"));
    return;
  }

  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"] = "bme680";
  json_data["temp"] = bme.temperature;
  json_data["humi"] = bme.humidity;
  json_data["press"] = bme.pressure;
  
  json_send(mqtt_sensor_topic.c_str(), json_data);
}

static void tcs_send(void) {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"] = "tcs34725";
  json_data["temp"]   = tcs.calculateColorTemperature_dn40(r, g, b, c);
  json_data["lux"]    = tcs.calculateLux(r, g, b);

  json_send(mqtt_sensor_topic.c_str(), json_data);
}

static void dht_send(void) {
  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"] = "dht";
  json_data["temp"]   = dht.readTemperature();
  json_data["humi"]   = dht.readHumidity();

  json_send(mqtt_sensor_topic.c_str(), json_data);
}

static void mma_send() {
  mma.read();

  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"] = "mma8451";
  json_data["x"]      = mma.x;
  json_data["y"]      = mma.y;
  json_data["z"]      = mma.z;

  json_send(mqtt_sensor_topic.c_str(), json_data);
}

#ifdef TFT_OUTPUT
static void tft_print_weather(double temperature, double humidity, double altitude) {

  int y_offset        = 3;
  int x_offset        = 10;

  tft.setTextWrap(false);
  tft.setTextSize(3);
  //tft.fillScreen(ST7735_BLACK);
  
  tft.fillRect(0, 0 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_RED);
  tft.fillRect(0, 1 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_BLUE);
  tft.fillRect(0, 2 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_BLACK);
  
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(x_offset, 0 + y_offset);
  tft.println(" Temp");
  tft.setCursor(x_offset, tft.getCursorY());
  tft.print(temperature, 1);
  tft.println(" C");

  tft.setCursor(x_offset, 1 * tft.height() / 3 + y_offset);
  tft.setTextColor(ST7735_WHITE);
  tft.println(" Humi");
  tft.setCursor(x_offset, tft.getCursorY());
  tft.print(humidity, 1);
  tft.println(" %");

  tft.setCursor(x_offset, 2 * tft.height() / 3 + y_offset);
  tft.setTextColor(ST7735_WHITE);
  tft.println(" Alti");
  tft.setCursor(x_offset, tft.getCursorY());
  tft.print(altitude, 0);
  tft.println(" M");
}

static void tft_print_status(void) {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  
  tft.println("# RSSI");
  tft.println(WiFi.RSSI());
  tft.println("");
  tft.println("# MAC");
  tft.println(String(WiFi.macAddress()));
  tft.println("");
  tft.println("# IP");
  tft.println(WiFi.localIP());
  tft.println("");
  tft.println("# SM");
  tft.println(WiFi.subnetMask());
  tft.println("");
  tft.println("# GW");
  tft.println(WiFi.gatewayIP());
  tft.println("");
  tft.println("# MQTT status");
  if (mqtt_client.state() == MQTT_CONNECTED) {
    tft.println("Connected!");
  } else {
    tft.println(mqtt_client.state());
  }
}
#endif

///////////////////////// COMMON FUNCTIONS BEGIN
bool common_setup(void) {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("Hello! Let's go..."));

  // We're using the pin for activity indication
  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(micros());

  // WIFI
  Serial.printf("Connecting to WIFI network: %s\n", CONFIG_WIFI_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(CONFIG_WIFI_NAME, CONFIG_WIFI_PSK);
  wifi_connect();

  // MQTT setup
  mqtt_client.setServer(CONFIG_MQTT_BROKER_ADDRESS, 1883);
  mqtt_client_id = String(WiFi.macAddress());
  mqtt_sensor_topic = "sensor/" + mqtt_client_id;
  mqtt_beacon_topic = "beacon/" + mqtt_client_id;

  // MQTT last will
  StaticJsonDocument<128> json_doc;
  JsonObject json_data  = json_doc.to<JsonObject>();
  json_data["id"]       = mqtt_client_id;
  json_data["type"]     = 2;
  serializeJson(json_data, mqtt_last_will_buffer);

#ifdef OTA_ENABLED
  ArduinoOTA.setPassword("bruttonetto");
#ifdef TARGET_ESP32
  ArduinoOTA.setMdnsEnabled(false);
#endif
  ArduinoOTA.setPort(54321);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);

      switch (error) {
        case OTA_AUTH_ERROR:
          Serial.println(F("OTA - Auth failed"));
          break;
        case OTA_BEGIN_ERROR:
          Serial.println(F("OTA - Begin failed"));
          break;
        case OTA_CONNECT_ERROR:
          Serial.println(F("OTA - Connect failed"));
          break;
        case OTA_RECEIVE_ERROR:
          Serial.println(F("OTA - Receive failed"));
          break;
        case OTA_END_ERROR:
          Serial.println(F("OTA - End failed"));
          break;
      }
    });

  ArduinoOTA.begin();
#endif // OTA_ENABLED

  Serial.println(F("Initialized"));
  return true;
}

bool common_loop(void) {
  static long last_beacon_sent;
  static long last_led_toggle;

  // Toggle status LED
  if (last_led_toggle == 0 || (millis() - last_led_toggle) >= 250) {
    last_led_toggle = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // Maintain WiFi connection
  if (!wifi_connect()) {
    delay(5000);
    return false;
  }

#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif // OTA_ENABLED

#ifdef MQTT_OUTPUT
  // Maintain MQTT connection
  if (!mqtt_connect()) {
    delay(5000);
    return false;
  }
  mqtt_client.loop();
#endif // MQTT_OUTPUT

  // Send an beacon every BEACON_INTERVAL
  if (last_beacon_sent == 0 || BEACON_INTERVAL == 0 || (millis() - last_beacon_sent) >= BEACON_INTERVAL) {
    last_beacon_sent = millis();
    beacon_send();
  }

  return true;
}


static void beacon_send(void) {
  uint8_t* bssid = WiFi.BSSID();
  char mac[18];
  char desc[64];
  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  snprintf(desc, sizeof(desc), "%s|%d", WiFi.localIP().toString().c_str(), BUILD_ID);

  json_data["id"]       = mqtt_client_id;
  json_data["type"]     = 1;
  json_data["device"]   = PIOENV;
  json_data["desc"]     = desc;
  json_data["rssi"]     = WiFi.RSSI();
  json_data["uptime"]   = millis();

  if (bssid) {
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    json_data["bssid"]  = String(mac);
  }

  json_send(mqtt_beacon_topic.c_str(), json_data);
}

static bool json_send(const char *mqtt_topic, JsonObject json_data) {
  char buffer[512];
  int bufferlen = serializeJson(json_data, buffer);

#ifdef DEBUG_OUTPUT
  Serial.print("MSG OUT [");
  Serial.print(mqtt_topic);
  Serial.print("] ");
  Serial.println(buffer);
#endif

#ifdef MQTT_OUTPUT
  if (!mqtt_client.publish(mqtt_topic, (const uint8_t *) buffer, bufferlen)) {
    Serial.println("publish failed!");
    return false;
  }
#endif

  return true;
}

static bool mqtt_connect(void) {
  // Loop until we're reconnected
  if (mqtt_client.connected()) {
    return true;
  }

  Serial.print(F("espClient.connected() = "));
  Serial.println(espClient.connected());

  Serial.print(F("Current MQTT state : "));
  Serial.println(mqtt_client.state());

  Serial.print(F("Attempting MQTT connection to "));
  Serial.print(CONFIG_MQTT_BROKER_ADDRESS);
  Serial.print(" ...");

  // Attempt to connect
  if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_beacon_topic.c_str(), 0, false, mqtt_last_will_buffer)) {
    Serial.println(F("connected"));
    //mqtt_client.subscribe("inTopic");
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqtt_client.state());
    return false;
  }

  return false;
}

static bool wifi_connect(void) {
  int connection_tries = 0;

  if (WiFi.isConnected()) {
    return true;
  }

  while (!WiFi.isConnected()) {
    delay(1000);
    Serial.print(".");

    if (++connection_tries >= 10) {
      Serial.print(" Connection failed after 10 tries, calling WiFi.reconnect()");
      WiFi.reconnect();
      return false;
    }
  }

  Serial.println(" connected!");
  Serial.print("MAC: ");
  Serial.println(String(WiFi.macAddress()));
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SM: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("GW: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("BSSID: ");
  Serial.println(WiFi.BSSIDstr());
  Serial.println("");

  return true;
}

///////////////////////// COMMON FUNCTIONS END