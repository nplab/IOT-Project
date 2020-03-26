from machine import Pin, I2C, ADC
from onewire import OneWire
from time import sleep
# Lib for the OLED display:
from ssd1306 import SSD1306, SSD1306_I2C
# Lib for the temperature sensor:
from ds18x20 import DS18X20

# Standard address of the OLED Display:
OLED_ADDR = 60

# For the I2C bus connect
# scl (yellow cable, clock) and
# sda (green cable, data) to any
# two output pins and set the constants accordingly:
SCL = 32
SDA = 14

# Standard address of the tempersture sensor:
T_ADDR = b'(\x87\xfdy\x97\x12\x03\xcd'

# Connect the digital temperature sensor DS18D20
PT = 12

# Connect the analog differential pressure transducer MPXV7002DP.
# Use a current-divider to map 5 V to 3.6 V:
# E.g: 1.5 kOhm between sensor and GPIO pin and 3.9 kOhm
# between GPIO pin and ground.
DP = 35

# Create the I2C bus object:
i2c = I2C(scl=Pin(SCL, Pin.OUT, Pin.PULL_UP), sda=Pin(SDA, Pin.OUT, Pin.PULL_UP), freq=100000)

# Create the one wire bus object:
ow = DS18X20(OneWire(Pin(PT, Pin.PULL_UP)))

# Create the analog digital converter object:
adc = ADC(Pin(DP))
# Attenuate 0 - 1 V range to 0 - 3.6 V range:
adc.atten(ADC.ATTN_11DB)

# Check, whether the display is connected
# on the standard address:
if OLED_ADDR not in i2c.scan():
    print('Display not found at 7 bit address {}.'.format(OLED_ADDR))
    print('Found devices at {}.'.format(i2c.scan()))
    sys.exit()
else:
    display = SSD1306_I2C(128, 64, i2c, addr=OLED_ADDR)


# Check, whether the temperature sensor is connected
# on the standard address:
if T_ADDR not in ow.scan():
    print('Sensor not found at {}.'.format(T_ADDR))
    print('Found devices at: {}.'.format(ow.scan()))
    sys.exit()
else:
    pass


def read_temperature():
    ow.convert_temp()
    return(ow.read_temp(T_ADDR))
             

def read_pressure_diff():
    d = adc.read()
    t = (d - 2036) * 2/2036
    return(t)

while True:
    temp = read_temperature()
    dp = read_pressure_diff()
    display.text('T (deg C): {:.2f}'.format(temp), 0, 0, 1)
    display.text('dp (kPa): {:.3f}'.format(dp), 0, 20, 1)
    display.show()
    sleep(1)
    display.fill(0)
