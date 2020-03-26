# Arduino Examples
These Arduino examples use the [Arduino API](https://www.arduino.cc/reference/en/) and are optimized for the [PlatformIO IDE](https://platformio.org/).

To install the PlatformIO IDE, install [Microsoft's Visual Studio Code Editor](https://code.visualstudio.com/) first.
The PlatformIO IDE is installed as an extension, as shown in the [official docs](https://platformio.org/install/ide?install=vscode).
Beginners should read and follow the [quick-start guide](https://docs.platformio.org/en/latest/ide/vscode.html#quick-start).

The examples should also work within the Arduino IDE, additional variable/define declaration may be necessary.

## Prepare you Board

* Get an ESP32/ESP8266 board and bring them to the University of Applied Sciences

* Search a linux/mac computer and download the initial setup script [init.sh](https://link-kommt-noch).
**Prepare the software**

* Connect your board via usb to the computer and run the init.sh setup script.
The setup script determines the board's MAC address and flashes the initial base image.
The base image prepares the board to connect with the WIFI network and listen for incoming Over-the-Air (OTA) updates.
The script will also display the board's MAC address after the flashing procedure has finished.

* Write down the MAC address and register the address at the `Datenverarbeitungszentrale` (DVZ) via the predefined [form](https://www.fh-muenster.de/datenverarbeitungszentrale/service/formulare/zulassung-geraete-rechnernetz.php).
The registration has to be submitted by an employee / professor, this procedure may take some time.

* Review the successful registration and flashing process by checking the board's status in the [IOT Dashboard](https://iot.nplab.de)
Your board should appear in the list and you should now write down the displayed IP address.

* Your board has now successfully been integrated in the FH-Muenster's IOT network and is ready to be flashed with it's purpose specific software image.

## Firmware Development

* Search a computer having [Visual Studio Code](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/) installed.
Alternatively login to you [CampusCluster](https://fh-muenster.de/campus-cluster) account.

* Open a terminal and clone the github repository

```bash
git clone https://github.com/nplab/IOT-project
```

* Run the Visual Studio Code editor and, if not already done, install the PlatformIO extension.

* Open the `Base-Image` example located in the `Arduino` subdirectory.
You should at least edit the `WIFI_NAME`, `WIFI_PSK` and the `OTA_PASSWORD` in the project specific `platformio.ini` configuration file.
If no `OTA_PASSWORD` is defined, the board will set its MAC address as the `OTA_PASSWORD`.
The actual source code is located in the `src` subdirectory in the `main.cpp` file.
You can now compile the firmware image by clicking on the `build` button in left menu bar.

* To flash the image, you have to set the board's IP address in the `upload_port` variable as well as the board's OTA password in the `--auth` variable.
You will find both variables in the `[env:Base-Image-OTA]` section of the `platformio.ini` configuration file.
Start the flashing process by clicking the `Upload` button in the left menu bar.
You can monitor the upload process in the terminal output in the bottom status window.
