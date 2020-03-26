# IOT Dashboard
This directory contains an HTML5/CSS/JS MQTT dashboard and multiple examples how to display sensor information.
The MQTT messages are transmitted via websockets.

The code is not production ready due to the lack of XSS/HTML injection protection.

## JSON Formats
The dashboard uses two message types.
The beacon message indices an active IOT device.
If the last will message is received, the device is getting marked as inactive.

### Beacon Message
```
{
    "id":"1A:2B:3C:4D:5E:6A",
    "type":1,
    "device":"esp32-tft",
    "desc":"My lovely ESP32 with TFT",
    "rssi":-50,
    "uptime":654321,
    "bssid":"6A:5E:4D:3C:2B:1A"
}
```

| Key    | Value                      | Description                                                         |
|--------|----------------------------|---------------------------------------------------------------------|
| id     | "1A:2B:3C:4D:5E:6A"        | (String) Unique device identifier, e.g. MAC                         |
| type   | 1                          | (Int) Message type (1 = beacon / 2 = last will)                     |
| device | "esp32-tft"                | (String) Device type                                                |
| desc   | "My lovely ESP32 with TFT" | (String) Device description                                         |
| rssi   | -50                        | (Int) Received Signal Strength Indicator (RSSI), 0 if not available |
| uptime | 654321                     | (Int) Uptime in milliseconds                                        |
| bssid  | "6A:5E:4D:3C:2B:1A"        | (String) Basic Service Set Identification (BSSID)                   |

### Last Will Message
```
{
    "id":"1A:2B:3C:4D:5E:6A",
    "type":2
}
```

| Key    | Value                      | Description                                                         |
|--------|----------------------------|---------------------------------------------------------------------|
| id     | "1A:2B:3C:4D:5E:6A"        | (String) Unique device identifier, e.g. MAC                         |
| type   | 2                          | (Int) Message type (1 = beacon / 2 = last will)                     |
