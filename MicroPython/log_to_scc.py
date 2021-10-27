import network as net
from umqtt.simple import MQTTClient
from machine import Pin, I2C, unique_id
from binascii import hexlify
import sys
import time
# Lib for the OLED display:
import ssd1306

# WLAN
ESSID = 'essid' 
PASSWD = 'passwd' 

# MQTT Broker Address
MQTT_SERVER = 'iot.fh-muenster.de'
MQTT_USER = 'MQTT_USER'
MQTT_PASSWORD = 'MQTT_PASSWORD'
# MQTT Topic
TOPIC = 'egu_ven'

# Standard address of the HDC sensor:
HDC_ADDR = 64
# Standard address of the OLED display:
OLED_ADDR = 61

# Registers storing temperature and humidity (data sheet):
TMP_REG = 0x00
HUM_REG = 0x01

# Connection of the I2C bus:
# scl (yellow cable, clock)
# sda (green cable, data)
SCL = 32 # pin number
SDA = 14 # pin number

# Measurment frequency in seconds:
DELAY = 60

def init_i2c():

    """Init the I2C bus."""
    
    i2c = I2C(scl=Pin(SCL, Pin.OUT, Pin.PULL_UP),
              sda=Pin(SDA, Pin.OUT, Pin.PULL_UP),
              freq=100000)
    
    return(i2c)


def init_display():
    
    """ Init OLED display."""

    if OLED_ADDR not in i2c.scan():
        print('Display not found at 7 bit address 61.')
        print('Found devices at: ' + str(i2c.scan()) )
        sys.exit()
    else:
        display = ssd1306.SSD1306_I2C(
            128, 64, i2c, addr=OLED_ADDR)

    return(display)


def init_wlan():
    """ Init WLAN.

    Returns: A net.WLAN object.
    """
    return(net.WLAN(net.STA_IF))


def check_sensors():

    """Check if sensors are present at the expected addresses."""
    
    if HDC_ADDR not in i2c.scan():
        print('HDC 1080 not found at 7 bit address 64.')
        print('Found devices at: ' + str(i2c.scan()) )
        sys.exit()
        
        
def connect_to_wlan(wlan, essid, passwd):

    """Connect to Wi-Fi network.

    Arguments:
    wlan   -- A net.WLAN object.
    essid  -- Name the Access Point.
    passwd -- Password of the network.
    """

    wlan.active(True)
    if not wlan.isconnected():
        print('Connecting to network...')
        wlan.connect(essid, passwd)
        while not wlan.isconnected():
            pass
    print('Network config:', wlan.ifconfig())


def send_data(topic=b'foo_topic',
              data=b'Hello!'):

    """Send data via MQTT.

    Keyword arguments:
    topic  -- The MQTT topic.
    data   -- The data.

    Test connection on the broker:
    mosquitto_sub -t foo_topic
    """
    try:
        client = MQTTClient("umqtt_client", MQTT_SERVER, user=MQTT_USER, password=MQTT_PASSWORD)
        client.connect()
        client.publish(topic, data)
        client.disconnect()
    except e:
        print('Data logging via MQTT failed:')
        print(e)


def get_temperature():

    ''' Read temperature sensor.

    Returns: Temperature in degrees Celsius.
    '''

    # Point to temperature register:
    i2c.writeto(HDC_ADDR, bytearray([TMP_REG]))
    # Hold on (conversion time, data sheet):
    time.sleep(0.0635)
    # Read two bytes:
    tmp_bytes = i2c.readfrom(HDC_ADDR, 2)
    # Convert to temperature (formula from data sheet):
    tmp_deg_c = (int.from_bytes(tmp_bytes, 'big') / 2**16) * 165 - 40

    return(tmp_deg_c)


def get_humidity():

    ''' Read humidity sensor.

    Returns: Percent relative Humidity.
    '''

    # Point to humidity register:
    i2c.writeto(HDC_ADDR, bytearray([HUM_REG]))
    # Hold on (conversion time, data sheet):
    time.sleep(0.065)
    # Read two bytes:
    hum_bytes = i2c.readfrom(HDC_ADDR, 2)
    # Convert to percent relative humidity (formula from data sheet):
    hum_p_rel = (int.from_bytes(hum_bytes, 'big') / 2**16) * 100

    return(hum_p_rel)


def pretty_hex_addr(s):
    """
    Reformat hexadecimal address.

    Arguments:
    s -- Bytes representing a hexadecimal address.

    Returns: Human readable hexadecimal address.
    """
    s = str(hexlify(s))[2:-1]
    return( '{}:{}:{}:{}:{}:{}'.format(
        s[0:2], s[2:4], s[4:6], s[6:8], s[8:10], s[10:12]) )


def log_data(wlan):

    """Display data and send it to the mqtt broker.

    Arguments:
    wlan -- A net.WLAN object with an IoT-Broker waiting for data.
    """
    
    while True:
        # get the masurement data
        tmp_deg_c = get_temperature()
        hum_p_rel = get_humidity()
        # Print data on display:
        display.fill(0)
        display.text('{:.2f} deg C'.format(tmp_deg_c), 0, 0, 1)
        display.text('{:.2f} percent'.format(hum_p_rel), 0, 20, 1)
        display.text('rel. humidity', 0, 30, 1)
        display.show()
        # check WLAN
        if not wlan.isconnected():
            connect_to_wlan(wlan, ESSID, PASSWD)
        # send payload
        send_data(topic=TOPIC,
                  data='{:.2f} {:.2f}'.format(tmp_deg_c, hum_p_rel))
        # say hello to the IoT
        send_data(topic='beacon/egu_ven',
                  data='{' +
                  '"id":"{}",'.format(pretty_hex(unique_id())) +
                  '"type":1,' +
                  '"device":"ESP32 Dev V4 EGU VEN",' +
                  '"desc":"{}",'.format(wlan.ifconfig()[0])  +
                  '"rssi":{},'.format(wlan.status('rssi')) +
                  '"uptime":{},'.format(time.ticks_ms()) +
                  '}')
        time.sleep(DELAY)
        

if __name__ == "__main__":

    """Log sensor data of an ESP32 board via Wi-Fi and MQTT."""
    i2c = init_i2c()
    display = init_display()
    check_sensors()
    wlan = init_wlan()
    log_data(wlan)
