#!/usr/bin/env python
import paho.mqtt.client as mqtt
import json
import random
import math
import time
import board
import busio
import adafruit_ads1x15.ads1115 as ADS
from adafruit_ads1x15.analog_in import AnalogIn

config_mqtt_broker_ip = "iot.fh-muenster.de"
config_mqtt_client_id = "ads1115-" + str(random.randint(1000, 9999));
config_mqtt_topic     = "sensor/" + config_mqtt_client_id
config_mqtt_send_rate = 10 # Hz

i2c = busio.I2C(board.SCL, board.SDA)
ads = ADS.ADS1115(i2c)
diff_01 = AnalogIn(ads, ADS.P0, ADS.P1)
diff_23 = AnalogIn(ads, ADS.P2, ADS.P3)


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

mqtt_c = mqtt.Client(config_mqtt_client_id)
mqtt_c.on_connect = on_connect

mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)
mqtt_c.loop_start()

last_announce   = time.time()
start_time      = time.time()

while True:

    data = {
        "sensor" : "ads1115",
        "time"   : int(round(time.time() * 1000)),
        "diff_01": math.sin(diff_01.value),
        "diff_23": math.cos(diff_23.value),
    }

    mqtt_c.publish(config_mqtt_topic, json.dumps(data));
    print(json.dumps(data))

    if (time.time() - last_announce) > 5:
        announce = {
            "id"        : config_mqtt_client_id,
            "type"      : 1,
            "device"    : "ads1115",
            "rssi"      : 0,
            "uptime"    : time.time() - start_time,
            "desc"      : "ADS1115 differential sensor"
        }

        mqtt_c.publish("beacon", json.dumps(announce))
        print(json.dumps(announce))
        last_announce = time.time()

    if config_mqtt_send_rate > 0:
        time.sleep(1 / config_mqtt_send_rate)
