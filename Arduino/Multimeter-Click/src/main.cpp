///////////////////////// COMMON HEADER BEGIN
#define CONFIG_WIFI_NAME "npiot"
#define CONFIG_WIFI_PSK "12345678"
#define CONFIG_MQTT_BROKER_ADDRESS "iot.fh-muenster.de"
#define CONFIG_MQTT_BROKER_USER "MQTT_USER"
#define CONFIG_MQTT_BROKER_PASSWORD "MQTT_PASSWORD"

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

// Buffer size used at various places
#define BUFFER_SIZE 512

String mqtt_client_id;
String mqtt_sensor_topic;
String mqtt_beacon_topic;
char mqtt_last_will_buffer[BUFFER_SIZE];

// WiFi / MQTT
#define MQTT_PORT 1883

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

#define SENSOR_INTERVAL 250
#define DISPLAY_INTERVAL 250

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define CS_PIN 27
#define CLOCK_PIN 5
#define MOSI_PIN 18
#define MISO_PIN 19

#define PIN_A 33
#define PIN_B 4
#define PIN_C 36

#define uDelay 50

static uint8_t I_CHANNEL = 0x01;
static uint8_t U_CHANNEL = 0x02;
static uint8_t R_CHANNEL = 0x03;
static uint8_t C_CHANNEL = 0x04;

float current = 0.0; 
float calibrationI = 0.0; 
float calibrationU = 0.0;
float calibrationC = 0.0;

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

int readADC(int channel){
	int adcvalue = 0;
	byte commandbits = B11000000; //command bits - start, mode, chn (3), dont care (3)

	//allow channel selection
	commandbits|=((channel-1)<<3);

	digitalWrite(CS_PIN,LOW); //Select adc
	// setup bits to be written
	for (int i=7; i>=3; i--){
		digitalWrite(MOSI_PIN,commandbits&1<<i);
		delayMicroseconds(uDelay);

		//cycle clock
		digitalWrite(CLOCK_PIN,HIGH);
		delayMicroseconds(uDelay);
		digitalWrite(CLOCK_PIN,LOW); 
		delayMicroseconds(uDelay);   
	}

	digitalWrite(CLOCK_PIN,HIGH); //ignores 2 null bits
	delayMicroseconds(uDelay);
	digitalWrite(CLOCK_PIN,LOW);
	delayMicroseconds(uDelay);
	digitalWrite(CLOCK_PIN,HIGH);  
	delayMicroseconds(uDelay);
	digitalWrite(CLOCK_PIN,LOW);
	delayMicroseconds(uDelay);

	//read bits from adc
	for (int i=11; i>=0; i--){
		adcvalue+=digitalRead(MISO_PIN)<<i;
		delayMicroseconds(uDelay);
		//cycle clock
		digitalWrite(CLOCK_PIN,HIGH);
		delayMicroseconds(uDelay);
		digitalWrite(CLOCK_PIN,LOW);
		delayMicroseconds(uDelay);
	}
	digitalWrite(CS_PIN, HIGH); //turn off device
	return adcvalue;
}

void measure_voltage(void) {
	int readvalueU = readADC(U_CHANNEL);
	float voltage = (readvalueU - calibrationU)*0.008333;  

	StaticJsonDocument<256> json_doc;
	JsonObject json_data = json_doc.to<JsonObject>();

	json_data["sensor"]   = "mmclick";
	json_data["uptime"]   = millis();
	json_data["v_c0"]    = voltage;

	json_send(mqtt_sensor_topic.c_str(), json_data);
} 

void setup(void) {
	common_setup();

	// ---  INITIALISE PINS
	pinMode(CS_PIN, OUTPUT); 
	pinMode(MOSI_PIN, OUTPUT); 
	pinMode(MISO_PIN, INPUT); 
	pinMode(CLOCK_PIN, OUTPUT); 

	pinMode(PIN_A, OUTPUT);
	pinMode(PIN_B, OUTPUT);
	pinMode(PIN_C, OUTPUT);

	digitalWrite(PIN_A, LOW); 
	digitalWrite(PIN_B, HIGH);
	digitalWrite(PIN_C, HIGH);

	digitalWrite(CS_PIN,HIGH); 
	digitalWrite(MOSI_PIN,LOW); 
	digitalWrite(CLOCK_PIN,LOW); 

	// --- CALIBRATE CHANNELS
	//calibrationC = (64285 / (readADC(C_CHANNEL)/2*4));
	calibrationI =  2048; //readADC(I_CHANNEL)/2;
	calibrationU =  2048; //readADC(U_CHANNEL)/2;

	// --- INITIALISE OLED DISPLAY
	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
	display.setRotation(2);
	display.display();
	delay(1000);
	display.clearDisplay();
	display.display();
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
		static int indicator;
		int readvalueU = readADC(U_CHANNEL);
		float voltage = (readvalueU - calibrationU)*0.008333;

		int resistance = readADC(R_CHANNEL);

		int readvalueI = readADC(I_CHANNEL);
		float current = (readvalueI - calibrationI)*0.04941;
		
		StaticJsonDocument<256> json_doc;
		JsonObject json_data = json_doc.to<JsonObject>();

		json_data["sensor"]   = "mmclick";
		json_data["uptime"]   = millis();
		json_data["v_c0"]     = voltage;
		json_data["i_c0"]     = current;
		json_data["r_c0"]     = resistance;

		json_send(mqtt_sensor_topic.c_str(), json_data);

		// text display tests
		display.clearDisplay();
		display.setTextSize(1);
		display.setTextColor(SSD1306_WHITE);

		display.setCursor(0,0);
		display.print("U=");
		display.print(voltage, 4);
		display.print("V");

		display.setCursor(0,8);
		display.print("I=");
		display.print(current, 4);
		display.print("A");

		display.setCursor(0,16);
		display.print("R=");
		display.print(resistance, 4);
		display.print("O");


		// Display spinning indicator - nonsense
		display.setCursor(0,24);
		if (indicator % 4 == 0) {
			display.print("|");
			display.print("/");
			display.print("-");
			display.print("\\");
		} else if (indicator % 4 == 1) {
			display.print("/");
			display.print("-");
			display.print("\\");
			display.print("|");
		} else if (indicator % 4 == 2) {
			display.print("-");
			display.print("\\");
			display.print("|");
			display.print("/");
		} else if (indicator % 4 == 3) {
			display.print("\\");
			display.print("|");
			display.print("/");
			display.print("-");
		}
		indicator++;

		display.display();

		last_value_sent = millis();
		//     readvalueI = readADC(1);
		// delay(50);
		
		// delay(50);
		// readvalueR = readADC(3);
		// delay(50);
		// readvalueC = readADC(4);

		// current = (readvalueI - calibrationI)*0.04941;
		// voltage = (readvalueU - calibrationU)*0.008333;           
		// //capacitance = ((64285 / (readvalueC/2*4)) - calibrationC)*2; //  [nF] - MicroElectronica Bibiliothek
		// // capacitance = 10000 / (3.07137 + 0.166861 * readvalueC);    // [nF] - entire Range
		// capacitance = 10000 / (3.51694 + 0.157972 * readvalueC);    // [nF] - better Fit for smaller Cs
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

#if 0
#ifdef ESP32
	espClient.setCACert(test_root_ca);
#else
	espClient.setInsecure();
#endif
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
	if (mqtt_client.connect(mqtt_client_id.c_str(), CONFIG_MQTT_BROKER_USER, CONFIG_MQTT_BROKER_PASSWORD, mqtt_beacon_topic.c_str(), 0, false, mqtt_last_will_buffer)) {
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

		if (++connection_tries >= 20) {
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