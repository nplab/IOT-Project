from umqtt.simple import MQTTClient
import network
import time
import esp
import machine
import ubinascii
import ujson
import utime
import uos

WIFI_SSID       = 'WIFI_NAME'
WIFI_PASSWORD   = 'WIFI_PSK'
MQTT_SERVER     = '10.42.10.86'

# We take the WIFI MAC as the device ID
device_id = ubinascii.hexlify(network.WLAN().config('mac'),':').decode()

def wifi_connect():
    wifi = network.WLAN(network.STA_IF)

    if  wifi.isconnected():
        return

    print('connecting to network ', end = '')
    wifi.active(True)
    wifi.connect(WIFI_SSID, WIFI_PASSWORD)
    while not wifi.isconnected():
        pass
    print(' network config:', wifi.ifconfig())


def mqtt_send_beacon():
    # Send beacon to 'beacon' topic, it's used in the IOT dashboard
    beacon = {}
    beacon['id']    = device_id
    beacon['type']  = 1
    beacon['device']= uos.uname()[0] + '-dummy'
    beacon['desc']  = network.WLAN().ifconfig()[0] + ' | uPython test'
    beacon['rssi']  = network.WLAN().status('rssi')
    beacon['uptime']= utime.ticks_ms()

    beacon_encoded = ujson.dumps(beacon)
    mqtt_send_data('beacon', beacon_encoded)


def mqtt_send_dummy_sensor():
    # Send dummy sensor data
    sensor_value = {}
    sensor_value['sensor']   = "dummy0815"
    sensor_value['temp_c']   = 35

    sensor_value_encoded = ujson.dumps(sensor_value)
    topic = "sensor/" + device_id
    mqtt_send_data(topic, sensor_value_encoded)

def mqtt_send_data(topic, message):
    mqtt = MQTTClient(device_id, MQTT_SERVER)
    mqtt.connect()
    mqtt.publish(topic, message)
    mqtt.disconnect()
    print('[MQTT][O] ' + topic + ' -> ' + message)


if __name__ == '__main__':
    #esp.osdebug(None)
    #print('Debug output disabled\n')

    # Status LED at PIN 2
    led = machine.Pin(2, machine.Pin.OUT)

    while True:
        # Maintain WIFI connection
        wifi_connect()

        mqtt_send_beacon()
        mqtt_send_dummy_sensor()

        # Toggle LED and sleep for 500
        led.value(not led.value())
        time.sleep_ms(500)


