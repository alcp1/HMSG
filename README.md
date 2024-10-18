# HMSG
HAPCAN / MQTT / Socket Server Gateway for Raspberry Pi!

# Scope
The HAPCAN - MQTT - Socket Gateway (or **HMSG** for short) was originally thought to be an extension of the HAPCAN ETHERNET INTERFACE (univ 3.102.0.x), with the following features:
 * Send and Receive messages from/to the CAN Bus to/from the Ethernet port (as the univ 3.102.0.x does);
 * Connect to the HAPCAN Programmer to interface with the HAPCAN Modules;
 * Subscribe/Publish to MQTT topics to receive/send messages to/from the CAN Bus;
This way, the HAPCAN integration with Home automation platforms, such as Home Assistant or OpenHAB through MQTT would be easier.

# Hardware
The goal is to use a HAPCAN HAT for the Raspberry Pi.
For the current tests, two third-party Raspberry Pi HATs were used:
 * https://www.waveshare.com/wiki/RS485_CAN_HAT
 * https://www.elektor.com/pican-2-duo-can-bus-board-for-raspberry-pi-2-3

Any other interface would work, as long as the CAN transceiver is compatible with the same CAN Bus specifications as HAPCAN, and the CAN Controller is compatible with Raspberry Pi / socketCAN (please, check the additional resources at the end of this document).

As with any HAT, it is attached to the Raspberry Pi. The HATs used during the “concept check” phase are a “generic” CAN Bus interface, only providing the connections for CANH and CANL. Therefore, at this stage, two power supplies are needed: a 24V supply for the HAPCAN modules, and a 5V supply for the Raspberry Pi. The power supply grounds (outputs) are connected together.

# Raspberry Pi Setup and Configuration
The following steps are needed to setup and configure the Raspberry Pi to run the **HMSG** software:

## Initial Raspberry Pi setup
 * STEP 1: Install "Raspberry Pi Imager"
 * STEP 2: Open the program, and click on "CHOOSE OS". Select "Raspberry Pi OS (other)" and then “Raspberry Pi OS Lite (32-bit)".
 * STEP 3: Click on the gear icon for advanced options.
 * STEP 4: Configure remote access with SSH:
     - Setup hostname (to find the device on the network)
     - Enable SSH
     - Set username and password
     - Click on "Save"
 * STEP 5: Choose the storage, and then Write. 
 * STEP 6: Power the raspberry pi with the flashed sd card. At this point you can run a SSH client (for example, PuTTY) on your computer, and login to the Raspberry Pi remotely (using the username and password configured above).
 * STEP 7: On the command line, run config tool: 
     ```
     sudo raspi-config
     ```
 * STEP 8: Inside the config tool, configure the following items:
    * Advanced Options > Expand Filesystem
 * STEP 9: Reboot
    ```
    sudo reboot
    ```
 * STEP 10: Setup the network options (optional).
    ```
    sudo nano /etc/dhcpcd.conf
    ```
 * STEP 11: Reboot
    ```
    sudo reboot
    ```
* STEP 12: Update
    ```
    sudo apt-get update
    sudo apt-get upgrade
    sudo reboot
    ```

## Installing dependencies
### Install git
```
sudo apt-get install git
```
### Compile and Install Eclipse Paho C client library
* STEP 1: Install Open SSL
    ```
    sudo apt-get install libssl-dev
    ```
* STEP 2: Install Doxygen
    ```
    sudo apt-get install doxygen
    ```
* STEP 3: Get Library Repository:
    ```
    git clone https://github.com/eclipse/paho.mqtt.c.git
    ```
* STEP 4: Compile and Install
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
    sudo nano /boot/firmware/config.txt
    ```
* STEP 2: Add the following lines to the end of the file (*this step will depend on the HAT used*):
    
    *For rPi HAT with LED Output:*
    ```
    dtparam=spi=on
    dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25,spimaxfrequency=2000000
    dtoverlay=spi-bcm2835
    ```
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
## Compile the HMSG code and run it:
* STEP 1: Go to user folder:
    ```
    cd /home/pi
    ```
    **REMARK:** It is considered that the user is "pi". If not, the folder should be /home/_username_.
* STEP 2: Get files from github
    ```
    git clone https://github.com/alcp1/HMSG
    ```
* STEP 3: Compile and Install
    ```
    cd /home/pi/HMSG/SW
    sudo make all
    ```
* STEP 4: Update the configuration file /home/pi/HMSG/SW/config.json 
    
    **REMARK:** See the section [Configuration File](#configuration-file) below for details on how to update the file.
    
* STEP 5: Run the program
    ```
    sudo /home/pi/HMSG/SW/HMSG
    ```
## Running the HMSG program at startup and automatically restarting it
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
    ExecStart= /home/pi/HMSG/SW/HMSG
    WorkingDirectory=/home/pi/HMSG/SW
    Restart=always
    RestartSec=40
    
    [Install]
    WantedBy=multi-user.target
    ```
**REMARK:** If the user is not pi, this file has to be updated with the correct user and folder.
* STEP 3: Enable the Service:
    ```
    sudo systemctl daemon-reload
    sudo systemctl enable hmsg.service
    ```
# Configuration file
Here is how the file config.json can be configured:

## Section "GeneralSettings"
In this section, it is possible to configure the following settings:
* Configuration ID:

    | Field       | Description       | Possible Values                |
    | :---        | :---              | :---                           |
    | computerID1 | COMP ID1 / Module | *Number* from **1** to **255** |
    | computerID2 | COMP ID2 / Module | *Number* from **1** to **255** |

    These fields configure the HAPCAN Group / Module of the HMSG module. Whenever a system message is sent by the module to the CAN Bus, the COMP ID1 and COMP ID2 fields are set with the values from these configuration fields.

* RTC CAN Message:

    | Field          | Description       | Possible Values                   |
    | :---           | :---              | :---                              |
    | enableRTCFrame | enable RTC Frames | *Boolean* (**true** or **false**) |

    This field enables or disables the RTC Frames. If enabled, the RTC Frames are sent every minute, when the seconds are 0. Be sure the system localization settings are correctly set up in *raspi-config* for proper time zone.

* Socket Server:

    | Field              | Description          | Possible Values                   |
    | :---               | :---                 | :---                              |
    | enableSocketServer | enable Socket server | *Boolean* (**true** or **false**) |
    | socketServerPort   | Socket Server Port   | *String* of port number           |

    These fields configure the Socket Server options. If *enableSocketServer* is false, this feature is disbled. The *socketServerPort* is used as the server's listening port when this feature is enabled. When using the HAPCAN Programmer, the port used to communicate with the HMSG has to be the same as this configuration. Be aware that some ports are reserved for operating system (0-1023). For the tests, the field socketServerPort was set as "33556".

* MQTT:

    | Field           | Description                   | Possible Values                            |
    | :---            | :---                          | :---                                       |
    | enableMQTT      | enable MQTT connection        | *Boolean* (**true** or **false**)          |
    | mqttBroker      | MQTT Broker address           | *String* of MQTT Broker IP Address         |
    | mqttClientID    | MQTT Client ID                | *String* of MQTT Client ID                 |
    | subscribeTopics | List of topis to subscribe to | *JSON Array* with *Strings* of MQTT Topics |

    These fields configure the MQTT options. If *enableMQTT* is false, this feature is disbled. The *mqttBroker* should be set with the IP Address of the MQTT Broker. For instance, "192.168.0.100". The *mqttClientID* is used for the HMSG modue to identify itself when connecting to the MQTT Broker. No special requirements are needed here. For the tests, a simple, short string was used, such as "RPiTest". The *subscribeTopics* is a JSON array of topics (strings) that the module will subscribe to.

    **REMARK:** When setting up the modules (for instance, a Relay module), if a command topic is used to send a HAPCAN command to a module, this command topic has to be subscribed in this list of *subscribeTopics*, otherwise the command will not get to the HMSG module. For example, if the coomand topic of a ginven relay module is set as "MyRootTopic/MyRelay/set", the *subscribeTopics* should have "MyRootTopic/MyRelay/set", or "MyRootTopic/MyRelay/#" or "MyRootTopic/#". Same is valid for *rawHapcanSubTopics* (each topic has to be part of the subscribed topics on *subscribeTopics*).

* RAW HAPCAN Frames

    | Field              | Description                                                  | Possible Values                                                                     |
    | :---               | :---                                                         | :---                                                                                |
    | enableRawHapcan    | enable RAW HAPCAN Frames                                     | *Boolean* (**true** or **false**)                                                   |
    | rawHapcanPubAll    | enable Pub of all application frames                         | *Boolean* (**true** or **false**)                                                   |
    | rawHapcanPubTopic  | MQTT Raw Publish topic                                       | *String* of MQTT Topic the RAW Messages will be Published to                        |
    | rawHapcanSubTopics | MQTT Raw Subscription topic                                  | *JSON Array* with *Strings* of MQTT Topics the RAW Messages will be Subscribed from |
    | rawHapcanPubModules      | List of modules to have their application messages published | *JSON Array* with a list of modules - see fields below |

    These fields configure the MQTT RAW Frame options. The MQTT RAW Frame is a JSON String with all the fields found in a typical HAPCAN Frame. Here is one example to set a LED from a UNIV 3.1.3.x button:
    
    *{"Frame":266, "Flags":0, "Module":170, "Group":170, "D0":2, "D1":0, "D2":11, "D3":100, "D4":32, "D5":255, "D6":255, "D7":255}*
    
    The MQTT RAW Frame is only sent for application messages. It means that whenever a CAN Bus HAPCAN message is read with "Frame" higher than 0x200, the HMSH module transform this message into a JSON String as in the example above, and publishes it to the *rawHapcanPubTopic*. 
  
    Also, whenever any JSON String with the given format is received on any of the *rawHapcanSubTopics*, the message is transformed into a HAPCAN Frame, and sent to the BUS. For the JSON string received by the HMSG module, no check is performed for the "Frame" (it means a system message - "Frame" lower than 0x200 - could be sento to the CAN Bus as well).

    If *rawHapcanPubAll* is set to true, all application frames are published to the configured rawHapcanPubTopic. Otherwise, only the modules configured on the field *rawHapcanPubModules* are published. The fields to be added are:
    | Field                | Description                               | Possible Values                |
    | :---                 | :---                                      | :---                           |
    | node                 | HAPCAN module / node                      | *Number* from **1** to **255** |
    | group                | HAPCAN group number                       | *Number* from **1** to **255** |
    
    One posible use of this feature is to create a HAPCAN network extension. If two HMSG modules are connected to the same Network (via the ethernet port), but each one of them is connected to its own HAPCAN nodes (via the CAN Bus HAT), the nodes from each HAPCAN network can exchange messages to each other. It is only necessary to configure the *rawHapcanPubTopic* of one HMSG as one of the the *rawHapcanSubTopics* of the other HMSG and vice-versa.

  <details><summary>Example 1 - JSON String (exerpt) - All channels, all status, all commands configured </summary>
  <p>
      
  ``` json
    "enableRawHapcan": true,
    "rawHapcanPubAll": false,
    "rawHapcanPubTopic": "MyRootTopic/RAW/rPiTest1",
    "rawHapcanSubTopics": 
    [
        "MyRootTopic/RAW/rPiTest2",
        "MyRootTopic/RAW/rPiTest3"
    ],
    "rawHapcanPubModules":
    [
        {"node": 1, "group": 1},
        {"node": 1, "group": 2}
    ],
  ````
      
  </p>
  </details>
    
* HAPCAN Module Status
 
    | Field              | Description                    | Possible Values                                                           |
    | :---               | :---                           | :---                                                                      |
    | enableHapcanStatus | enable HAPCAN Status Frames    | *Boolean* (**true** or **false**)                                         |
    | statusPubTopic     | MQTT Status Publish topic      | *String* of MQTT *Base* Topic the Status Messages will be Published to    |
    | statusSubTopic     | MQTT Status Subscription topic | *String* of MQTT *Base* Topic the Status Messages will be Subscribed from |

    These fields configure the MQTT Status Frame options. The *enableHapcanStatus* field enables this feature, which consists of two functionalities:
    * Module information is sent to *statusPubTopic* as a JSON String when requested.
    * Module information requests are received on *statusSubTopic*.
   
    The MQTT message with the status of a given module is sent by HMSG to the the following MQTT Topic: *statusPubTopic*/GROUP/MODULE. Here is one example of the message sent by the HMSG module when *statusPubTopic* is "MyRootTopic/CANOut/Status", and the information is requested to the HAPCAN module 3 of the group 3:
    
    **Topic:** MyRootTopic/CANOut/Status/3/3
    
    **Payload**: *{"NODE":3,"GROUP":3,"HARD":12288,"HVER":3,"ID":3477,"ATYPE":1,"AVERS":0,"FVERS":0,"BVER":772,"DESCRIPTION":"NAME","DEVID":9825,"VOLBUS":23.99232245681382,"VOLCPU":3.3709923664122137,"UPTIME":80451,"RXCNT":0,"TXCNT":0,"RXCNTMX":2,"TXCNTMX":8,"CANINTCNT":0,"RXERRCNT":0,"TXERRCNT":0,"RXCNTMXE":255,"TXCNTMXE":255,"CANINTCNTE":255,"RXERRCNTE":255,"TXERRCNTE":255}*
    
    For the *statusSubTopic* field, the HMSG module considers messages received on the topic formatted as *statusPubTopic*/GROUP/MODULE. See the example below:
    | Topic              | Request is sent to                | 
    | :---               | :---                              | 
    | statusSubTopic     | All configured modules            | 
    | statusSubTopic/3   | All configured modules of group 3 | 
    | statusSubTopic/3/5 | Configured module 5 of group 3    | 

    As for the MQTT payloads sent to the status topic, the following messages are accepted:
    | Payload   | Frames sent to the *configured* module(s) | 
    | :---      | :---                                      | 
    | `STATIC`  | 0x104, 0x106, 0x10E, 0x111                | 
    | `DYNAMIC` | 0x10C, 0x113, 0x115                       | 
    | `STATUS`  | 0x109                                     | 
    | `ALL`     | All messages detailed above               | 
        
    **REMARK:** If a module is connected to the HAPCAN Bus, but it is not configured within *config.json*, the requests will not reach such module.

* HAPCAN - MQTT Gateway
 
    | Field         | Description                     | Possible Values                   |
    | :---          | :---                            | :---                              |
    | enableGateway | enable HAPCAN <--> MQTT Gateway | *Boolean* (**true** or **false**) |

    This functionality enables the translation from MQTT to HAPCAN Frames and vice-versa.
    
    Every Relay, Button or RGB module (right now, these are the supported modules) has to be manually added to the JSON configuration file. And each module has a specific set of fields to be filled within this configuration file, and accepted messages for incoming MQTT messsages. See the sections below for more details.
    
## Section "HAPCANRelays"

This section handles modules that send the frame type "0x302" for their status:
    
### JSON Setup:

Fields needed for setup:
    
| Field                | Description                               | Possible Values                |
| :---                 | :---                                      | :---                           |
| node                 | HAPCAN module / node                      | *Number* from **1** to **255** |
| group                | HAPCAN group number                       | *Number* from **1** to **255** |
| relays               | JSON Array - one element for each channel | *JSON String*                  |
| -   relays[].channel | Channel Number (1 to 6)                   | *Number* from **1** to **6**   |
| -   relays[].state   | MQTT State Topic                          | *String* of MQTT State Topic   |
| -   relays[].command | MQTT Command Topic                        | *String* of MQTT Command Topic |
    
<details><summary>Example 1 - JSON String (exerpt) - All channels, all status, all commands configured </summary>
<p>
     
``` json
{
  "node": 10,
  "group": 235,
  "relays": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/Test1",
      "command": "MyRootTopic/CANIn/Test1"
    },
    {
      "channel": 2,
      "state": "MyRootTopic/CANOut/Test2",
      "command": "MyRootTopic/CANIn/Test2"
    },
    {
      "channel": 3,
      "state": "MyRootTopic/CANOut/Test3",
      "command": "MyRootTopic/CANIn/Test3"
    },
    {
      "channel": 4,
      "state": "MyRootTopic/CANOut/Test4",
      "command": "MyRootTopic/CANIn/Test4"
    },
    {
      "channel": 5,
      "state": "MyRootTopic/CANOut/Test5",
      "command": "MyRootTopic/CANIn/Test5"
    },
    {
      "channel": 6,
      "state": "MyRootTopic/CANOut/Test6",
      "command": "MyRootTopic/CANIn/Test6"
    }
  ]
}
````
     
</p>
</details>

<details><summary>Example 2 - JSON String (exerpt) - Only a few channels, command, status configured </summary>
<p>

**REMARK:** When the channel is not configured, messages received on the CAN Bus by the HMSG module will not generate a MQTT status message. The same happens if a given channel is configured without a status topic. When the command topic is blank, the HMSG will not send commands to the HAPCAN module from MQTT.

``` json
{
  "node": 10,
  "group": 235,
  "relays": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/Test1",
      "command": "MyRootTopic/CANIn/Test1"
    },
    {
      "channel": 3,
      "state": "MyRootTopic/CANOut/Test3"
    },
    {
      "channel": 5,
      "command": "MyRootTopic/CANIn/Test4"
    }
  ]
}
````

</p>
</details>
    
### COMMAND Payloads

The following are the acceptable Payloads when receiving a message on the command topic (all are strings):

| Payload                                                                                       | Action (all are sent with direct control frame 0x10A)           | 
| :---                                                                                          | :---                                                            | 
| `ON`                                                                                          | Turn the relay channel output ON immediately                    | 
| `OFF`                                                                                         | Turn the relay channel output OFF immediately                   | 
| `TOGGLE`                                                                                      | Toggle the relay channel output immediately                     | 
| `String as Number`                                                                            | Set the relay channel output immediately*                       | 
| `{"INSTR1":Integer, "INSTR3":Integer, "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}`  | Set the relay channel according to the INSTR set on the payload |

**REMARK:** For the `String as Number` payload, the acceptable values are "0" (OFF), "0x00" (OFF), "0xFF" (ON), or "255" (ON).

### STATUS Payloads

The following are the Payloads sent to the STATUS when the module sends a Relay message on the CAN Bus (all are strings):

| Status              | MQTT Payload | 
| :---                | :---         | 
| Relay channel is ON | `"ON"`       | 
| Relay channel is ON | `"OFF"`      | 


## Section "HAPCANButtons"

This section handles modules that send the frame type "0x301" for their status:

### JSON Setup:

Fields needed for setup:
    
| Field                              | Description                               | Possible Values                |
| :---                               | :---                                      | :---                           |
| node                               | HAPCAN module / node                      | *Number* from **1** to **255** |
| group                              | HAPCAN group number                       | *Number* from **1** to **255** |
| buttons                            | JSON Array - one element for each channel | *JSON String*                  |
| -   buttons[].channel              | Channel Number (1 to 14)                  | *Number* from **1** to **14**  |
| -   buttons[].state                | MQTT State Topic                          | *String* of MQTT State Topic   |
| -   buttons[].command (*optional*) | MQTT Command Topic (only valid for LEDs)  | *String* of MQTT Command Topic |

Optional fields for temperature setup (for button modules with temperature sensor):

| Field                              | Description                               | Possible Values                |
| :---                               | :---                                      | :---                           |
| temperature                        | JSON Object - up to one for each module   | *JSON Object*                  |
| - temperature.state                | MQTT State Topic                          | *String* of MQTT State Topic   |
| thermostat                         | JSON Object - up to one for each module   | *JSON Object*                  |
| - thermostat.state                 | MQTT State Topic                          | *String* of MQTT State Topic   |
| - thermostat.command               | MQTT Command Topic                        | *String* of MQTT Command Topic |
| temperatureController              | JSON Object - up to one for each module   | *JSON Object*                  |
| - temperatureController.state      | MQTT State Topic                          | *String* of MQTT State Topic   |
| - temperatureController.command    | MQTT Command Topic                        | *String* of MQTT Command Topic |
| temperatureError                   | JSON Object - up to one for each module   | *JSON Object*                  |
| - temperatureError.state           | MQTT State Topic                          | *String* of MQTT State Topic   |

<details><summary>Example 1 - JSON String (exerpt) - All button, LED, temperature fields configured</summary>
<p>
     
``` json
{
  "node": 11,
  "group": 100,
  "buttons": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/ButtonTest1",
      "command": ""
    },
    {
      "channel": 2,
      "state": "MyRootTopic/CANOut/ButtonTest2",
      "command": ""
    },
    {
      "channel": 3,
      "state": "MyRootTopic/CANOut/ButtonTest3",
      "command": ""
    },
    {
      "channel": 4,
      "state": "MyRootTopic/CANOut/ButtonTest4",
      "command": ""
    },
    {
      "channel": 5,
      "state": "MyRootTopic/CANOut/ButtonTest5",
      "command": ""
    },
    {
      "channel": 6,
      "state": "MyRootTopic/CANOut/ButtonTest6",
      "command": ""
    },
    {
      "channel": 7,
      "state": "MyRootTopic/CANOut/ButtonTest7",
      "command": ""
    },
    {
      "channel": 8,
      "state": "MyRootTopic/CANOut/ButtonTest8",
      "command": ""
    },
    {
      "channel": 9,
      "state": "MyRootTopic/CANOut/ButtonTest9",
      "command": ""
    },
    {
      "channel": 10,
      "state": "MyRootTopic/CANOut/ButtonTest10",
      "command": ""
    },
    {
      "channel": 11,
      "state": "MyRootTopic/CANOut/ButtonTest11",
      "command": ""
    },
    {
      "channel": 12,
      "state": "MyRootTopic/CANOut/ButtonTest12",
      "command": ""
    },
    {
      "channel": 13,
      "state": "MyRootTopic/CANOut/ButtonTest",
      "command": ""
    },
    {
      "channel": 14,
      "state": "MyRootTopic/CANOut/LEDTest",
      "command": "MyRootTopic/CANIn/LEDTest/set"
    }
  ],
  "temperature": {
    "state": "MyRootTopic/CANOut/Temperature1"
  },
  "thermostat": {
    "state": "MyRootTopic/CANOut/Thermostat1",
    "command": "MyRootTopic/CANIn/Thermostat1/set"
  },
  "temperatureController": {
    "state": "MyRootTopic/CANOut/TControl1",
    "command": "MyRootTopic/CANIn/TControl1/set"
  },
  "temperatureError": {
    "state": "MyRootTopic/CANOut/TError1"
  }
}
````

</p>
</details>

<details><summary>Example 2 - JSON String (exerpt) - Only one button channel configured</summary>
<p>

**REMARK:** When the channel is not configured, messages received on the CAN Bus by the HMSG module will not generate a MQTT status message. The same happens if a given channel is configured without a status topic. When the command topic is blank, the HMSG will not send commands to the HAPCAN module from MQTT.

``` json
{
  "node": 11,
  "group": 100,
  "buttons": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/ButtonTest1",
      "command": ""
    }
  ]
}
````

</p>
</details>

### COMMAND Payloads - LEDs

The following are the acceptable Payloads when receiving a message on the command topic for *LED* (all are strings):

| Payload                                                                     | Action (all are sent with direct control frame 0x10A)           | 
| :---                                                                        | :---                                                            | 
| `ON`                                                                        | Turn the LED channel output ON immediately                      | 
| `OFF`                                                                       | Turn the LED channel output OFF immediately                     | 
| `TOGGLE`                                                                    | Toggle the LED channel output immediately                       | 
| `String as Number`                                                          | Set the LED channel output immediately*                         | 
| `{"INSTR1":Integer, "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}`  | Set the LED channel according to the INSTR set on the payload   |

**REMARK:** For the `String as Number` payload, the acceptable values are "0" (OFF), "0x00" (OFF), "0xFF" (ON), or "255" (ON).

### COMMAND Payloads - Thermostat

The following are the acceptable Payloads when receiving a message on the command topic for *THERMOSTAT* (all are strings):

| Payload                                                                     | Action (all are sent with direct control frame 0x10A)                | 
| :---                                                                        | :---                                                                 | 
| `ON`                                                                        | Turn the Thermostat ON immediately                                   | 
| `OFF`                                                                       | Turn the Thermostat OFF immediately                                  | 
| `TOGGLE`                                                                    | Toggle the Thermostat immediately                                    |
| `String as Number (Double)`                                                 | Set the Thermostat value to the input number (from -55 to 125)       | 
| `JSON String: {"Setpoint":Double}`                                          | Set the Thermostat value to the input number (from -55 to 125)       | 
| `JSON String: {"Increase":Double}`                                          | Increase the Thermostat value according to the number (from 0 to 16) | 
| `JSON String: {"Decrease":Double}`                                          | Decrease the Thermostat value according to the number (from 0 to 16) | 

### COMMAND Payloads - Temperature Controller

The following are the acceptable Payloads when receiving a message on the command topic for *TEMPERATURE CONTROLLER* (all are strings):

| Payload                                                                     | Action (all are sent with direct control frame 0x10A)  | 
| :---                                                                        | :---                                                   | 
| `ON`                                                                        | Turn the Temperature Controller ON immediately         | 
| `OFF`                                                                       | Turn the Temperature Controller OFF immediately        | 
| `TOGGLE`                                                                    | Toggle the Temperature Controller immediately          | 

### STATUS Payloads - LED / Button

The following are the Payloads sent to the STATUS when the module sends a *BUTTON / LED* message on the CAN Bus (all are strings):
| Status                       | MQTT Payload | 
| :---                         | :---         | 
| LED \ Button channel is ON   | `"ON"`       | 
| LED \ Button channel is OFF  | `"OFF"`      | 

### STATUS Payload - Temperature sensor

The following is the Payload sent to the STATUS topic when the module sends a *TEMPERATURE* message on the CAN Bus (as a *JSON String*):
| Status                                  | MQTT Payload                                                         | 
| :---                                    | :---                                                                 | 
| Temperature / Thermostat / Hysteresis   | `"{"Temperature": Float, "Thermostat": Float, "Hysteresis": Float}"` | 

### STATUS Payload - Thermostat

The following is the Payload sent to the STATUS topic when the module sends a *THERMOSTAT* message on the CAN Bus (as a *JSON String*):
| Status       | MQTT Payload                                               | 
| :---         | :---                                                       | 
| Thermostat   | `"{"Position": Integer, "State": String ("ON" or "OFF")}"` | 

### STATUS Payload - Temperature Controller

The following is the Payload sent to the STATUS topic when the module sends a *TEMPERATURE CONTROLLER* message on the CAN Bus (as a *JSON String*):
| Status                   | MQTT Payload                                               | 
| :---                     | :---                                                       | 
| Temperature Controller   | `"{"HeatState": String ("ON" or "OFF"), "HeatValue": Integer, "CoolState": String ("ON" or "OFF"), "CoolValue": Integer, "ControlState": String ("ON" or "OFF")}"` | 

## Section "HAPCANRGBs"

This section handles modules that send the frame type "0x308" for their status:
    
### JSON Setup:

Fields needed for setup:
    
| Field                | Description                               | Possible Values                   |
| :---                 | :---                                      | :---                              |
| node                 | HAPCAN module / node                      | *Number* from **1** to **255**    |
| group                | HAPCAN group number                       | *Number* from **1** to **255**    |
| isRGB                | RGB or Individual channels                | *Boolean* (**true** or **false**) |
| rgb                  | JSON Array - one element for each channel | *JSON String*                     |
| -   rgb[].channel    | Channel Number (1 to 3)                   | *Number* from **1** to **3**      |
| -   rgb[].state      | MQTT State Topic                          | *String* of MQTT State Topic      |
| -   rgb[].command    | MQTT Command Topic                        | *String* of MQTT Command Topic    |

**REMARK:** When the field "isRGB" is true, the field rgb[].channel is optional, otherwise, it is mandatory if rgb[].state or rgb[].command are set.

<details><summary>Example 1 - JSON String (exerpt) - Single Channel Configuration </summary>
<p>
     
When isRGB field is false, each channel can be configured with independent MQTT status / command topics. Here is an example:
     
``` json
{
  "node": 32,
  "group": 32,
  "isRGB": false,
  "rgb": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/PWM1",
      "command": "MyRootTopic/CANIn/PWM1/set"
    },
    {
      "channel": 2,
      "state": "MyRootTopic/CANOut/PWM2",
      "command": "MyRootTopic/CANIn/PWM2/set"
    },
    {
      "channel": 3,
      "state": "MyRootTopic/CANOut/PWM3",
      "command": "MyRootTopic/CANIn/PWM4/set"
    }
  ]
}
````
     
</p>
</details>

<details><summary>Example 2 - JSON String (exerpt) - RGB mode </summary>
<p>

When the field isRGB is true, all four channels (R, G, B and MASTER) are considered as one single entity, therefore, just a single status and a single command topics are configurable. Here is an example: 

``` json
{
  "node": 64,
  "group": 64,
  "isRGB": true,
  "rgb": [
    {
      "state": "MyRootTopic/CANOut/RGB1",
      "command": "MyRootTopic/CANIn/RGB1/set"
    }
  ]
}
````

</p>
</details>
    
### COMMAND Payloads - isRGB is false

The following are the acceptable Payloads for modules configured with isRGB as false when receiving a message on the command topic (all are strings):

| Payload                | Action (all are sent with direct control frame 0x10A)                                 | 
| :---                   | :---                                                                                  | 
| `ON`                   | Turn the RGB channel output ON softly and immediately                                 | 
| `OFF`                  | Turn the RGB channel output OFF softly and immediately                                | 
| `TOGGLE`               | Toggle the RGB channel output immediately                                             | 
| `String as Number`     | Set the RGB channel output softly and immediately with the value received as a number | 

**REMARK:** For the `String as Number` payload, the acceptable range is from "0" to "255".

### COMMAND Payloads - isRGB is true

The following are the acceptable Payloads for modules configured with isRGB as true when receiving a message on the command topic (all are strings):

| Payload                                      | Action (all are sent with direct control frame 0x10A)                                     | 
| :---                                         | :---                                                                                      | 
| `ON`                                         | Turn the RGB channels to 127 softly and immediately. MASTER channel is set to 255.        | 
| `OFF`                                        | Turn the RGB channels to OFF softly and immediately. MASTER channel is not affected.      | 
| `TOGGLE`                                     | Toggle the RGB channels immediately. MASTER channel is set to 255.                        | 
| `String as Number (comma-separated values)`  | Turn the RGB / RGBW channels according to the numbers, softly and immediately. MASTER channel is set to 255. | 
| `{"INSTR1":Integer, "INSTR2":Integer, "INSTR3":Integer, "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}`  | Send the INSTR fields to the RGB module |

**REMARK:** For each channell, the `String as Number` payload, the acceptable range is from "0" to "255".

### STATUS Payloads

The following is the Payload sent to the STATUS topic when the module sends a RGB message on the CAN Bus (all are strings):

| Status                              | MQTT Payload                                          | 
| :---                                | :---                                                  | 
| RGB Channel changes, isRGB is false | `String as Number - Integer`                          | 
| RGB Channel changes, isRGB is true  | `String as Number (comma-separated values) - Integer` |

**REMARK:** The status for each channel is defined as the output value from 0 to 255 when isRGB is false. Otherwise, for each channel, the calculation is *channel x Master / 256* as an integer from 0 to 255.

## Section "RGBWs" (RGBW Module)

This section handles modules that send the frame type "0x318" for their status:
    
### JSON Setup:

Fields needed for setup:
    
| Field                | Description                                  | Possible Values                   |
| :---                 | :---                                         | :---                              |
| node                 | HAPCAN module / node                         | *Number* from **1** to **255**    |
| group                | HAPCAN group number                          | *Number* from **1** to **255**    |
| isRGB                | RGB only or RGB + single channel (channel 4) | *Boolean* (**true** or **false**) |
| isRGBW               | RGBW only                                    | *Boolean* (**true** or **false**) |
| rgb                  | JSON Array - one element in case isRGB = true| *JSON String*                     |
| -   rgb[].state      | MQTT State Topic                             | *String* of MQTT State Topic      |
| -   rgb[].command    | MQTT Command Topic                           | *String* of MQTT Command Topic    |
| rgbw                 | JSON Array - one element for each channel    | *JSON String*                     |
| -   rgbw[].channel   | Channel Number (1 to 4)                      | *Number* from **1** to **4**      |
| -   rgbw[].state     | MQTT State Topic                             | *String* of MQTT State Topic      |
| -   rgbw[].command   | MQTT Command Topic                           | *String* of MQTT Command Topic    |

**REMARKS:** 
- When the field "isRGBW" is true, the field rgbw[].channel is optional, otherwise, it is mandatory if rgbw[].state or rgbw[].command are set.
- When the field "isRGBW" is false, but the field "isRGB" is true, it is expected that the field rgbw[] is empty (no additional single channel) or that rgbw has one single channel and rgbw[].channel is 4 (first 3 channels are RGB and the fourth is a single independent channel).


<details><summary>Example 1 - JSON String (exerpt) - Single Channel Configuration </summary>
<p>
     
When isRGBW is false and isRGB is false, each channel can be configured with independent MQTT status / command topics. Here is an example:
     
``` json
{
  "node": 32,
  "group": 32,
  "isRGBW": false,
  "isRGB": false,  
  "rgbw": [
    {
      "channel": 1,
      "state": "MyRootTopic/CANOut/PWM1",
      "command": "MyRootTopic/CANIn/PWM1/set"
    },
    {
      "channel": 2,
      "state": "MyRootTopic/CANOut/PWM2",
      "command": "MyRootTopic/CANIn/PWM2/set"
    },
    {
      "channel": 3,
      "state": "MyRootTopic/CANOut/PWM3",
      "command": "MyRootTopic/CANIn/PWM4/set"
    },
    {
      "channel": 4,
      "state": "MyRootTopic/CANOut/PWM4",
      "command": "MyRootTopic/CANIn/PWM4/set"
    }
  ]
}
````

When the field isRGBW is true, all four channels (R, G, B and W) are considered as one single entity, therefore, just a single status and a single command topics are configurable. Here is an example: 

``` json
{
  "node": 64,
  "group": 64,
  "isRGBW": true,
  "isRGB": false,
  "rgbw": [
    {
      "state": "MyRootTopic/CANOut/RGBW1",
      "command": "MyRootTopic/CANIn/RGBW1/set"
    }
  ]
}
````
     
</p>
</details>

<details><summary>Example 2 - JSON String (exerpt) - RGB + single channel mode </summary>
<p>

When the field isRGBW is false and isRGB is true, the first three channels (R, G, B) are considered as one single entity with a single status and a single command topics and a fourth channel can be setup as an independent output. Here is an example: 

``` json
{
  "node": 64,
  "group": 64,
  "isRGBW": false,
  "isRGB": true,
  "rgb": [
    {
      "state": "MyRootTopic/CANOut/RGB1",
      "command": "MyRootTopic/CANIn/RGB1/set"
    }
  ],
  "rgbw": [    
    {
      "channel": 4,
      "state": "MyRootTopic/CANOut/PWM4",
      "command": "MyRootTopic/CANIn/PWM4/set"
    }
  ]
}
````

</p>
</details>
    
### COMMAND Payloads - isRGBW is false and isRGB is false

The following are the acceptable Payloads for modules configured with isRGBW is false and isRGB as false when receiving a message on the command topic (all are strings):

| Payload                | Action (all are sent with direct control frame 0x10A)                                   | 
| :---                   | :---                                                                                    | 
| `ON`                   | Turn the RGB channel output 255 softly and immediately. MASTER channel is set to 255.   | 
| `OFF`                  | Turn the RGB channel output OFF softly and immediately. MASTER channel is not affected. | 
| `TOGGLE`               | Toggle the RGB channel output immediately. MASTER channel is set to 255.                | 
| `String as Number`     | Set the RGB channel output softly and immediately with the value received as a number. MASTER channel is set to 255. | 

**REMARK:** For the `String as Number` payload, the acceptable range is from "0" to "255".

### COMMAND Payloads - isRGBW is true or isRGB is true

The following are the acceptable Payloads for modules configured with isRGBW as true or isRGB as true when receiving a message on the command topic (all are strings):

| Payload                                      | Action (all are sent with direct control frame 0x10A)                                       | 
| :---                                         | :---                                                                                        | 
| `ON`                                         | Turn the RGB / RGBW channels to 127 softly and immediately. MASTER channel is set to 255.   | 
| `OFF`                                        | Turn the RGB / RGBW channels to OFF softly and immediately. MASTER channel is not affected. | 
| `TOGGLE`                                     | Toggle the RGB / RGBW channels immediately. MASTER channel is set to 255.                   | 
| `String as Number (comma-separated values)`  | Turn the RGB / RGBW channels according to the numbers, softly and immediately. MASTER channel is set to 255. | 
| `{"INSTR1":Integer, "INSTR2":Integer, "INSTR3":Integer, "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}`  | Send the INSTR fields to the RGBW module |

**REMARK:** For each channel, the `String as Number` payload, the acceptable range is from "0" to "255".

### STATUS Payloads

The following is the Payload sent to the STATUS topic when the module sends a RGB / RGBW message on the CAN Bus (all are strings):

| Status                                                   | MQTT Payload                                          | 
| :---                                                     | :---                                                  | 
| RGBW Channel changes, isRGBW is false and isRGB is false | `String as Number - Integer`                          | 
| RGB Channel changes, isRGB is true or isRGBW is true     | `String as Number (comma-separated values) - Integer` |

**REMARK:** The status for each channel is defined as *channel x Master / 256* as an integer from 0 to 255.


## Section "TIMs" (multiple temperature sensors and IR transmitter module)

This section handles modules that send the frame type "0x314" for their status:

### JSON Setup:

Fields needed for setup:
    
| Field                                            | Description                               | Possible Values                |
| :---                                             | :---                                      | :---                           |
| node                                             | HAPCAN module / node                      | *Number* from **1** to **255** |
| group                                            | HAPCAN group number                       | *Number* from **1** to **255** |
| temperature (*optional*)                         | JSON Array - one element for each channel | *JSON String*                  |
| -   temperature[].channel                        | Channel Number (1 to 4)                   | *Number* from **1** to **4**   |
| -   temperature[].temperatureState               | MQTT State Topic                          | *String* of MQTT State Topic   |
| -   temperature[].thermostatState (*optional*)   | MQTT State Topic                          | *String* of MQTT State Topic   |
| -   temperature[].thermostatCommand (*optional*) | MQTT Command Topic                        | *String* of MQTT Command Topic |
| -   temperature[].errorState (*optional*)        | MQTT State Topic                          | *String* of MQTT State Topic   |
| irCommand (*optional*)                           | MQTT Command Topic                        | *String* of MQTT Command Topic |

**REMARKS:** 
- When the module only has temperature sensor(s), the field "irCommand" may be ommitted.
- When the module only has IR transmitter, the field "temperature" may be ommitted.

<details><summary>Example 1 - JSON String (exerpt) - All temperature sensors configured with IR command topic </summary>
<p>
     
``` json
{
  "node": 11,
  "group": 100,
  "temperature": [
    {
      "channel": 1,
      "temperatureState": "MyRootTopic/CANOut/TempSensor1TempState",
      "thermostatState": "MyRootTopic/CANOut/TempSensor1ThermState",
      "thermostatCommand": "MyRootTopic/CANOut/TempSensor1ThermComm",
      "errorState": "MyRootTopic/CANOut/TempSensor1ErrState"
    },
    {
      "channel": 2,
      "temperatureState": "MyRootTopic/CANOut/TempSensor2TempState",
      "thermostatState": "MyRootTopic/CANOut/TempSensor2ThermState",
      "thermostatCommand": "MyRootTopic/CANOut/TempSensor2ThermComm",
      "errorState": "MyRootTopic/CANOut/TempSensor2ErrState"
    },
    {
      "channel": 3,
      "temperatureState": "MyRootTopic/CANOut/TempSensor3TempState",
      "thermostatState": "MyRootTopic/CANOut/TempSensor3ThermState",
      "thermostatCommand": "MyRootTopic/CANOut/TempSensor3ThermComm",
      "errorState": "MyRootTopic/CANOut/TempSensor3ErrState"
    },
    {
      "channel": 4,
      "temperatureState": "MyRootTopic/CANOut/TempSensor4TempState",
      "thermostatState": "MyRootTopic/CANOut/TempSensor4ThermState",
      "thermostatCommand": "MyRootTopic/CANOut/TempSensor4ThermComm",
      "errorState": "MyRootTopic/CANOut/TempSensor4ErrState"
    }    
  ],
  "irCommand": "MyRootTopic/CANOut/IRCommand"
}
````
</p>
</details>

### COMMAND Payloads - Thermostat

The following are the acceptable Payloads when receiving a message on the command topic for *thermostatCommand* for each temperature sensor channel (all are strings):

| Payload                                                                     | Action (all are sent with direct control frame 0x10A)                | 
| :---                                                                        | :---                                                                 | 
| `ON`                                                                        | Turn the Thermostat ON immediately                                   | 
| `OFF`                                                                       | Turn the Thermostat OFF immediately                                  | 
| `TOGGLE`                                                                    | Toggle the Thermostat immediately                                    |
| `String as Number (Double)`                                                 | Set the Thermostat value to the input number (from -55 to 125)       | 
| `JSON String: {"Setpoint":Double}`                                          | Set the Thermostat value to the input number (from -55 to 125)       | 
| `JSON String: {"Increase":Double}`                                          | Increase the Thermostat value according to the number (from 0 to 16) | 
| `JSON String: {"Decrease":Double}`                                          | Decrease the Thermostat value according to the number (from 0 to 16) | 

### COMMAND Payloads - IR Transmitter

The following are the acceptable Payloads for *irCommand* topic (all are strings):

| Payload                                      | Action (all are sent with direct control frame 0x10A)                                     | 
| :---                                         | :---                                                                                      | 
| `{"INSTR1":Integer, "INSTR2":Integer, "INSTR3":Integer, "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}`  | Send the INSTR fields to the module |


### STATUS Payload - Temperature sensor

The following is the Payload sent to the STATUS topic (*temperatureState*) when the module sends a *TEMPERATURE* message on the CAN Bus (as a *JSON String*):
| Status                                  | MQTT Payload                                                         | 
| :---                                    | :---                                                                 | 
| Temperature / Thermostat / Hysteresis   | `"{"Temperature": Float, "Thermostat": Float, "Hysteresis": Float}"` | 

### STATUS Payload - Thermostat

The following is the Payload sent to the STATUS topic (*thermostatState*) when the module sends a *THERMOSTAT* message on the CAN Bus (as a *JSON String*):
| Status       | MQTT Payload                                               | 
| :---         | :---                                                       | 
| Thermostat   | `"{"Position": Integer, "State": String ("ON" or "OFF")}"` | 

### STATUS Payload - Temperature Sensor Error

The following is the Payload sent to the STATUS topic (*errorState*) when the module sends an *ERROR* message on the CAN Bus (as a *Number*):
| Status       | MQTT Payload                                               | 
| :---         | :---                                                       | 
| Sensor Error | *Number* from **0** to **255**                             | 


# Optional Installation:
Here are some *optional* installation that could be performed, and additional information on some of the aspects involved in the development of HMSG.

## Visual Studio Code Debug
To debug the HMSG program using VS Code, use the steps below:
 * STEP 1: Install the extension on your PC:
     - Remote - SSH (by Microsoft)
     - Remote Development (by Microsoft)
     - Remote Explorer (by Microsoft)
 * STEP 2: Connect to the raspberry pi:
    - On VS Code, go to File > New Window
    - Go to Remote Explorer (button on the Activity Bar on the left)
    - Under the Menu "REMOTE", select "SSH", and "New Remote" (the "+" sign).
    - On the dialog box for "Enter SSH Connection Command", type the command below (using the correct user name and IP address or network name of your raspberry pi):
    ```
    ssh pi@192.168.0.255
    ```
    - Select the SSH file you want to update
    - A message "Host Added" will appear. Select "Connect"
    - Select the platform (in this case, Linux)
    - It will ask you to check the fingerprint of the connect devide. Select "Continue".
    - The password for the raspberry pi user will be asked. Type it and continue.
    - It will setup the VS Code server. Wait it to finish.
    - On the initial page or the "File" menu, select "Open Folder...".
    - Select the folder:
        ```
        /home/pi/HMSG/SW/
        ```
        **REMARK**: Use the appropriate user for the folder path (it may not be "pi").
    - It will ask if you trust the authors of this folder. Check the box that you trust the parent folder authors, and press the "Yes, I trust the authors" button.
    - It will ask say that a git repository was found. Select yes if you intend to develop using github.
* STEP 3: Add the following extensions to the raspberry pi:
    - C/C++ (by Microsoft)
    - C/C++ Extension Pack (by Microsoft). It will install 
    - Makefile Tools (by Microsoft)
* Step 4: Update json files:
    - Inside the folder .vscode, configure the files launch.json, settings.json and tasks.json (they are a part of the HMSG repository in github).
    **REMARK**: Use the appropriate user for the folder path (it may not be "pi").
* Step 5: Click on "Makefile" (button on the Activity Bar on the left) and configure the Makefile Options as follows:
    - Configuration: [Default]
    - Build target: [all]
    - Launch target: [HMSG]
    - Makefile: [Makefile]
    - Make: [/usr/bin/make]

To **build** the project, select the “Makefile” button on the left panel, and press “Makefile: Build the current target”.
To **debug** the project, go to “Run and Debug” button on the left panel, and select (gdb) Launch.

**HINT**: If the Visual Studio connection to the raspberry pi stops working, it is possible to remove all the files and start from the STEP 1 again. Go to the home folder (i.e. /home/pi) and do:
```
rm -r ./.vscode-server/ -f
```

#### Github integration
To commit the changes from the raspberry pi to github:
* Step 1: From de SW folder, edit the git config file:
```
sudo nano ../.git/config
```
* Step 2: Add the git credentials to the endo of the file. Example (use the appropriate user and email):
```
[user]
 name = example
 email = example@gmail.com
```

## Setup SSH Keys on raspberry pi
Check https://pimylifeup.com/raspberry-pi-ssh-keys/
The summary is:
```
install -d -m 700 ~/.ssh
```
```
nano ~/.ssh/authorized_keys
```
*Here it is needed to copy the public key from PuTTYgen and then save the file.
```
sudo chmod 644 ~/.ssh/authorized_keys
```
```
sudo chown pi:pi ~/.ssh/authorized_keys
```
**REMARK:** If the user is not pi, this file has to be updated with the correct user and folder.

## Valgrind Install (for memory leak tests):
It is needed to make from source (apt-get valgrind will install an older version which dows not work).
* PRE-BUILD 1: Uninstall the non working valgrind version (OPTIONAL - only if previously installed):
     ```
     sudo apt-get --purge valgrind
     ```
* PRE-BUILD 2: Force the 32-bit kernel (needed for raspberry pi 4):
     ```
     sudo nano /boot/firmware/config.txt
     ```
     Add the following lines to the end:
     ```
     arm_64bit=0
     ```
* PRE-BUILD 3: Install autoconf:
     ```
     sudo apt install autoconf 
     ```
* STEP 1: Clone the code from GIT:
     ```
     git clone https://sourceware.org/git/valgrind.git
     ```
* STEP 2: Go into the source directory.
     ```
     cd ./valgrind
     ```
* STEP 3: Setup the environment
     ```
     sudo ./autogen.sh
     ```
* STEP 4: Configure
     ```
     sudo ./configure
     ```
* STEP 5: Compile
     ```
     sudo make
     ```
* STEP 6: Install
     ```
     sudo make install
     ```
* STEP 7: Check installation (there should be no complaints)
     ```
     valgrind ls -l
     ```
After valgrind is installed, in order to check for memory leaks, use the following command:
```
valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes ./HMSG  > log.txt 2>&1
```
**REMARK:** As the program has a lot of buffers, at the log, there will be messages for "still reachable" data. The best approach is to run the command line above twice, and compare the two logs generated to see if the reachable data is the same.

## Disabling Wi-Fi and Bluetooth (for saving a few mA)
* STEP 1: Edit config file:
    ```
    sudo nano /boot/firmware/config.txt
    ```
* STEP 2: Add the following lines to the file: 
    ```
    dtoverlay=disable-wifi
    dtoverlay=disable-bt
    ```

## HW Watchdog
To enable the raspberry pi's internal HW Watchdog:

*See additional information at: https://pysselilivet.blogspot.com/2021/10/raspberry-pi-watchdog-made-simple.html*

* STEP 1: Check if the watchdog HW is available:
    ```
    ls -al /dev/watchdog*
    ```
    The answer should be:    

    *crw------- 1 root root  10, 130 Oct 17 20:26 /dev/watchdog*

    *crw------- 1 root root 248,   0 Oct 17 20:26 /dev/watchdog0*

 * STEP 2: Check the current settings:
    ```
    systemctl show | grep -i watchdog
    ```
    The answer should be something like this:    
    
    *WatchdogLastPingTimestampMonotonic=18446744073709551615*
    
    *RuntimeWatchdogUSec=0*
    
    *RuntimeWatchdogPreUSec=0*
    
    *RebootWatchdogUSec=10min*
    
    *KExecWatchdogUSec=0*
    
    *ServiceWatchdogs=yes*
    
    The info "RuntimeWatchdogUSec=0" tells us that the WD isn't active. **Must be greater than 0.**

 * STEP 3: Enable Watchdog:
    ```
    sudo nano /etc/systemd/system.conf
    ```
    Add the lines below at the end of the file:
    ```
    RuntimeWatchdogSec=10
    RebootWatchdogSec=5min
    ```
    See https://manpages.debian.org/testing/systemd/systemd-system.conf.5.en.html for more info on the parameters.
    
    Please note that the value "RuntimeWatchdogSec" must be <= 15, since the RPi processors can't handle a greater value.

* STEP 4: Reboot
    ```
    sudo reboot
    ```

* STEP 5: Check if Watchdog is running
    ```
    journalctl -b --no-pager | grep Watchdog
    ```
    The answer should be:

    *raspberrypi systemd[1]: Using hardware watchdog 'Broadcom BCM2835 Watchdog timer', version 0, device /dev/watchdog0*
    
    *raspberrypi systemd[1]: Watchdog running with a hardware timeout of 10s.*

## SW Watchdog
To enable the system's SW Watchdog:

*See additional information at:* 

*https://www.crawford-space.co.uk/old_psc/watchdog/Linux-Watchdog.html*

*https://www.crawford-space.co.uk/old_psc/watchdog/watchdog-configure.html*

* STEP 1: Enable HW watchdog
    ```
    sudo nano /boot/firmware/config.txt
    ```
    Add the following lines to the end of the [all] section:
    ```
    dtparam=watchdog=on
    ```
    Reboot
    ```
    sudo reboot
    ```

* STEP 2: Check HW Watchdog
    ```
    ls /dev/watchdog*
    ```
    The answer should be something like this:    
    
    */dev/watchdog  /dev/watchdog0*

* STEP 3: Install the watchdog system service
    ```
    sudo apt-get update
    sudo apt-get install watchdog
    ```
* STEP 4: Edit the settings
    ```
    sudo nano /etc/watchdog.conf
    ```
    Add the following lines to the end of the file:
    ```
    watchdog-device = /dev/watchdog
    watchdog-timeout = 15
    interface = eth0
    retry-timeout = 60
    ```
* STEP 5: Start and enable
    ```
    sudo systemctl enable watchdog
    sudo systemctl start watchdog
    sudo systemctl status watchdog    
    ```

## Read-only file system
* STEP 1: Go to config
    ```
    sudo raspi-config
    ```

* STEP 2: Enable Overlay File System
    Inside the config tool, configure the following items:
    * Performance Options > Overlay File System Enable/disable read-only file system > Yes
  
 * STEP 3: Reboot
    ```
    sudo reboot
    ```

# Additional resources:
## Useful terminal commands:
```
sudo systemctl status hmsg.service
```
```
sudo systemctl stop hmsg.service
```
```
sudo tail -f /var/log/syslog -n 50 | more
```
```
grep --text 'HMSG' /var/log/syslog
```
```
sudo vcgencmd measure_temp
```
```
top
```
```
sudo date
```
```
sudo uptime
```
```
raspi-gpio get prints the state of all GPIO pins
```
```
raspi-gpio get X prints the state of GPIO pin X
```
```
raspi-gpio set X op sets GPIO pin X as an output
```
```
raspi-gpio set X dh sets GPIO pin X to drive high
```
```
raspi-gpio set X dl sets GPIO pin X to drive low
```

## socketCAN information:
 * https://en.wikipedia.org/wiki/SocketCAN
 * https://elinux.org/CAN_Bus

## Flashing Raspberry Pi:
 * https://www.raspberrypi.com/software/

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
