///////////////////////// COMMON HEADER BEGIN
#define CONFIG_WIFI_NAME "WIFI_NAME"
#define CONFIG_WIFI_PSK "WIFI_PASS"
#define CONFIG_MQTT_BROKER_ADDRESS "iot.fh-muenster.de"

#define DEBUG_OUTPUT
#define MQTT_OUTPUT

#define BEACON_INTERVAL 5000

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

#include <WiFiClientSecure.h>
const char* test_root_ca= \
	"-----BEGIN CERTIFICATE-----\n" \
	"MIIEDDCCAvSgAwIBAgIJAPx+A7Web8LQMA0GCSqGSIb3DQEBCwUAMIGaMQswCQYD\n" \
	"VQQGEwJERTEMMAoGA1UECAwDTlJXMRIwEAYDVQQHDAlTdGVpbmZ1cnQxFDASBgNV\n" \
	"BAoMC0ZILU11ZW5zdGVyMQ4wDAYDVQQLDAVOUExhYjEbMBkGA1UEAwwSaW90LmZo\n" \
	"LW11ZW5zdGVyLmRlMSYwJAYJKoZIhvcNAQkBFhd3ZWlucmFua0BmaC1tdWVuc3Rl\n" \
	"ci5kZTAeFw0yMTAzMjIxNTEyMzRaFw0zMTAzMjAxNTEyMzRaMIGaMQswCQYDVQQG\n" \
	"EwJERTEMMAoGA1UECAwDTlJXMRIwEAYDVQQHDAlTdGVpbmZ1cnQxFDASBgNVBAoM\n" \
	"C0ZILU11ZW5zdGVyMQ4wDAYDVQQLDAVOUExhYjEbMBkGA1UEAwwSaW90LmZoLW11\n" \
	"ZW5zdGVyLmRlMSYwJAYJKoZIhvcNAQkBFhd3ZWlucmFua0BmaC1tdWVuc3Rlci5k\n" \
	"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALil1RJaPdPfYuQ11CIs\n" \
	"1D0wX7DqyySPQW48MuS9x5mcwljEIc+0HRJsk4KJtVH7ZHcHvMPJnwzhwe1OYYoV\n" \
	"dD6jbBsbZ3MfslbBUtoRX/9Wt2TooH4NzToIayr88Q//Z9H+NUJuLsR8Nlaz3NL4\n" \
	"S04HC5pd/1RPWfAveW/TjCKF1++PJnvw6Bv1eXahJu0NEqtGRP8Mwm+lAzHgMafQ\n" \
	"dFTjcen07hW0kT1z3AFuobOpbw1CWzXtvLcDy5rnDZpkwWV8BBp9eI2rrzRS9WOk\n" \
	"DEbBamiNqBtp61aJDwDft9gar0ZM6lt1o+d5RsYNEN5uWq8Q2AEHtrOjxgznze4+\n" \
	"06UCAwEAAaNTMFEwHQYDVR0OBBYEFFArmuxCphlxjSwEygGheFqAjsqsMB8GA1Ud\n" \
	"IwQYMBaAFFArmuxCphlxjSwEygGheFqAjsqsMA8GA1UdEwEB/wQFMAMBAf8wDQYJ\n" \
	"KoZIhvcNAQELBQADggEBADeEbf+BJZ5NVk3/aScfsIjvWDoLxoGzZks0O7VsCyuI\n" \
	"an/eG0USXVYklXkfxPap82gK1KKHSTh4fthlzkpDJueaFv1fxlQXnSGJLW/zOFOt\n" \
	"x8jzQFiAhuepAnjjHoTbYKMufVQYI89uTSD79xaQBfA55xCqBkOcm47n2LSSsfkc\n" \
	"DaWwe/VaBGuVivcYXJ4q5VJodR3Zv4wTNdofjvRTA76oSiIYSy4xw5cZJxY3BNkR\n" \
	"8gA5bd6aFiwi8sxPVViZCTXIBfHpFrQi9IQBMIldqqxy52ds4IjHjmrxwi1+IRD4\n" \
	"lKnBBRVFt5F6OAMMnhIUTuQu8pbi6VbJX7IPE5ODjzc=\n" \
	"-----END CERTIFICATE-----\n";

// Buffer size used at various places
#define BUFFER_SIZE 512

String mqtt_client_id;
String mqtt_sensor_topic;
String mqtt_beacon_topic;
char mqtt_last_will_buffer[BUFFER_SIZE];

// WiFi / MQTT
#define MQTT_PORT 8883
WiFiClientSecure espClient;

PubSubClient mqtt_client(espClient);

static bool common_setup(void);
static bool common_loop(void);
static void beacon_send(void);
static bool mqtt_connect(void);
static bool wifi_connect(void);
static bool json_send(const char *mqtt_topic, JsonObject json_data);
static int json_serialize(JsonObject json_data, char *buffer, size_t buffer_size);
///////////////////////// COMMON HEADER END

#define SENSOR_INTERVAL 5000

void setup(void) {
	common_setup();
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
		
		StaticJsonDocument<BUFFER_SIZE> json_doc;
		JsonObject json_data = json_doc.to<JsonObject>();

		json_data["msg"]     = "Hallo Welt...";
		json_data["uptime"]  = millis();

		json_send(mqtt_sensor_topic.c_str(), json_data);
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

#ifdef ESP32
	espClient.setCACert(test_root_ca);
#else
	espClient.setInsecure();
#endif

	// MQTT setup
	mqtt_client.setServer(CONFIG_MQTT_BROKER_ADDRESS, MQTT_PORT);
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

	// Maintain MQTT connection
	if (!mqtt_connect()) {
		delay(5000);
		return false;
	}
	mqtt_client.loop();

	// Send a beacon every BEACON_INTERVAL
	if (last_beacon_sent == 0 || BEACON_INTERVAL == 0 || (millis() - last_beacon_sent) >= BEACON_INTERVAL) {
		beacon_send();
		last_beacon_sent = millis();
	}

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

	Serial.print("MSG OUT [");
	Serial.print(mqtt_topic);
	Serial.print("] ");
	Serial.println(buffer);

	if (!mqtt_client.publish(mqtt_topic, (const uint8_t *) buffer, bufferlen)) {
		Serial.println("publish failed!");
		return false;
	}

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