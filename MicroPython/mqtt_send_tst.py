import network as net
from umqtt.simple import MQTTClient
# WLAN
ESSID = 'essid'
PASS = 'passwd'
# MQTT Broker Address
SERVER = '192.168.1.126'

# Test reception with:
# mosquitto_sub -t foo_topic

def send_data(server=SERVER,
              topic=b'foo_topic',
              data=b'hello'):
    client = MQTTClient("umqtt_client", server)
    client.connect()
    client.publish(topic, data)
    c.disconnect()

def connect_to_wlan():
    wlan = net.WLAN(net.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print('Connecting to network...')
        wlan.connect(ESSID, PASS)
        while not wlan.isconnected():
            pass
    print('Network config:', wlan.ifconfig())

    
connect_to_wlan()
send_data(topic='foo_topic', data='Hello')
