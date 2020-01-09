#!/bin/sh
ampy --port /dev/ttyUSB0 put main.py
#ampy --port /dev/ttyUSB0 reset
screen /dev/ttyUSB0 115200
