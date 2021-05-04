import paho.mqtt.client as mqtt
import json
import random
import math
import time

config_mqtt_broker_ip = "10.42.10.86"
config_mqtt_client_id = "dummy-sender-" + str(random.randint(1000, 9999));
config_mqtt_topic     = "sensor/" + config_mqtt_client_id
config_mqtt_send_rate = 10 # Hz

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

mqtt_c = mqtt.Client(config_mqtt_client_id)
mqtt_c.on_connect = on_connect

mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)
mqtt_c.loop_start()

counter = 0
divider = 100

last_announce   = time.time()
start_time      = time.time()

while True:

    data = {
        "sensor" : "dummy",
        "index"  : counter,
        "sin"    : math.sin(math.pi * (counter / divider)),
        "cos"    : math.cos(math.pi * (counter / divider)),
        "rec"    : math.floor((divider - counter) / (divider / 2))
    }

    mqtt_c.publish(config_mqtt_topic, json.dumps(data));
    print(json.dumps(data))
    counter = (counter + 1) % divider

    if (time.time() - last_announce) > 5:
        announce = {
            "id"        : config_mqtt_client_id,
            "type"      : 1,
            "device"    : "dummy-sender",
            "rssi"      : 0,
            "uptime"    : time.time() - start_time,
            "desc"      : "sin/cos/rec dummy sender"
        }

        mqtt_c.publish("beacon", json.dumps(announce))
        print(json.dumps(announce))
        last_announce = time.time()

    time.sleep(1 / config_mqtt_send_rate)
