#!/bin/bash
source ~/.platformio/penv/bin/activate

timeout 60 platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.28
timeout 60 platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.29
timeout 60 platformio run -e esp32-TFT-OTA  --target upload --upload-port 10.227.1.30
timeout 60 platformio run -e esp32-OTA      --target upload --upload-port 10.227.1.31
timeout 60 platformio run -e esp32-TFT-OTA  --target upload --upload-port 10.227.1.141

