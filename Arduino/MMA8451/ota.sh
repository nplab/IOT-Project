#!/bin/bash
source ~/.platformio/penv/bin/activate

platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.28
platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.29
platformio run -e esp32-TFT-OTA  --target upload --upload-port 10.227.1.30
platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.31
platformio run -e esp32-TFT-OTA  --target upload --upload-port 10.227.1.141

