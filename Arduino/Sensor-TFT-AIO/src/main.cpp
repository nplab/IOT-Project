///////////////////////// COMMON HEADER BEGIN
#define CONFIG_WIFI_NAME "WIFI_NETWORK"
#define CONFIG_WIFI_PSK "WIFI_PSK"
#define CONFIG_MQTT_BROKER_ADDRESS "iot.fh-muenster.de"

#define DEBUG_OUTPUT
#define MQTT_OUTPUT

//#define TLS_ENABLED

#define OTA_ENABLED
//#define OTA_PASSWORD "bruttonetto"

//#define HMAC_ENABLED
#define HMAC_KEY "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"

#define BEACON_INTERVAL 5000

//#define MQTT_MAX_PACKET_SIZE 512 << BE AWARE OF THIS!!

#ifndef DEVICE_TYPE
#define DEVICE_TYPE "GENERIC-ESP"
#endif

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

#ifdef ESP32
#include <WiFi.h>
#else // ESP32
#include <ESP8266WiFi.h>
#endif // ESP32

#ifdef TLS_ENABLED
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
#endif

#ifndef ESP32
X509List cert(test_root_ca);
#endif

// Buffer size used at various places
#define BUFFER_SIZE 512

String mqtt_client_id;
String mqtt_sensor_topic;
String mqtt_beacon_topic;
char mqtt_last_will_buffer[BUFFER_SIZE];

// WiFi / MQTT
// WiFi / MQTT
#ifdef TLS_ENABLED
#define MQTT_PORT 8883
WiFiClientSecure espClient;
#else
#define MQTT_PORT 1883
WiFiClient espClient;
#endif
PubSubClient mqtt_client(espClient);

static bool common_setup(void);
static bool common_loop(void);
static void beacon_send(void);
static bool mqtt_connect(void);
static bool wifi_connect(void);
static bool json_send(const char *mqtt_topic, JsonObject json_data);
static int json_serialize(JsonObject json_data, char *buffer, size_t buffer_size);
///////////////////////// COMMON HEADER END

#define SENSOR_INTERVAL 1000
#define DISPLAY_INTERVAL 5000

#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_BME680.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MMA8451.h>
#include <DHT.h>

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


#define TFT_FAILURE_WIFI 1
#define TFT_FAILURE_MQTT 2
#endif // TFT_OUTPUT

#ifdef EINK_OUTPUT
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
#include GxEPD_BitmapExamples

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 25, /*RST=*/ 15); // arbitrary selection of 17, 16
GxEPD_Class eink(io, /*RST=*/ 15, /*BUSY=*/ 4); // arbitrary selection of (16), 4

#endif

#if defined(TFT_OUTPUT) || defined(EINK_OUTPUT)
static void display_print_weather(double temperature, double humidity, double altitude);
static void display_print_status(void);
#endif

bool bme_present    = false;
bool bmp_present    = false;
bool sht_present    = false;
bool tcs_present    = false;
bool mma_present    = false;
bool dht_present    = false;

// Sensors
Adafruit_BME680 bme; // I2C
Adafruit_BMP280 bmp; // I2C
Adafruit_SHT31 sht31   = Adafruit_SHT31();
Adafruit_TCS34725 tcs  = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Adafruit_MMA8451 mma   = Adafruit_MMA8451();

#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

static void bme_setup(void);
static void bmp_setup(void);
static void bmp_send(void);
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
		Serial.println(F("SHT31 detected"));
	}

	// Initialize BME
	if (bme.begin()) {
		bme_present = true;
		bme_setup();
		Serial.println(F("BME680 detected"));
	}

	// Initialize BME
	if (bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
		bmp_present = true;
		bmp_setup();
		Serial.println(F("BMP280 detected"));
	}

	// Initialize TCS
	if (tcs.begin()) {
		tcs_present = true;
		Serial.println(F("TCS46725 detected"));
	}

	// Initialize MMA
	if (mma.begin()) {
		mma_present = true;
		mma.setRange(MMA8451_RANGE_4_G);
		Serial.println(F("MMA8451 detected"));
	}

	// Initialize DHTXX
	dht.begin();
	delay(1000);
	if (dht.read()) {
		dht_present = true;
		Serial.println(F("DHTxx detected"));
	}

#ifdef TFT_OUTPUT
	// Use this initializer if using a 1.8" TFT screen:
	tft.initR();      // Init ST7735S chip, black tab
	tft.fillScreen(ST7735_BLACK);
#endif // TFT_OUTPUT

#ifdef EINK_OUTPUT
	eink.init(115200); // enable diagnostic output on Serial
#endif // TFT_OUTPUT

	delay(500);
}

void loop(void) {
	static unsigned long last_value_sent;

	if (!common_loop()) {
		// Something went wrong, skip further execution
		return;
	}

	// Send sensor values every SENSOR_INTERVAL
	if (last_value_sent == 0 || SENSOR_INTERVAL == 0 || (millis() - last_value_sent) >= SENSOR_INTERVAL || last_value_sent > millis()) {
		last_value_sent = millis();

		if (bme_present) {
			bme_send();
		}

		if (bmp_present) {
			bmp_send();
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

#if defined(TFT_OUTPUT) || defined(EINK_OUTPUT)
	static unsigned long last_display_change = 0;
	static uint8_t display_page = 0;
	if (last_display_change == 0 || (millis() - last_display_change) > DISPLAY_INTERVAL) {
#ifdef DEBUG_OUTPUT
		Serial.println(F("Changing Display..."));
#endif
		last_display_change = millis();
		display_page = (display_page + 1 ) % 2;
		if (display_page == 0 && bme_present) {
			display_print_weather(bme.temperature, bme.humidity, bme.readAltitude(SEALEVELPRESSURE_HPA));
		} else if (display_page == 0 && sht_present)  {
			display_print_weather(sht31.readTemperature(), sht31.readHumidity(), 0);
		} else {
			display_print_status();
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

static void bmp_setup(void) {
	bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
					Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
					Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
					Adafruit_BMP280::FILTER_X16,      /* Filtering. */
					Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}

static void bmp_send(void) {

	StaticJsonDocument<512> json_doc;
	JsonObject json_data = json_doc.to<JsonObject>();

	json_data["sensor"] = "bmp680";
	json_data["temp"] = bmp.readTemperature();
	json_data["press"] = bme.readPressure();

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
	int x_offset        = 5;

	tft.setTextWrap(false);
	tft.setTextSize(3);
	//tft.fillScreen(ST7735_BLACK);
	
	tft.fillRect(0, 0 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_RED);
	tft.fillRect(0, 1 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_BLUE);
	tft.fillRect(0, 2 * tft.height() / 3, tft.width(), tft.height() / 3, ST7735_BLACK);

	tft.drawFastHLine(0, 1 * tft.height() / 3, tft.width(), ST7735_WHITE);
	tft.drawFastHLine(0, 2 * tft.height() / 3, tft.width(), ST7735_WHITE);

	tft.setTextColor(ST7735_WHITE);
	tft.setCursor(x_offset, 0 * tft.height() / 3 + 1 * y_offset);
	tft.println(" Temp");
	tft.setCursor(x_offset, tft.getCursorY());
	tft.print(temperature, 1);
	tft.println(" C");

	tft.setCursor(x_offset, 1 * tft.height() / 3 + 2 * y_offset);
	tft.setTextColor(ST7735_WHITE);
	tft.println(" Humi");
	tft.setCursor(x_offset, tft.getCursorY());
	tft.print(humidity, 1);
	tft.println(" %");

	tft.setCursor(x_offset, 2 * tft.height() / 3 + 3 * y_offset);
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

#ifdef EINK_OUTPUT
static void eink_print_weather(double temperature, double humidity, double altitude) {

	int y_offset        = 25;
	int x_offset        = 5;
	eink.setCursor(0, 0);


	eink.fillScreen(GxEPD_WHITE);
	eink.setTextWrap(false);
	eink.setTextSize(6);

	eink.drawFastHLine(0, 1 * eink.height() / 3, eink.width(), GxEPD_BLACK);
	eink.drawFastHLine(0, 2 * eink.height() / 3, eink.width(), GxEPD_BLACK);

	eink.setTextColor(GxEPD_BLACK);
	eink.setCursor(x_offset, 0 * eink.height() / 3 + 1 * y_offset);
	eink.print("Temp ");
	eink.print(temperature, 1);
	eink.println(" C");

	eink.setCursor(x_offset, 1 * eink.height() / 3 + 1 * y_offset);
	eink.setTextColor(GxEPD_BLACK);
	eink.print("Humi ");
	eink.print(humidity, 1);
	eink.println(" %");

	eink.setCursor(x_offset, 2 * eink.height() / 3 + 1 * y_offset);
	eink.setTextColor(GxEPD_BLACK);
	eink.print("Alti ");
	eink.print(altitude, 0);
	eink.println(" M");

	eink.update();
}

static void eink_print_status(void) {
	eink.fillScreen(GxEPD_WHITE);
	eink.setTextColor(GxEPD_BLACK);
	eink.setTextSize(3);
	eink.setCursor(0, 20);

	eink.print("RSSI ");
	eink.println(WiFi.RSSI());
	eink.println("");
	eink.print("MAC ");
	eink.println(String(WiFi.macAddress()));
	eink.println("");
	eink.print("IP ");
	eink.println(WiFi.localIP());
	eink.println("");
	eink.print("SM ");
	eink.println(WiFi.subnetMask());
	eink.println("");
	eink.print("GW ");
	eink.println(WiFi.gatewayIP());
	eink.println("");
	eink.print("MQTT status ");
	if (mqtt_client.state() == MQTT_CONNECTED) {
		eink.println("Connected!");
	} else {
		eink.println(mqtt_client.state());
	}
	eink.update();
}
#endif

#if defined(TFT_OUTPUT) || defined(EINK_OUTPUT)
static void display_print_weather(double temperature, double humidity, double altitude) {
#ifdef TFT_OUTPUT
	tft_print_weather(temperature, humidity, altitude);
#endif
#ifdef EINK_OUTPUT
	eink_print_weather(temperature, humidity, altitude);
#endif
}

static void display_print_status(void) {
#ifdef TFT_OUTPUT
	tft_print_status();
#endif
#ifdef EINK_OUTPUT
	eink_print_status();
#endif
}

#endif


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

#ifdef TLS_ENABLED
#ifdef ESP32
	espClient.setCACert(test_root_ca);
#else // ESP32
	espClient.setTrustAnchors(&cert);
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

#ifdef OTA_ENABLED
#ifdef OTA_PASSWORD
	ArduinoOTA.setPassword(OTA_PASSWORD);
#else // OTA_PASSWORD
	ArduinoOTA.setPassword(mqtt_client_id.c_str());
#endif // OTA_PASSWORD
#ifdef ESP32
	ArduinoOTA.setMdnsEnabled(false);
#endif // ESP32
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
	static unsigned long last_beacon_sent;
	static unsigned long last_led_toggle;
#ifdef DEBUG_OUTPUT
	static long hz_counter;
	static unsigned long hz_counter_delta;
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
	if (last_beacon_sent == 0 || BEACON_INTERVAL == 0 || (millis() - last_beacon_sent) >= BEACON_INTERVAL || last_beacon_sent > millis()) {
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

#ifdef DEV_DESC
	snprintf(desc, sizeof(desc), "%s", DEV_DESC);
#else
	snprintf(desc, sizeof(desc), "%s|%s", WiFi.localIP().toString().c_str(), __DATE__);
#endif

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
	Serial.print(":");
	Serial.print(MQTT_PORT);
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