#!/usr/bin/env python

import paho.mqtt.client as mqtt
import json
import rrdtool
import traceback

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("sensor/ESP32-3C:71:BF:AA:80:4C")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    #print(msg.topic+" "+str(msg.payload))
    data = json.loads(msg.payload)

    print(data['humi'])

    data_string = "N:" + str(data["temp"]) + ":" + str(data["humi"]) + ":" + str(data["pres"]) + ":" + str(data["gasr"])

    print(data_string)
    try:
        rrdtool.update("climate.rrd", data_string)
    except:
        print(traceback.format_exc())
        print("other error!")

    print(data)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("wks.nplab.de", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()