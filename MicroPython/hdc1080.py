# Test hdc1080 sensor (humidity and temperature) and display data on OLED

from machine import Pin, I2C
import sys
import time
# Lib for the OLED display:
import ssd1306

# Data sheet of the sensor:
# https://www.ti.com/lit/ds/symlink/hdc1080.pdf

# Standard address of the HDC sensor on ESP32:
HDC_ADDR = 64
# Standard address of the OLED Display:
OLED_ADDR = 61

# Registers storing temperature and humidity (data sheet):
TMP_REG = 0x00
HUM_REG = 0x01

# Connect
# scl (yellow cable, clock) and
# sda (green cable, data) to any
# two output pins and set the constants accordingly:
SCL = 32
SDA = 14

# Create the bus
i2c = I2C(scl=Pin(SCL, Pin.OUT, Pin.PULL_UP), sda=Pin(SDA, Pin.OUT, Pin.PULL_UP), freq=100000)

# Check wether the sensor is connected under the standard address:
if HDC_ADDR not in i2c.scan():
    print('HDC 1080 not found at 7 bit address 64.')
    print('Found devices at: ' + str(i2c.scan()) )
    sys.exit()

if OLED_ADDR not in i2c.scan():
    print('Display not found at 7 bit address 61.')
    print('Found devices at: ' + str(i2c.scan()) )
    sys.exit()
else:
    display = ssd1306.SSD1306_I2C(128, 64, i2c, addr=OLED_ADDR)

while True:
    # Point to temperature register:
    i2c.writeto(HDC_ADDR, bytearray([TMP_REG]))
    # Hold on (conversion time, data sheet):
    time.sleep(0.0635)
    # Read two bytes:
    tmp_bytes = i2c.readfrom(HDC_ADDR, 2)
    # Convert to temperature (formula from data sheet):
    tmp_deg_c = (int.from_bytes(tmp_bytes, 'big') / 2**16) * 165 - 40

    # Point to humidity register:
    i2c.writeto(HDC_ADDR, bytearray([HUM_REG]))
    # Hold on (conversion time, data sheet):
    time.sleep(0.065)
    # Read two bytes:
    hum_bytes = i2c.readfrom(HDC_ADDR, 2)
    # Convert to percent relative humidity (formula from data sheet):
    hum_p_rel = (int.from_bytes(hum_bytes, 'big') / 2**16) * 100

    # Print results:
    display.text('{:.2f} deg C'.format(tmp_deg_c), 0, 0, 1)
    display.text('{:.2f} percent'.format(hum_p_rel), 0, 20, 1)
    display.text('rel. humidity', 0, 40, 1)
    display.show()
    time.sleep(10)
    display.fill(0)
