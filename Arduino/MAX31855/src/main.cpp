#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include <NTPClient.h>

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

// MAX31855 Thermocouple
// Example creating a thermocouple instance with software SPI on any three digital IO pins.
#define MAXDO     12
#define MAXCLK    14
#define MAXCS0    13
#define MAXCS1    16

// Initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS0, MAXDO);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "de.pool.ntp.org");

static void max31855_send(void);

void setup(void) {
  common_setup();
  timeClient.begin();
  delay(500);
}

void loop(void) {
  static long last_value_sent;

  if (!common_loop()) {
    // Something went wrong, skip further execution
    return;
  }

  timeClient.update();

  // Send sensor values every SENSOR_INTERVAL
  if (last_value_sent == 0 || SENSOR_INTERVAL == 0 || (millis() - last_value_sent) >= SENSOR_INTERVAL) {
    last_value_sent = millis();
    max31855_send();
  }

}

static void max31855_send() {
  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();

  json_data["sensor"]   = "max31855";
  json_data["uptime"]   = millis();
  json_data["time"]     = timeClient.getEpochTime();
  json_data["time_ms"]  = timeClient.getEpochTimeMs();
  json_data["temp0"]    = thermocouple.readCelsius();

  json_send(mqtt_sensor_topic.c_str(), json_data);
}

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
      Serial.println("wifi_connect() ");
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
