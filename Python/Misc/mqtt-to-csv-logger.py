#!/usr/bin/env python

import paho.mqtt.client as mqtt
import json
import csv
from datetime import datetime

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("sensor/3C:71:BF:AA:7E:4C")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    #print(msg.topic+" "+str(msg.payload))
    y = json.loads(msg.payload)
    y['datetime'] = datetime.now()
    
    csv_writer.writerow([y["datetime"], y["temp"], y["humi"], y["press"]])
    print(y)
    csv_file.flush()

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("10.42.10.86", 1883, 60)

csv_file = open("log.csv", "w")
csv_writer = csv.writer(csv_file)

# Write CSV Header, If you dont need that, remove this line
csv_writer.writerow(["timestamp", "temperature", "humidity", "pressure"])


# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()