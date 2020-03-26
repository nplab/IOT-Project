#!/usr/bin/env python

import paho.mqtt.client as mqtt
import json
import threading
import time
import hmac
import hashlib
import base64
import binascii
import sys


messages_total          = 0
messages_total_last     = 0
time_total_last         = time.time()
stats_period            = 10
check_hmac              = False

hmac_secret     = "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"

class StatsThread(threading.Thread):
    def __init__(self, event):
        threading.Thread.__init__(self)
        self.stopped = event

    def run(self):
        while not self.stopped.wait(stats_period):
            global messages_total
            global messages_total_last
            global time_total_last


            messages_delta  = messages_total - messages_total_last
            time_delta      = int(round((time.time() - time_total_last) * 1000))

            print("{} s - {} msgs - {} hz".format(time_delta / 1000, messages_delta, messages_delta / time_delta * 1000))

            messages_total_last = messages_total
            time_total_last = time.time()



# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and reconnect then subscriptions will be renewed.
    #client.subscribe("sensor/dummy-sender")
    #client.subscribe("sensor/80:7D:3A:B7:4F:18")
    client.subscribe("sensor/3C:71:BF:AA:7B:18")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    #print(msg.topic+" "+str(msg.payload))
    global messages_total
    messages_total = messages_total + 1

    payload_obj = json.loads(msg.payload)

    if not "hmac" in payload_obj and check_hmac:
        print("No HMAC ...")
        sys.exit()
        return


    payload_hmac = base64.b64decode(payload_obj["hmac"])

    payload_obj["hmac"] = ""
    data = json.dumps(payload_obj, separators=(',', ':'))

    h = hmac.new(key = hmac_secret.encode('utf-8'), msg = data.encode('utf-8'), digestmod = hashlib.sha256)

    if not hmac.compare_digest(h.digest(), payload_hmac):
        print("HMAC INVALID")
        sys.exit()


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("10.42.10.86", 1883, 60)


# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.


stopFlag = threading.Event()
thread = StatsThread(stopFlag)
thread.start()
client.loop_forever()
