#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import json
import random
import math
import time
import ssl

config_mqtt_broker_ip = "iot.fh-muenster.de"
config_mqtt_client_id = "dummy-receiver-" + str(random.randint(1000, 9999));
config_mqtt_topic     = "sensor/60:01:94:4A:AF:7A"

ts_last_message = int(round(time.time() * 1000))

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
	print("Connected with result code " + str(rc))
	client.subscribe(config_mqtt_topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
	print(msg.topic + " " + str(msg.payload))

mqtt_c = mqtt.Client(config_mqtt_client_id)
mqtt_c.on_connect = on_connect
mqtt_c.on_message = on_message
mqtt_c.tls_set(ca_certs="ca.pem")

#mqtt_c.tls_insecure_set(True)

mqtt_c.connect(config_mqtt_broker_ip, 8883, 60)

mqtt_c.loop_forever();
