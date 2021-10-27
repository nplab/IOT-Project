///////////////////////// COMMON HEADER BEGIN
#define CONFIG_WIFI_NAME "WIFI_NAME"
#define CONFIG_WIFI_PSK "WIFI_PASSWORD"
#define CONFIG_MQTT_BROKER_ADDRESS "iot.fh-muenster.de"
#define CONFIG_MQTT_BROKER_USER "MQTT_USER"
#define CONFIG_MQTT_BROKER_PASSWORD "MQTT_PASSWORD"

#define DEBUG_OUTPUT
#define MQTT_OUTPUT

#define BEACON_INTERVAL 5000

//#define MQTT_MAX_PACKET_SIZE 512 << BE AWARE OF THIS!!

#ifndef DEVICE_TYPE
#define DEVICE_TYPE "GENERIC-ESP"
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <WiFi.h>
#else // ESP32
#include <ESP8266WiFi.h>
#endif // ESP32

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

#define SENSOR_INTERVAL 100

static void mqtt_callback(char* topic, byte* payload, unsigned int length);

void setup(void) {
	common_setup();

	pinMode(19, OUTPUT);
	pinMode(22, OUTPUT);

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
		// Action starts here...
	}
}


void mqtt_callback(char* topic, byte* payload, unsigned int length) {

/*
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();
*/

	StaticJsonDocument<BUFFER_SIZE> doc;
	DeserializationError error = deserializeJson(doc, payload, length);

	// Test if parsing succeeds.
	if (error) {
		Serial.print(F("deserializeJson() failed: "));
		Serial.println(error.f_str());
		return;
	}

	String sensor = doc["sensor"];

	if (sensor == "tcs34725") {
		int lux = doc["lux"];

		if (lux < 100) {
			digitalWrite(19, LOW);
			digitalWrite(22, HIGH);
		} else {
			digitalWrite(19, HIGH);
			digitalWrite(22, LOW);
		}
		Serial.printf("tcs34725 - lux: %d\n", lux);
	}
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
	WiFi.begin(CONFIG_WIFI_NAME, CONFIG_WIFI_PSK);

	// MQTT setup
	mqtt_client.setServer(CONFIG_MQTT_BROKER_ADDRESS, 1883);
	mqtt_client.setCallback(mqtt_callback);
	mqtt_client_id = String(WiFi.macAddress());
	mqtt_sensor_topic = "sensor/" + mqtt_client_id;
	mqtt_beacon_topic = "beacon/" + mqtt_client_id;

	// MQTT last will
	StaticJsonDocument<128> json_doc;
	JsonObject json_data  = json_doc.to<JsonObject>();
	json_data["id"]       = mqtt_client_id;
	json_data["type"]     = 2;
	serializeJson(json_data, mqtt_last_will_buffer, BUFFER_SIZE);

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
		Serial.printf("Loop Freq : %ld Hz\n", hz_counter);
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

	snprintf(desc, sizeof(desc), "%s|%s", WiFi.localIP().toString().c_str(), __DATE__);

	json_data["id"]       = mqtt_client_id;
	json_data["type"]     = 1;
	json_data["device"]   = DEVICE_TYPE;
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
	if (mqtt_client.connect(mqtt_client_id.c_str(), CONFIG_MQTT_BROKER_USER, CONFIG_MQTT_BROKER_PASSWORD, mqtt_beacon_topic.c_str(), 0, false, mqtt_last_will_buffer)) {
		Serial.println(F("connected"));
		mqtt_client.subscribe("sensor/3C:71:BF:AB:32:D4");
		return true;
	} else {
		Serial.print("failed, rc=");
		Serial.println(mqtt_client.state());
		return false;
	}
}

static bool wifi_connect(void) {
	int connection_tries = 0;

	if (WiFi.isConnected()) {
		return true;
	}

	while (!WiFi.isConnected()) {
		delay(1000);
		Serial.print(".");

		if (++connection_tries >= 5) {
			Serial.println(" - failed");
			Serial.println("Connection failed after 5 tries, waiting 61 seconds and calling WiFi.reconnect()");
			delay(61000); // delaying for 61 seconds thanks to &^%$# security policies of our wifi vendor
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