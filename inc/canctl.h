/**
 * @file canctl.h
 * @date 2017-03-24
 *
 * Linux HID Raw supports the following ioctl() requests:
 * HIDIOCGRDESCSIZE: Get Report Descriptor Size
 * HIDIOCGRDESC: Get Report Descriptor
 * HIDIOCGRAWINFO: Get Raw Info
 * HIDIOCGRAWNAME(len): Get Raw Name
 * HIDIOCGRAWPHYS(len): Get Physical Address
 * HIDIOCSFEATURE(len): Send a Feature Report
 * HIDIOCGFEATURE(len): Get a Feature Report
 */

#ifndef CANCTL_H_
#define CANCTL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <stdlib.h> // size_t

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
// Various globals
#define CANBUS_VID                  0x04d8 // Microchip default Vendor ID
#define CANBUS_PID                  0x003f // Microchip default Product ID
#define GPIO_VID                    0x04d8 // Microchip default Vendor ID
#define GPIO_PID                    0x004f // Microchip default Product ID
#define CANBUS_MSG_SIZE             64 // Max recv'd CANbus message size
#define CANBUS_FIRMWARE_SIZE        3
#define CANBUS_ERROR_STATE_SIZE     3
#define CANBUS_DEFAULT_TIMEOUT_MS   10000 // Read timeout in milliseconds
#define CANBUS_MAX_BPS              1000000 // Max bus speed in bits/second
#define GET_ESTATE_STR(byte, flag) (((byte & flag) == flag) ? "yes" : "no")
#define GPIO_PIN_COUNT              8 // Number of GPIO pins (8)

//GPIO Subcommands and Responses
#define GPIO_READ_PIN_TYPE_CMD      0x01
#define GPIO_SET_PIN_TYPE_CMD       0x02
#define GPIO_SET_PIN_TYPE_RESPONSE  0x00
#define GPIO_READ_PIN_TYPE_RESPONSE 0x01
#define GPIO_READ_PIN_DATA_CMD      0x01
#define GPIO_SET_PIN_DATA_CMD       0x02
#define GPIO_SET_PIN_DATA_RESPONSE  0x00

#define PIN_DATA 1
#define PIN_TYPE 0
#define GET_IOM 1
#define GET_BOARD_ID 0

/**
 * CANbus error state flags
 */
typedef enum canbus_estate_flags
{
	CANBUS_ESTATE_TXRX_WARN = 0x1,    // bit 1
	CANBUS_ESTATE_RX_WARN = 0x2,      // bit 2
	CANBUS_ESTATE_TX_WARN = 0x4,      // bit 3
	CANBUS_ESTATE_RX_PASSIVE = 0x8,   // bit 4
	CANBUS_ESTATE_TX_PASSIVE = 0x10,  // bit 5
	CANBUS_ESTATE_TX_OFF = 0x20,      // bit 6
} canbus_estate_flags_t;

/**
 * @todo Document
 */
typedef enum canbus_led
{
	CANBUS_LED_OFF,
	CANBUS_LED_ON,
	CANBUS_LED_NORMAL,
	CANBUS_LED_UNKNOWN,
} canbus_led_t;

/**
 * @todo Document
 */
typedef enum canbus_cfg
{
	CANBUS_CFG_ERROR = -1,
	CANBUS_CFG_NORMAL = 0x00,
	CANBUS_CFG_DISABLE = 0x01,
	CANBUS_CFG_LOOPBACK = 0x02,
	CANBUS_CFG_LISTEN_ONLY = 0x03,
	CANBUS_CFG_CONFIGURATION = 0x04,
	CANBUS_CFG_RESERVED_1 = 0x05, // Never valid
	CANBUS_CFG_RESERVED_2 = 0x06, // Never valid
	CANBUS_CFG_LISTEN_ALL_MESSAGE = 0x07,
	CANBUS_CFG_UNKNOWN // This and higher values are never valid
} canbus_cfg_t;


const unsigned char *canctl_get_firmware_version(int fd);
canbus_cfg_t canctl_get_config(int fd);
int canctl_set_config(int fd, canbus_cfg_t cfg, unsigned int speed);
int canctl_read(int fd, unsigned char *buf, size_t len);
int canctl_write(int fd, unsigned char *buf, size_t len);
int canctl_set_led(int fd, canbus_led_t mode);
void canctl_set_timeout_ms(int ms);
int canctl_get_timeout_ms(void);
const char *canctl_config_to_string(canbus_cfg_t cfg);
const unsigned char *canctl_get_error_state(int fd);
int gpio_set_pin(int fd, int op_type, unsigned char *pin_types);
int gpio_read_pin(int fd, int op_type, unsigned char *pin_types);
int gpio_get_iom_or_sku(int fd, int op_select, unsigned char *outbuf);

#ifdef __cplusplus
}
#endif

#endif // CANCTL_H_
