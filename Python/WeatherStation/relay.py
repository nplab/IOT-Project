#!/usr/bin/env python3

import socket
import paho.mqtt.client as mqtt
import json
import time


TCP_IP = "10.211.42.10"
TCP_PORT = 14001
BUFFER_SIZE = 1024
length = None
buffer = ""

config_mqtt_broker_ip = "10.42.10.86"
config_mqtt_client_id = "weather-station-steinfurt";
config_mqtt_topic     = "sensor/" + config_mqtt_client_id

# The callback for when the client receives a CONNACK response from the server.
def on_mqtt_connect(client, userdata, flags, rc):
	print("Connected with result code " + str(rc))

mqtt_c = mqtt.Client(config_mqtt_client_id)
mqtt_c.on_connect = on_mqtt_connect

mqtt_c.connect(config_mqtt_broker_ip, 1883, 60)
mqtt_c.loop_start()


last_announce   = time.time()
start_time      = time.time()

def connect_station():
	global s
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect((TCP_IP, TCP_PORT))

connect_station()
reset_counter = 0

while True:
	reset_counter = reset_counter + 1
	if reset_counter > 1000:
		s.close()
		connect_station()
		reset_counter = 0
		print("Reconnect!")


	data = s.recv(BUFFER_SIZE)
	if len(data) == 0:
		print("Disconnected! Reconnecting...")
		connect_station()
		continue

	buffer += data.decode()

	#print(buffer)

	while True:
		if '\r\n' not in buffer:
			break
		# remove the message from the front of buffer
		# leave any remaining bytes in the buffer!
		message, ignored, buffer = buffer.partition('\r\n')
		message_length = len(message)

		message = message.split()
		print("received data: {}".format(message))
		mqtt_c.publish(config_mqtt_topic, json.dumps(message));

	if (time.time() - last_announce) > 5:
		announce = {
			"id"        : config_mqtt_client_id,
			"type"      : 1,
			"device"    : "serial-relay",
			"rssi"      : 0,
			"uptime"    : time.time() - start_time,
			"desc"      : "Weather station Steinfurt"
		}

		mqtt_c.publish("beacon", json.dumps(announce))
		print(json.dumps(announce))
		last_announce = time.time()

s.close()
