# HMSG
HAPCAN / MQTT / Socket Server Gateway for Raspberry Pi

# Scope
The HAPCAN - MQTT - Socket Gateway (or **HMSG** for short) was originally thought to be an extension of the HAPCAN ETHERNET INTERFACE (univ 3.102.0.x), with the following features:
 * Send and Receive messages from/to the CAN Bus to/from the Ethernet port (as the univ 3.102.0.x does);
 * Connect to the HAPCAN Programmer to interface with the HAPCAN Modules;
 * Subscribe/Publish to MQTT topics to receive/send messages to/from the CAN Bus;
This way, the HAPCAN integration with Home automation platforms, such as Home Assistant or OpenHAB through MQTT would be easier.

# Hardware
The goal is to create a HAPCAN HAT for the Raspberry Pi. At the moment, HW development hasn't started.
Basically, it would contain the following blocks:
 * Connector: The Ethernet connector (x2) for the HAPCAN Bus Connection
 * DC-DC converter: To supply the Raspberry Pi and the HAT with 5V
 * CAN Transceiver
 * CAN Controller: For socketCAN compatibility with linux.

For the current tests, two third-party Raspberry Pi HATs were used:
 * https://www.waveshare.com/wiki/RS485_CAN_HAT
 * https://www.elektor.com/pican-2-duo-can-bus-board-for-raspberry-pi-2-3

Any other interface would work, as long as the CAN transceiver is compatible with the same CAN Bus specifications as HAPCAN, and the CAN Controller is compatible with Raspberry Pi / socketCAN (please, check the additional resources at the end of this document).

As with any HAT, it is attached to the Raspberry Pi. The HATs used during the “concept check” phase are a “generic” CAN Bus interface, only providing the connections for CANH and CANL. Therefore, at this stage, two power supplies are needed: a 24V supply for the HAPCAN modules, and a 5V supply for the Raspberry Pi. The power supply grounds (outputs) are connected together.

# Raspberry Pi Setup and Configuration
The following steps are needed to setup and configure the Raspberry Pi to run the **HMSG** software:

## Initial Raspberry Pi setup
 * STEP 1: Flash the Raspberry Pi with the latest version “Raspberry Pi OS Lite”.
 * STEP 2: Connect the Raspberry Pi to a Monitor and a Keyboard (only needed for the initial configuration).
 * STEP 3: Power up the Raspberry Pi.
 * STEP 4: Login (default user: *pi* / default password: *raspberry*)
 * STEP 5: Run config tool: 
     ```
     sudo raspi-config
     ```
 * STEP 6: Inside the config tool, configure the following items:
    * Localisation Options (for keyboard / timezone)
    * System Options > Password (for secutiry) 
    * Interface Options > (Enable SSH Server)
    * Advanced Options > Expand Filesystem
 * STEP 7: Reboot
    ```
    sudo reboot
    ```
 * STEP 8: Setup the network options.
    ```
    sudo nano /etc/dhcpcd.conf
    ```
 * STEP 9: Reboot
    ```
    sudo reboot
    ```
**REMARK:** At this point you do not need the keyboard or the monitor. You can run a SSH client (for example, PuTTY) on your computer, and login to the Raspberry Pi remotely.
## Installing dependencies
### Install git
```
sudo apt-get install git
```
### Compile and Install Eclipse Paho C client library
* STEP 1: Update Packages:
    ```
    sudo apt-get update
    sudo apt-get upgrade
    sudo apt-get dist-upgrade
    sudo reboot
    ```
* STEP 2: Install Open SSL
    ```
    sudo apt-get install libssl-dev
    ```
* STEP 3: Install Doxygen
    ```
    sudo apt-get install doxygen
    ```
* STEP 4: Get Library Repository:
    ```
    git clone https://github.com/eclipse/paho.mqtt.c.git
    ```
* STEP 5: Compile and Install
    ```
    cd paho.mqtt.c
    sudo make
    sudo make html
    sudo make install
    ```   
### Install JSON-C library
```
sudo apt-get update
sudo apt-get upgrade
sudo apt install libjson-c-dev
```
## Setting Up CAN Interface / CAN HAT
* STEP 1: Edit config file to enable SPI and MCP2515 interface:
    ```
    sudo nano /boot/config.txt
    ```
* STEP 2: Add the following lines to the end of the file (*this step will depend on the HAT used*):
    
    *For PiCAN 2 Duo:*
    ```
    dtparam=spi=on
    dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25
    dtoverlay=mcp2515-can1,oscillator=16000000,interrupt=24
    dtoverlay=spi-bcm2835
    ```
    *For Waveshare RS485 CAN HAT with 8MHz Crystal:*
    ```
    dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25,spimaxfrequency=1000000
    ```
    *For Waveshare RS485 CAN HAT with 12MHz Crystal:*
    ```
    dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25,spimaxfrequency=2000000
    ```
* STEP 3: Reboot
    ```
    sudo reboot
    ```
* STEP 4: Edit interface information / update:
    ```
    sudo nano /etc/network/interfaces
    ```
* STEP 5: Add the following lines to the end of the file:
    ```
    # CAN 0 interface
    auto can0   
    iface can0 inet manual
        pre-up /sbin/ip link set $IFACE type can bitrate 125000 listen-only off
        up /sbin/ifconfig $IFACE up
        down /sbin/ifconfig $IFACE down
    ```
* STEP 6: Reboot
    ```
    sudo reboot
    ```
* STEP 7 (*optional*): Install CAN Utils:
    ```
    sudo apt-get install can-utils
    ```
## Initial Test
At this point you should be able to read and write messages to the CAN Bus using the CAN Utils (see additional resources below). You can use this tool to check your setup before compiling and running the code.
## Complie the HMSG code and run it:
* STEP 1: Go to user folder:
    ```
    cd /home/pi
    ```
* STEP 2: Get files from github
    ```
    git clone https://github.com/alcp1/HMSG
    ```
* STEP 5: Compile and Install
    ```
    cd /home/pi/HMSG/SW
    sudo make all
    ```
* STEP 6: Update the configuration file /home/pi/HMSG/SW/config.json 
    
    **REMARK:** See the section "Configuration file" below for details on how to update the file.
    
* STEP 7: Run the program
    ```
    sudo /home/pi/HMSG/SW/HMSG
    ```
# Additional resources:
## socketCAN:
 * https://en.wikipedia.org/wiki/SocketCAN
 * https://elinux.org/CAN_Bus

## Flashing Raspberry Pi:
 * https://www.raspberrypi.org/downloads/

## Disbaling Wi-Fi and Bluetooth (for saving a few mA)
* STEP 1: Edit config file:
    ```
    sudo nano /boot/config.txt
    ```
* STEP 2: Add the following lines to the file: 
    ```
    dtoverlay=disable-wifi
    dtoverlay=disable-bt
    ```
## Running the HMSG program at startup and automatically restart it
* STEP 1: Create a service:
    ```
    sudo nano /lib/systemd/system/hmsg.service
    ```
* STEP 2: Edit the service by adding the the lines below to the file:
    ```
    [Unit]
    Description=HMSG Service
    After=multi-user.target
    
    [Service]
    Type=idle
    
    User=pi
    ExecStart= /home/pi/HMSG/HMSG
    
    Restart=always
    RestartSec=40
    
    [Install]
    WantedBy=multi-user.target
    ```
* STEP 3: Enable the Service:
    ```
    sudo systemctl daemon-reload
    sudo systemctl enable hmsg.service
    ```

## CAN Utils - HAPCAN AND CAN Messages
For initial tests, you can use CAN Utils.
* *Reading messages from CAN:*
```
sudo candump can0 -t A
```
* *Writing messages to the CAN Bus (example for Button Frame):*
```
cansend can0 0602DEED#FFFF0101FFFFFFFF
```
The example below is for a button module (UNIV 3.1.3.1) setup as MODULE 11 (0x0B) from GROUP 100 (0x64).
When the button 13 is pressed, the CAN message sent by the module is:
* CANID (32-bit / 4 Bytes): 0x06020B64
* DATA (8 Bytes): 0xFF 0xFF 0x0D 0xFF 0x01 0xFF 0xFF 0xFF

*Breaking down the message:*
* CANID:
    * Frametype (CANID bits 28 through 17): 0x301
    * Flags (just one nibble): 0x0
    * Module: 0x0B
    * Group: 0x64
* DATA:
    * D2 (CHANNEL): 0x0D (13 in decimal)
    * D3 (BUTTON):  0xFF (closed)
    * D4 (LED):     0x01 (disabled)        

When the button 13 is released, the CAN message sent by the module is:
* CANID (32-bit / 4 Bytes): 0x06020B64
* DATA (8 Bytes): 0xFF 0xFF 0x0D 0x00 0x01 0xFF 0xFF 0xFF

It is the same as before, but D3 (BUTTON) is set to 0x00 (open).

# Configuration file
## Section "GeneralSettings"
In this section, it is possible to configure the following settings:
* Configuration ID:

    | Field       | Description       | Possible Values                |
    | :---:       | :---:             | :---:                          |
    | computerID1 | COMP ID1 / Module | *Number* from **1** to **255** |
    | computerID2 | COMP ID2 / Module | *Number* from **1** to **255** |

    These fields configure the HAPCAN Group / Module of the HMSG module. Whenever a system message is sent by the module to the CAN Bus, the COMP ID1 and COMP ID2 fields are set with the values from these configuration fields.

* RTC CAN Message:

    | Field          | Description       | Possible Values                   |
    | :---:          | :---:             | :---:                             |
    | enableRTCFrame | enable RTC Frames | *Boolean* (**true** or **false**) |

    This field enables or disables the RTC Frames. If enabled, the RTC Frames are sent every minute, when the seconds are 0. Be sure the system localization settings are correctly set up in *raspi-config* for proper time zone.

* Socket Server:

    | Field              | Description          | Possible Values                   |
    | :---:              | :---:                | :---:                             |
    | enableSocketServer | enable Socket server | *Boolean* (**true** or **false**) |
    | socketServerPort   | Socket Server Port   | *String* of port number           |

    These fields configure the Socket Server options. If *enableSocketServer* is false, this feature is disbled. The *socketServerPort* is used as the server's listening port when this feature is enabled. When using the HAPCAN Programmer, the port used to communicate with the HMSG has to be the same as this configuration. Be aware that some ports are reserved for operating system (0-1023). For the tests, the field socketServerPort was set as "33556".

* MQTT:

    | Field           | Description                   | Possible Values                            |
    | :---:           | :---:                         | :---:                                      |
    | enableMQTT      | enable MQTT connection        | *Boolean* (**true** or **false**)          |
    | mqttBroker      | MQTT Broker address           | *String* of MQTT Broker IP Address         |
    | mqttClientID    | MQTT Client ID                | *String* of MQTT Client ID                 |
    | subscribeTopics | List of topis to subscribe to | *JSON Array* with *Strings* of MQTT Topics |

    These fields configure the MQTT options. If *enableMQTT* is false, this feature is disbled. The *mqttBroker* should be set with the IP Address of the MQTT Broker. For instance, "192.168.0.100". The *mqttClientID* is used for the HMSG modue to identify itself when connecting to the MQTT Broker. No special requirements are needed here. For the tests, a simple, short string was used, such as "RPiTest". The *subscribeTopics* is a JSON array of topics (strings) that the module will subscribe to.
**REMARK:** When setting up the modules (for instance, a Relay module), if a command topic is used to send a HAPCAN command to a module, this command topic has to be subscribed in this list of *subscribeTopics*, otherwise the command will not get to the HMSG module. For example, if the coomand topic of a ginven relay module is set as "MyRootTopic/MyRelay/set", the *subscribeTopics* should have "MyRootTopic/MyRelay/set", or "MyRootTopic/MyRelay/#" or "MyRootTopic/#".

* RAW HAPCAN Frames

    | Field             | Description                   | Possible Values                                                 |
    | :---:             | :---:                         | :---:                                                           |
    | enableRawHapcan   | enable RAW HAPCAN Frames      | *Boolean* (**true** or **false**)                               |
    | rawHapcanPubTopic | MQTT Raw Publish topic        | *String* of MQTT Topic the RAW Messages will be Published to    |
    | rawHapcanSubTopic | MQTT Raw Subscription topic   | *String* of MQTT Topic the RAW Messages will be Subscribed from |

    These fields configure the MQTT RAW Frame options. The MQTT RAW Frame is a JSON String with all the fields found in a typical HAPCAN Frame. Here is one example to set a LED from a UNIV 3.1.3.x button: 
    *{"Frame":266, "Flags":0, "Module":170, "Group":170, "D0":2, "D1":0, "D2":11, "D3":100, "D4":32, "D5":255, "D6":255, "D7":255}*
    The MQTT RAW Frame is only sent for application messages. It means that whenever a CAN Bus HAPCAN message is read with "Frame" higher than 0x200, the HMSH module transform this message into a JSON String as in the example above, and publishes it to the *rawHapcanPubTopic*. 
    Also, whenever any JSON String with the given format is received on the *rawHapcanSubTopic*, the message is transformed into a HAPCAN Frame, and sent to the BUS. For the JSON string received by the HMSG module, no check is performed for the "Frame" (it means a system message is sento to the CAN Bus as well).    
    One posible use of this feature is to create a HAPCAN network extension. If two HMSG modules are connected to the same Network (via the ethernet port), but each o them is connected to its own HAPCAN nodes (via the CAN Bus HAT), the nodes from each HAPCAN network can exchange messages to each other. It is only necessary to configure the *rawHapcanPubTopic* of one HMSG as the *rawHapcanSubTopic* of the other HMSG and vice-versa.
    
* HAPCAN Module Status
 
    | Field              | Description                    | Possible Values                                                           |
    | :---:              | :---:                          | :---:                                                                     |              
    | enableHapcanStatus | enable HAPCAN Status Frames    | *Boolean* (**true** or **false**)                                         |
    | statusPubTopic     | MQTT Status Publish topic      | *String* of MQTT *Base* Topic the Status Messages will be Published to    |
    | statusSubTopic     | MQTT Status Subscription topic | *String* of MQTT *Base* Topic the Status Messages will be Subscribed from |

    These fields configure the MQTT Status Frame options. The *enableHapcanStatus* field enables the MQTT Status feature. This feature consists of two functionalities:
    * Module information is sent to *statusPubTopic* as a JSON String.
    * Module information is requested by messages received on *statusSubTopic*.
    The frame is sent by HMSG to the the following MQTT Topic: *statusPubTopic*/<GROUP>/<MODULE>. One example of the message sent by the HMSG module is:
    **Topic:** MyRootTopic/3/3
    **Payload**: *{"NODE":3,"GROUP":3,"HARD":12288,"HVER":3,"ID":3477,"ATYPE":1,"AVERS":0,"FVERS":0,"BVER":772,"DESCRIPTION":"DEFECT-BDEFECT-B","DEVID":9825,"VOLBUS":23.99232245681382,"VOLCPU":3.3709923664122137,"UPTIME":80451,"RXCNT":0,"TXCNT":0,"RXCNTMX":2,"TXCNTMX":8,"CANINTCNT":0,"RXERRCNT":0,"TXERRCNT":0,"RXCNTMXE":255,"TXCNTMXE":255,"CANINTCNTE":255,"RXERRCNTE":255,"TXERRCNTE":255}*
    
    For requesting status
    
