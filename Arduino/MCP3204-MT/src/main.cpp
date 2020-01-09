///////////////////////// COMMON HEADER BEGIN
#include <PubSubClient.h>
#include <ArduinoJson.h>

#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif // OTA_ENABLED

#ifdef HMAC_ENABLED
#include <base64.h>
#include "mbedtls/md.h"
mbedtls_md_context_t md_context;
mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
const char *md_key = HMAC_KEY;
#endif // HMAC_ENABLED

#ifdef TARGET_ESP8266
#include <ESP8266WiFi.h>
#else // TARGET_ESP8266
#include <WiFi.h>
#endif // TARGET_ESP8266

// Buffer size used at various places
#define BUFFER_SIZE 512

String mqtt_client_id;
String mqtt_sensor_topic;
String mqtt_beacon_topic;
char mqtt_last_will_buffer[BUFFER_SIZE];

// WiFi / MQTT
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

static bool common_setup(void);
static bool common_loop(void);
static void beacon_send(void);
static bool mqtt_connect(void);
static bool wifi_connect(void);
static bool json_send(const char *mqtt_topic, JsonObject json_data);
static int json_serialize(JsonObject json_data, char *buffer, size_t buffer_size);
///////////////////////// COMMON HEADER END

#include <SPI.h>
#include <Wire.h>
#include <Mcp320x.h>

#define SPI_CS      5        // SPI slave select
#define ADC_VREF    3300     // 3.3V Vref
#define ADC_CLK     1600000  // SPI clock 1.6MHz

typedef struct{
  char buffer[BUFFER_SIZE];
  int buffer_len;
} xQueue_Data;

xQueueHandle xQueue;
void sensor_task(void * param);
TaskHandle_t sensor_task_handle;

MCP3204 adc(ADC_VREF, SPI_CS);

void setup(void) {
  common_setup();

  // configure PIN mode
  pinMode(SPI_CS, OUTPUT);
  // set initial PIN state
  digitalWrite(SPI_CS, HIGH);

  // initialize SPI interface for MCP3204
  SPISettings settings(ADC_CLK, MSBFIRST, SPI_MODE0);
  SPI.begin();
  SPI.beginTransaction(settings);

  xQueue = xQueueCreate(100, sizeof(xQueue_Data));

  xTaskCreatePinnedToCore(
    sensor_task, /* Function to implement the task */
    "sensor_task", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &sensor_task_handle,  /* Task handle. */
    0); /* Core where the task should run */

  delay(500);
}

void loop(void) {
  /* keep the status of receiving data */
  BaseType_t xStatus;
  /* time to block the task until data is available */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  xQueue_Data queue_element;

  if (!common_loop()) {
    // Something went wrong, skip further execution
    return;
  }

  /* receive data from the queue */
  xStatus = xQueueReceive( xQueue, &queue_element, xTicksToWait );
  /* check whether receiving is ok or not */
  if(xStatus == pdPASS){
    /* print the data to terminal */
    //Serial.print("receiveTask got data: ");
    mqtt_client.publish(mqtt_sensor_topic.c_str(), (const uint8_t *) queue_element.buffer, queue_element.buffer_len);
  }

}

void sensor_task(void * param) {
  /* keep the status of sending data */
  BaseType_t xStatus;
  /* time to block the task until the queue has free space */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);

  StaticJsonDocument<512> json_doc;
  JsonObject json_data = json_doc.to<JsonObject>();
  uint16_t raw;
  uint16_t mv;

  xQueue_Data queue_element;

  for(;;){
    //Serial.println("sendTask1 is sending data");

    raw = adc.read(MCP3204::Channel::SINGLE_0);
    // get analog value
    mv = adc.toAnalog(raw);

    json_data["sensor"]   = "mcp3204";
    json_data["uptime"]   = millis();
    json_data["mv"]       = mv;

    //json_send(mqtt_sensor_topic.c_str(), json_data);
    queue_element.buffer_len = json_serialize(json_data, queue_element.buffer, BUFFER_SIZE);
    //mqtt_client.publish(mqtt_sensor_topic.c_str(), (const uint8_t *) queue_element.buffer, queue_element.buffer_len);
    static long hz_counter;
    static long hz_counter_delta;

    if ((millis() - hz_counter_delta) >= 1000) {
      Serial.printf("Freq : %ld Hz - Core: %d\n", hz_counter, xPortGetCoreID());
      hz_counter_delta = millis();
      hz_counter = 0;
    }



    /* send data to front of the queue */
    xStatus = xQueueSendToFront( xQueue, &queue_element, xTicksToWait );
    /* check whether sending is ok or not */
    if( xStatus == pdPASS ) {
      /* increase counter of sender 1 */
      hz_counter++;
    }
    /* we delay here so that receiveTask has chance to receive data */
    //delay(1000);
  }
  vTaskDelete( NULL );
}

///////////////////////// COMMON FUNCTIONS BEGIN
static bool common_setup(void) {
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
  serializeJson(json_data, mqtt_last_will_buffer, BUFFER_SIZE);

#ifdef OTA_ENABLED
  ArduinoOTA.setPassword("bruttonetto");
#ifdef TARGET_ESP32
  ArduinoOTA.setMdnsEnabled(false);
#endif // TARGET_ESP32
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

#ifdef HMAC_ENABLED
  mbedtls_md_init(&md_context);
  mbedtls_md_setup(&md_context, mbedtls_md_info_from_type(md_type), 1);
#endif // HMAC_ENABLED

  Serial.println(F("Initialized"));
  return true;
}

bool common_loop(void) {
  static long last_beacon_sent;
  static long last_led_toggle;
#ifdef DEBUG_OUTPUT
  static long hz_counter;
  static long hz_counter_delta;
#endif // DEBUG_OUTPUT

  // Toggle status LED as activity indicator
  if (last_led_toggle == 0 || (millis() - last_led_toggle) >= 500) {
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

  // Send a beacon every BEACON_INTERVAL
  if (last_beacon_sent == 0 || BEACON_INTERVAL == 0 || (millis() - last_beacon_sent) >= BEACON_INTERVAL) {
    beacon_send();
    last_beacon_sent = millis();
  }

#ifdef DEBUG_OUTPUT
  if ((millis() - hz_counter_delta) >= 1000) {
    Serial.printf("Freq : %ld Hz\n", hz_counter);
    hz_counter_delta = millis();
    hz_counter = 0;
  }
  hz_counter++;
#endif

  return true;
}

static void beacon_send(void) {
  uint8_t* bssid = WiFi.BSSID();
  char mac[18];
  char desc[64];
  StaticJsonDocument<BUFFER_SIZE> json_doc;
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
  char buffer[BUFFER_SIZE];
  int bufferlen;

  bufferlen = json_serialize(json_data, buffer, BUFFER_SIZE);

#ifdef DEBUG_OUTPUT
  Serial.print("MSG OUT [");
  Serial.print(mqtt_topic);
  Serial.print("] ");
  Serial.println(buffer);
#endif // DEBUG_OUTPUT

#ifdef MQTT_OUTPUT
  if (!mqtt_client.publish(mqtt_topic, (const uint8_t *) buffer, bufferlen)) {
    Serial.println("publish failed!");
    return false;
  }
#endif // MQTT_OUTPUT

  return true;
}

static int json_serialize(JsonObject json_data, char *buffer, size_t buffer_size) {
  int buffer_filled = 0;

#ifdef HMAC_ENABLED
  byte hmac_hash[32];

  json_data["hmac"] = "";
  buffer_filled = serializeJson(json_data, buffer, buffer_size);

  mbedtls_md_hmac_starts(&md_context, (const unsigned char *) md_key, strlen(md_key));
  mbedtls_md_hmac_update(&md_context, (const unsigned char *) buffer, buffer_filled);
  mbedtls_md_hmac_finish(&md_context, hmac_hash);
  json_data["hmac"] = base64::encode(hmac_hash, 32);

#if 0
  for (int i= 0; i< sizeof(hmac_hash); i++){
      char str[3];
      sprintf(str, "%02X", (int)hmac_hash[i]);
      Serial.print(str);

  }
  Serial.println("");
#endif // DEBUG_OUTPUT
#endif // HMAC_ENABLED

  buffer_filled = serializeJson(json_data, buffer, buffer_size);

  return buffer_filled;
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

  // Try to connect
  if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_beacon_topic.c_str(), 0, false, mqtt_last_will_buffer)) {
    Serial.println(F("connected"));
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
      Serial.println("Connection failed after 10 tries, calling WiFi.reconnect()");
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
