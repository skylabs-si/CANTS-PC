
# CAN-TS client example for PC

This project provides cross-platform reference implementation of client side CAN-TS protocol developed in Qt and C++.

## Overview

CAN-TS is a lightweight communication protocol and device services specification for embedded systems used in space applications. 
In terms of the OSI model, CAN-TS implements the layers above and including the network layer. 
The CAN-TS protocol defines an addressing scheme, several memory efficient communication transfers type 
to support TM/TC, bulk data transfer (multi-message transfers) to and from device, unsolicited TM, time synchronization 
across devices and defines redundancy mechanism. The lower level protocol implementing the data link and physical layers is CAN, 
although devices using some other means of communication (such as, RS-422/485, SPI, I2C, Ethernet, EtherCAT) 
can also implement the CAN-TS. CAN-TS provides memory efficient communication stack for embedded application, 
while still assures high level of services between devices.

## Getting started

### Building

1. Download open source version of Qt installer from [web page](https://www.qt.io/download)
1. Open Qt installer and select:    
    - Latest version of Qt Creator
    - Instal MinGW 7.3.0 64-bit toolchain (you can also use MSVC compiler with Visual Studio 2017 installer)
    - Qt 5.12.x (LTS) prebuilt components for MSVC 2017 64-bit or MinGW 7.3.0 64-bit or both (depends on preferred target compiler)
1. Open project with Qt Creator and select desired configuration
1. Compile and run project

### Development board

This example works out of the box with [CAN-TS for MCU](https://github.com/skylabs-si/CANTS-MCU/).
Follow instructions in linked repository to establish environment on [PicoSky Evaluation Board](https://www.skylabs.si/portfolio-item/picosky-evaluation-board-sky-9213) with running server side of the CAN-TS communication stack. Check corresponding documentation of the [CAN-TS for MCU](https://github.com/skylabs-si/CANTS-MCU/) project for supported telecommands, telemetry and block transfers.

## Demonstration

1. Connect [PicoSky Evaluation Board](https://www.skylabs.si/portfolio-item/picosky-evaluation-board-sky-9213) with CANdelaber dongle and to PC (see evaluation board guide for more information). 
1. Start _cants-demo_ application from Qt Creator.
1. Enter serial port settings for nominal and redundant buses.
1. Open communication channel. Status should change from 'Closed' to 'Open'.
1. Start sending keep alive messages.
1. Reset evaluation board by pressing RST button.
1. Evaluation board should start sending keep alive messages on the nominal bus.
1. Check application output (tab in Qt Creator) or CANdelaber LEDs to determine if both devices use same bus.
   - Switch bus if necessary. Server should sync with client after couple of retries. Also see [CAN-TS protocol](https://support.skylabs.si/public/CAN-TS_protocol_v1.4.pdf) document for detailed description about CAN-TS redundancy.  
1. Now you can start sending example commands to evaluation board which shall test following CAN-TS transfers:
    - Telecommand: Turn LED on and off
    - Telemetry: Read LED status automatically after LED TC is executed
    - Block transfers: set and get block of memory on evaluation board
    - Time sync: trigger time synchronization on the evaluation board 

![CAN-TS application screenshot](doc/cants-demo.png?raw=true "CAN-TS application screenshot")

## Documentation

Project documentation can be build with doxygen with configuration file provided in doc folder.

Following external documents are available:

| Name| Type | Version | Date |
| :--- | :---: | :---: | :---: |
| [CAN-TS protocol](https://support.skylabs.si/public/CAN-TS_protocol_v1.4.pdf) | pdf | 1.4 | 06/2019 |
| [CAN-TS overview presentation](https://support.skylabs.si/public/CAN-TS_protocol_presentation_v1.0.pdf) | pdf | 1.0 | 06/2019 |

## License

This project is licensed under the 3-clause BSD License - see the [LICENSE.txt](LICENSE.txt) file for details.
