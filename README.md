Project: ESP32 mp3 Player

Overview: 
This is an mp3 player made using an esp32 devkitv1 (or any dual core esp32)
It is currently powered using a 5V input from the micro USB port but I am working on adding 
external battery and power regulator

Components:
(will be updated later to accomodate battery and charging ic)
Dual core esp32,
PCM5102A I2C DAC;
4x Push buttons(push to make),
4x 10k ohm resistors,
Rotary Encoder,
SD card reader SPI,
SD card,
U8g2 Blue Oled I2C,
1000uf electrolytic capacitor,
2x 100uf electrolytic capacitor,
1uf Electrolytic capacitor,
100nf ceramic capacitor.

Wiring:
(will be updated later to accomodate battery and charging ic)
Oled
Vcc->3.3V
Gnd->Gnd
SCK->D22
SDA->D21

PCM5102A(Purple board)
SCK->Gnd
BCK->D26
DIN->D13
LCK->D25
GND->Gnd
VIN->3.3V
(the rest of the pins can be ignored if they have proper resistor bridges on the back)

SD Card Reader
Vcc->3.3V/5V(depending on the module used)
Gnd->Gnd
CS->D5
MOSI->D23
SCK->D18
MISO->D19

Rotary Encoder(3pin)
Right Pin(PinA)->D32
Middle Pin(PinC)->Gnd
Left Pin(PincB)->D33
(if the scrolling/rotation is wrong swap the pins either in the code or on the encoder)

Push Button
Button Up->D35
Button Down->D34
Button Right(Select)->D36
Button Left(Back)->D39
(These use the esp32 input only button which do not have internal resistors.)
(Instead connect a 10K ohm resistor between the pins(D35,D34.D36,D39) and 3.3V with the buttons bridging to Gnd)

Capacitors
1000uf Electrolytic + 100nf Ceramic between the esp32 3V3 and GND pins.(can use 100uf Electrolytic as well)
100uf Electrolytic + 100nf Ceramic between the SD Card reader Vin and Gnd pins.
100uf Electrolytic + 100nf Ceramic between the PCM5102A Vin and Gnd pins.(can also use 10ufElectrolytic instead)
1uf Electrolytic + 100nf Ceramic between the OLED VCC and GND pins.

Libraries:
Bounce2(button debounce)
ESP32Encoder by Kevin Harrington
ESP8266Audio by Earle F. Philhower, III
U8g2 by oliver

This code was written with the arduino framework using PlatformIO on VS Code.

Usage:
UP/Down buttons-> used to scroll up and down lists, and play next/previous song when song is currently playing.
LEFT/RIGHT(Back/Select) used to move back to the previous page/root folder, and open folder and play song.
Select button is also used to pause and resume the song while song is playing.
Encoder is used for fast scrolling through lists and folders and can be used to control the volume when the song is playing.
The code also supports scrolling titles for long title names, just uncomment the code lines.
