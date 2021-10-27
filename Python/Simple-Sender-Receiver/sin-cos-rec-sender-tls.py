import paho.mqtt.client as mqtt
import json
import random
import math
import time

config_mqtt_broker_ip = "iot.fh-muenster.de"
config_mqtt_client_id = "dummy-sender-" + str(random.randint(1000, 9999));
config_mqtt_send_rate = 10 # Hz

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

mqtt_c = mqtt.Client(config_mqtt_client_id, clean_session=True)
mqtt_c.on_connect = on_connect
#mqtt_c.tls_set(ca_certs="ca.pem")

mqtt_meta_last_will = {'type' : 2}
mqtt_c.will_set("meta/" + config_mqtt_client_id, None, qos=0, retain=True)

mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)
mqtt_c.loop_start()

counter = 0
divider = 100

last_announce   = time.time()
start_time      = time.time()

mqtt_meta = {
    'type'              : 1,
    'name'              : 'sin-cos-rec-sender-tls.py',
    'desc'              : 'Dummy sender for sin, cos and rec to demonstrate the capabilities',
    'payloadType'       : 'json',
    'payloadStructure'  : [
        {
            'name'          : 'index',
            'description'   : 'increasing counter, rollover possible',
        }, {
            'name'          : 'sin',
            'description'   : 'random sinus values',
        }, {
            'name'          : 'cos',
            'description'   : 'random cosinus values',
        }, {
            'name'          : 'rec',
            'description'   : 'math.floor((divider - counter) / (divider / 2))',
        }
    ]
}

mqtt_c.publish("meta/" + config_mqtt_client_id, json.dumps(mqtt_meta), qos=0, retain=True)

while True:

    mqtt_data = [
        counter,
        math.sin(math.pi * (counter / divider)),
        math.cos(math.pi * (counter / divider)),
        math.floor((divider - counter) / (divider / 2))
    ]

    mqtt_c.publish("sensor/" + config_mqtt_client_id, json.dumps(mqtt_data));
    print(json.dumps(mqtt_data))
    counter = (counter + 1) % divider

    if (time.time() - last_announce) > 1:
        mqtt_beacon = {
            "type"      : 3,
            "rssi"      : 0,
            "uptime"    : time.time() - start_time,
        }

        mqtt_c.publish("beacon/" + config_mqtt_client_id, json.dumps(mqtt_beacon))
        print(json.dumps(mqtt_beacon))
        last_announce = time.time()

    time.sleep(1 / config_mqtt_send_rate)
