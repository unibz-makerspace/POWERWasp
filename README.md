# PowerWASP Firmware v1.9
Original Firmware taken from: https://www.personalfab.it/downloads/download-info/firmware-powerwasp/

## How to donwload the Firmware:
 - Download Arduino: https://www.arduino.cc/en/Main/OldSoftwareReleases#1.5.x (working version 1.6.8)
 - Connect the printer with the USB cable to a PC. The printer needs to be powered off
 - Decompress the downloaded firmware folder
 - Verify in the printer menu the board version (possible versions are 2.0 or 2.1, if none then it is compatible to version 1.9)
 - Open the .ino file
 - Select Arduino MEGA 2560 in: Tools -> Board
 - Select the correct serial port in: Tools -> Port
 - Click on the icon "Upload" on the top left pane and wait till finished
 - Power on the printer and choose following operations:
   - Menu -> Control -> Default settings
   - Menu -> Control -> Save to EEPROM
   - Menu -> Control -> Load from EEPROM

## Functionalities available in the Firmware:
 - STOP & SAVE: Possibility to save during the print the coordinates of the current working point. It is possible to recover the print in a second moment. All it needs is to select after a restart of the machine the file "RESURR.G" from the SD Card.
 - LDM Mode (Clay): Switch between FFF mode for plastic materials and LDM mode for dense-fluids extrusion. Loads different presets like print heigth and step/mm. With LDM mode enabled the temperature control is disabled.
 