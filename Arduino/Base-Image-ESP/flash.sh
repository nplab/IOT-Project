#!/bin/bash
#source ~/.platformio/penv/bin/activate

timeout 60 platformio run --target upload

esptool.py read_mac

