# AirLight
TL;DR: Visualizing air pollution with LEDs using Arduino Nano 33 IoT and open-source data

The AirLight project aims to create a lamp that changes color based on air quality data. This project involves using an Arduino Nano 33 IoT to connect to a WiFi network, retrieve air quality data from a specified server, parse the data, and control an RGB LED strip to display different colors corresponding to different levels of particulate matter (PM) pollution. For more info on my Senior Independent Honors Project, check out https://sites.google.com/acsbg.org/airlight/home. UPDATE: Luftdaten.info is now https://sensor.community/bg/

![image](https://github.com/miachervenkova/AirLight/assets/144074897/05f07f19-7caa-450a-bf77-9aafb0f89d69)

### Project Components ###
Materials:
- Adapter AC/DC 220v 12v

- Arduino Nano 33 IoT (and a USB cable)

- 3 MOSFET transistors IRLB8721

- 3 resistors 120 ohm

- RGB LED strip

- 3 resistors 10K ohm

- (OPTIONAL) 1 resistor 240 ohm

- (OPTIONAL) 1 LED diode

- AND of course, a breadboard and all sizes of jumper wires 


### Step 1: Setting Up the Arduino Environment ###

- Download the Arduino IDE from the Arduino website.
- Use a USB cable to connect the Arduino Nano 33 IoT to your computer.
- Open the Arduino IDE and create a new project.
- Go to Tools > Manage Libraries... and install the following libraries:
  - WiFiNINA
  - ArduinoJson
- Go to Tools > Board and select Arduino Nano 33 IoT.
- Select the correct port from Tools > Port.

  # Schematic Circuit Diagram
  ![image](https://github.com/miachervenkova/AirLight/assets/144074897/f7b130bc-5eec-4a13-9009-c9d44533858e)

