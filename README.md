> **NOTE: The code and executable contained in this repository is provided as an aid to users of the Edge Gateway 5000/5100 and is intended to serve as an example Linux IO access implementation for CANBus and GPIO on the platform in the C programming language. 

# CANbus Controller

The CANbus chip in Dell Edge Gateway 5000s is a PIC32MX530F128H microcontroller from Microchip (see **References** below for manual).  The microcontroller is connected via the USB bus to the CPU and the OS sees it as a generic Human Interface Device (HID). For this reason, it is possible to communicate with the CANbus chip via generic HID raw instructions. There are many HID libraries freely available to help faciliate the communcation (such as HIDAPI), but this code uses the basic `<linux/hidraw.h>` header with C programming.

## Supported Platforms

The code and executable in this repository tested the below OS's and hardware:


**Operating Systems**

- Ubuntu 16.04 Desktop (Linux, AMD64)
- Ubuntu 15.04 Desktop (Linux, AMD64)
- Wind River (Linux, AMD64)

**Hardware Support**

- Dell Edge Gateway 5000 Series

## Setup

**Physical Hardware**

It is **absolutely critical** to have a properly terminated cable when connecting the Dell gateways to a CAN network.  A properly terminated cable consists of a 120-ohm resistor between CAN-Hi and CAN-Lo on ALL endpoints.  This can be as simple as a spliced Ethernet cable with a shunted resistor at the connector, but it is still necessary.

```
         |                                |
  CAN-Hi |--x---------- /// -----------x--| CAN-Hi
         |  |                          |  |
         |  [] 120-ohm        120-ohm []  |
         |  |                          |  |
  CAN-Lo |--x---------- /// -----------x--| CAN-Lo
         |                                |
     GND |------------- /// --------------| GND
         |                                |
```

**Get the Binaries and Source Code**

Download this repository as a zip file and extract, or clone it like this:

```sh
$ git clone https://github.com/dell/Dell-Gateway-5000-IO-Tool.git
```

The latest stable x86_64 binary files can be found in the `dist` directory. If you do not wish to compile the source yourself, skip to the **Running** section. If you wish to compile from source, continue reading below.

**Compilation**

For compilation you will need the `libudev` development headers from package `libudev-dev`. Ubuntu Desktop comes with these by default, but Wind River does not. There are many ways to obtain this package, but the easiest is to use a package manager, like this: `sudo apt-get install libudev-dev`.

By default, the `debug` configuration is built when `make` is executed. This will include the `-g` compiler flag, which `gcc` interprets to include debug symbols. You can use the resulting binary to attach a debugger, such as `valgrind`.

```sh
# From inside the top-level directory
$ make
```

The `release` configuration is built with the `-O3` compiler flag instead of `-g`, which `gcc` interprets as the most aggressive level of optimization.

```sh
# From inside the top-level directory
$ make CONF=release
```

## Running

Change to the appropriate directory:

```sh
# Change into the correct configuration directory (bin/debug or bin/release)
$ cd bin/<config_dir>/

# View the program arguments and documentation
$ ./canctl --help

# Run the program with elevated privileges
$ sudo ./canctl
```

## CANbus Configuration Modes

The `canbus_cfg_t` enum contains valid configuration modes for the CANbus module.

```c
typedef enum canbus_cfg
{
	CANBUS_CFG_NORMAL = 0x00,
	CANBUS_CFG_DISABLE = 0x01,
	CANBUS_CFG_LOOPBACK = 0x02,
	CANBUS_CFG_LISTEN_ONLY = 0x03,
	CANBUS_CFG_CONFIGURATION = 0x04,
	CANBUS_CFG_RESERVED_1 = 0x05, // Never valid
	CANBUS_CFG_RESERVED_2 = 0x06, // Never valid
	CANBUS_CFG_LISTEN_ALL_MESSAGES = 0x07,
	CANBUS_CFG_UNKNOWN // This and higher values are never valid
} canbus_cfg_t;
```

More information about these modes can be found in the *PIC32MX Family Reference Manual, Section 34 "Controller Area Network (CAN)"* (see **References** section). Some of that information has been conveniently copied below.

**Normal Operation Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
In Normal Operation mode, the CAN module will be on the CAN bus and can transmit and receive CAN messages.

**Disable Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
Disable mode is similar to Configuration mode, except that the CAN module error counters are not reset.

**Loopback Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
Loopback mode is used for self-test to allow the CAN module to receive its own message. In this mode, the CAN module transmit path is connected internally to the receive path. A “dummy” Acknowledge is provided thereby eliminating the need for another node to provide the Acknowledge bit. The CAN message is not actually transmitted on the CAN bus.

**Listen Only Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
Listen Only mode is a variant of Normal Operation mode. If Listen Only mode is activated, the module is present on the CAN bus but is passive. It will receive messages but not transmit. The CAN module will not generate error flags and will not acknowledge signals. The error counters are deactivated in this state. Listen Only mode can be used for detecting the baud rate on the CAN bus. For this to occur, it is necessary that at least two other nodes are present that communicate with each other. The baud rate can be detected empirically by testing different values. This mode is also useful as a bus monitor as the CAN bus does not influence the data traffic.

**Configuration Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
After a device reset, the CAN module is in Configuration mode. The error counters are cleared and all registers contain the reset values. The CAN module can be configured or initialized only in Configuration mode. This protects the user from accidentally violating the CAN protocol through programming errors.

**Listen All Messages Mode**

*From the PIC32MX Family Reference Manual, Section 34:*
Listen All Messages mode is a variant of Normal Operation mode. If Listen All Messages mode is activated, transmission and reception operate the same as normal operation mode with the exception that if a message is received with an error, it will be transferred to the receive buffer as if there was no error. The receive buffer will contain data that was received up to the error. The filters still need to be enabled and configured.

## CANbus Commands

The PIC microcontroller responds to and receives a few different commands exposed through HID USB interrupt endpoints. Take a look at the comments and functions in `canctl.c` for an indication on how to use the commands. From `canctl.h`:

```c
// USB Interrupt out endpoints
#define CANBUS_OUT_SEND_DATA        0xca
#define CANBUS_OUT_ERROR_STATUS     0xce
#define CANBUS_OUT_GET_CONFIG       0xcc
#define CANBUS_OUT_SET_CONFIG       0xcf
#define CANBUS_OUT_FW_VERSION       0xec
#define CANBUS_OUT_USB_TEST         0xda
#define CANBUS_OUT_CAN_TEST         0xca
#define CANBUS_OUT_LED_OFF          0xdd
#define CANBUS_OUT_LED_ON           0xde
#define CANBUS_OUT_LED_NORMAL       0xdf
// USB Interrupt in endpoints
#define CANBUS_IN_RECV_DATA         0xca
#define CANBUS_IN_ERROR_STATUS      0xce
#define CANBUS_IN_GET_CONFIG        0xcc
#define CANBUS_IN_SET_CONFIG        0xcf
#define CANBUS_IN_FW_VERSION        0xec
#define CANBUS_IN_USB_TEST          0xda
#define CANBUS_IN_CAN_TEST          0xca
#define CANBUS_IN_LED_OFF           0xdd
#define CANBUS_IN_LED_ON            0xde
#define CANBUS_IN_LED_NORMAL        0xdf
```

## Built-in Tests

**CANbus Loopback (Internal)**

```
________________________________________________________
Gateway                                                 |
______________      _________      __________________   |
              |    |         |    |                  |  |
              |--->|-------->|--->|---\              |  |
PC (USB Host) |    | PIC32XX |    |    | Transceiver |  |
Application   |<---|<--------|<---|<--/     Chip     |  |
______________|    |_________|    |__________________|  |
                                                        |
________________________________________________________|
```

Steps:

1. CANBUS_OUT_SET_CONFIG (CANBUS_CFG_CONFIGURATION)
2. CANBUS_IN_SET_CONFIG
3. CANBUS_OUT_SET_CONFIG (CANBUS_CFG_LOOPBACK)
4. CANBUS_IN_SET_CONFIG
5. CANBUS_OUT_SEND_DATA
6. CANBUS_IN_RECV_DATA
7. CANBUS_OUT_SET_CONFIG (CANBUS_CFG_CONFIGURATION)
8. CANBUS_IN_SET_CONFIG
9. CANBUS_OUT_SET_CONFIG (CANBUS_CFG_NORMAL)
10. CANBUS_IN_SET_CONFIG


### GPIO
-NEW: Added GPIO Command set to CANCTL to support GPIO port on Edge Gateway I/O Module. Listing from 'canctl.h' follows

// GPIO Interrupt out endpoints
#define GPIO_OUT_READ_PIN_TYPE      0xb0
#define GPIO_OUT_SET_PIN_TYPE       0xb0
#define GPIO_OUT_READ_PIN_DATA      0xb1
#define GPIO_OUT_SET_PIN_DATA       0xb1
#define GPIO_OUT_GET_BOARD_ID       0xb2
#define GPIO_OUT_GET_IOM_SKU        0xb3


// GPIO Interrupt in endpoints
#define GPIO_IN_READ_PIN_TYPE       0xb0
#define GPIO_IN_SET_PIN_TYPE        0xb0
#define GPIO_IN_READ_PIN_DATA       0xb1
#define GPIO_IN_SET_PIN_DATA        0xb1
#define GPIO_IN_GET_BOARD_ID        0xb2
#define GPIO_IN_GET_IOM_SKU         0xb3

-NEW: Added GPIO menu items to use above commands

"3 - Get firmware version (GPIO and CANBus)"
 //Updated to return both GPIO and CANBus PIC firmware versions if I/O module is selected as an endpoint
			
"14- Get GPIO pin direction settings"
 //Displays INPUT or OUTPUT direction currently set on each GPIO pin

"15- Set GPIO pin directions"
 //Presents a looped menu to guide user through setting INPUT or OUTPUT direction on each GPIO pin

"16- Get GPIO pin Data"
 //Displays HIGH or LOW  voltage currently seen at each GPIO pin

"17- Set GPIO pin Data (for OUTPUT pins only)"
 //Presents a looped menu to guide user through setting HIGH or LOW voltage output on each GPIO pin

"18- Get GPIO board ID"
 //Returns I/O Module board ID value
			
"19- Get IO Module SKU ID (GPIO Device Path)"
 //Returns I/O Module SKU value

## Known Issues

See BUGS.md

## References

- [PIC32MX Family Reference Manual ( *ref/DS60001290D* )](http://ww1.microchip.com/downloads/en/DeviceDoc/60001290D.pdf)
- [PIC32MX Family Reference Manual, Section 34. "Controller Area Network (CAN)" ( *ref/DS60001123* )](http://www.microchip.com.tw/Data_CD/Reference%20Manuals/PIC32MX%20Family%20Reference%20Manual/PIC32%20Family%20Reference%20Manual,%20Sect.%2034%20Controller%20Area%20Network%20(CAN).pdf)

## License and Copyright

MIT - See LICENSE.txt

## Contributors

Daren Cody <Daren_Cody@Dell.com>

