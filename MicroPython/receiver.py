import paho.mqtt.client as mqtt
import json
import random
import math
import time

SERVER = "10.42.10.86"
CLIENT_ID = "egu_ven_mqtt_client";
# all data: "sensor/#"
TOPIC = "egu_ven"


def on_connect(client, userdata, flags, rc):
    '''
    Callback function when a CONNECT response is received.
    '''
    print("Connected with result code " + str(rc))
    client.subscribe(TOPIC)


def on_message(client, userdata, msg):
    '''
    Callback function when a PUBLISH message is received.
    '''
    print(time_stamp() + " " + str(msg.payload))


def time_stamp():
    lc = time.localtime()
    return('{}-{:02}-{:02}T{:02}:{:02}:{:02}'.format(lc.tm_year,
                                                     lc.tm_mon,
                                                     lc.tm_mday,
                                                     lc.tm_hour,
                                                     lc.tm_min,
                                                     lc.tm_sec))


mqtt_c = mqtt.Client(CLIENT_ID)
mqtt_c.on_connect = on_connect
mqtt_c.on_message = on_message
mqtt_c.connect(SERVER, 1883, 60)
mqtt_c.loop_forever();
