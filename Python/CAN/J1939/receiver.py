#!/usr/bin/env python3

import logging
import time
import can
import j1939
import json
import pprint
import paho.mqtt.client as mqtt
import json
import random
import math
import time

pgnDict = {}
unknownPgnList = []

dashboardDict = {}

config_mqtt_broker_ip = "iot.fh-muenster.de"
config_mqtt_client_id = "can-sender"
config_mqtt_topic     = "sensor/" + config_mqtt_client_id

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

def collect_dashboard_info(pgn, offset, value):
    if pgn == 61444:
        if offset == 3:
            # Engine Speed
            dashboardDict['rpm'] = round(value)
        elif offset == 2:
            # Percent Torque
            dashboardDict['torque'] = round(value)
    elif pgn == 65272:
        if offset == 4:
            # Transmission Oil Temperature
            dashboardDict['trans_oil_temp'] = round(value, 1)
    elif pgn == 65263:
        if offset == 3:
            # Engine Oil Pressure
            dashboardDict['eng_oil_press'] = round(value)
    elif pgn == 65262:
        if offset == 0:
            # Engine Coolant Temperature
            dashboardDict['eng_coolant_temp'] = round(value, 1)
        elif offset == 2:
            # Engine Oil Temperature
            dashboardDict['eng_oil_temp'] = round(value, 1)
    else:
        return False

    return True

def on_message(pgn, data):
    """Receive incoming messages from the bus

    :param int pgn:
        Parameter Group Number of the message
    :param bytearray data:
        Data of the PDU
    """
    #print("PGN {} length {}".format(pgn, len(data)))

    if pgn in pgnDict:
        for spn in pgnDict[pgn]['spns']:
            if spn['type'] == 0:
                byteOffset = spn['offset']
                byteSize = spn['byteSize']

                rawValue = int.from_bytes(data[byteOffset:(byteOffset + byteSize)], "little")
                value = rawValue * spn['formatGain'] + spn['formatOffset']

                if not (collect_dashboard_info(pgn, byteOffset, value)):
                    #print("[{}] {} : {}".format(pgn, spn['name'], value))
                    pass
    else:
        if not pgn in unknownPgnList:
            unknownPgnList.append(pgn)
            #print("unknown: {}".format(pgn))

def main():
    print("Initializing")

    mqtt_c = mqtt.Client(config_mqtt_client_id)
    mqtt_c.on_connect = on_connect

    mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)
    mqtt_c.loop_start()

    logging.getLogger('j1939').setLevel(logging.DEBUG)
    logging.getLogger('can').setLevel(logging.DEBUG)

    last_announce   = time.time()
    start_time      = time.time()

    with open('frames.json', 'r') as jsonfile:
        pgnList = json.load(jsonfile)
        for pgnDef in pgnList:
            pgnDict[pgnDef['pgn']] = pgnDef

    print("{} PGNs loaded".format(len(pgnDict)))

    # create the ElectronicControlUnit (one ECU can hold multiple ControllerApplications)
    ecu = j1939.ElectronicControlUnit()

    # Connect to the CAN bus
    # Arguments are passed to python-can's can.interface.Bus() constructor
    # (see https://python-can.readthedocs.io/en/stable/bus.html).
    ecu.connect(bustype='socketcan', channel='can0')
    # ecu.connect(bustype='kvaser', channel=0, bitrate=250000)
    # ecu.connect(bustype='pcan', channel='PCAN_USBBUS1', bitrate=250000)
    # ecu.connect(bustype='ixxat', channel=0, bitrate=250000)
    # ecu.connect(bustype='vector', app_name='CANalyzer', channel=0, bitrate=250000)
    # ecu.connect(bustype='nican', channel='CAN0', bitrate=250000)

    # subscribe to all (global) messages on the bus
    ecu.subscribe(on_message)

    while 1:
        print(dashboardDict)
        mqtt_c.publish(config_mqtt_topic, json.dumps(dashboardDict));

        if (time.time() - last_announce) > 5:
            announce = {
                "id"        : config_mqtt_client_id,
                "type"      : 1,
                "device"    : "can-sender",
                "rssi"      : 0,
                "uptime"    : time.time() - start_time,
                "desc"      : "reading-can-data"
            }

            mqtt_c.publish("beacon", json.dumps(announce))
            print(json.dumps(announce))
            last_announce = time.time()
        time.sleep(0.1)

    print("Deinitializing")
    ecu.disconnect()

if __name__ == '__main__':
    print("start")
    main()
