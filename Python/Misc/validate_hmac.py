#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import json
import threading
import time
import hmac
import hashlib
import base64
import binascii
import platform
print(platform.python_version())

json_string = '{"id":"3C:71:BF:AB:33:58","type":1,"device":"MMA8451-ESP32","desc":"10.11.12.100|107","rssi":-54,"uptime":158678,"bssid":"F0:B0:14:75:82:EA","hmac":"hOiS/fsv4/t985QesIDZZHtAyusLHCS2yFTN9uyzdtY="}'
json_obj = json.loads(json_string)

arr = bytes(json_string, 'utf-8')
print(arr)

json_hmac = json_obj['hmac']
json_obj['hmac'] = ''

json_to_hash = json.dumps(json_obj, separators=(',', ':'))
print(json_to_hash)
print(len(json_to_hash))

secret = "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"
h = hmac.new(key = secret.encode('utf-8'), msg = json_to_hash.encode('utf-8'), digestmod = hashlib.sha256)

print(binascii.hexlify(bytearray(base64.b64decode(json_hmac))))
print(h.hexdigest())