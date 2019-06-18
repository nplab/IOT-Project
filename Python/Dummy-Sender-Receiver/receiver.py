import paho.mqtt.client as mqtt
import json
import random
import math
import time

config_mqtt_broker_ip = "10.42.10.86"
config_mqtt_client_id = "dummy-receiver-" + str(random.randint(1000, 9999));
config_mqtt_topic     = "sensor/#"


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

mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)

mqtt_c.loop_forever();
