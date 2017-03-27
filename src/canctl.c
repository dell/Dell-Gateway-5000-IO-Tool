/**
 * @file canctl.h
 * @date 2017-03-24
 */

#include "canctl.h"
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int _timeout_ms = CANBUS_DEFAULT_TIMEOUT_MS;
void canctl_set_timeout_ms(int ms) { _timeout_ms = ms; }
int canctl_get_timeout_ms(void) { return (_timeout_ms); }

/**
 * Write data to the device at file descriptor @c fd. Assume device is
 * already open and is non-blocking. Assume @c buf has already been
 * set to @c len bytes.
 * @param fd CANbus module's file descriptor
 * @param buf Data to write to the device
 * @param len Length of buffer @c buf
 * @returns Returns number of bytes written on success, -1 on error
 */
int canctl_write(int fd, unsigned char *buf, size_t len)
{
	if (buf == NULL)
		return (-1); // @todo Return a better error indicator
	return (write(fd, buf, len)); // @todo Return a better error indicator
} // canctl_write()

/**
 * Reads data from the device at file descriptor @c fd. Assume device
 * is already open and is non-blocking. Assume @c buf has already been
 * set to @c len bytes and its memory is cleared.
 * @param fd CANbus module's file descriptor
 * @param buf Buffer to read data into
 * @param len Length of buffer @c buf
 * @returns Returns the number of bytes read on success, -1 on error
 */
int canctl_read(int fd, unsigned char *buf, size_t len)
{
	int rc;
	fd_set rdset;
	struct timeval tv = { .tv_sec = 0, .tv_usec = _timeout_ms * 1000 };
	struct timeval *tvptr = _timeout_ms < 0 ? NULL : &tv;

	if (buf == NULL)
		return (-1); // @todo Return a better error indicator
	if (len > CANBUS_MSG_SIZE)
		return (-1); // @todo Return a better error indicator

	// Set up the variables for the select() call
	FD_ZERO(&rdset);
	FD_SET(fd, &rdset);

	// Wait for the file descriptor to become readable, or timeout
	if ((rc = select(fd+1, &rdset, NULL, NULL, tvptr)) < 0)
		return (-1); // @todo Return a better error indicator
	else if (rc == 0)
		return (0); // @todo Return a better error indicator
	else if (FD_ISSET(fd, &rdset))
		return (read(fd, buf, len));

	// NOTE: THIS SHOULD NEVER HAPPEN
	return (-1); // @todo Return a better error indicator
} // canctl_read()

/**
 * Gets the firmware version from the CANbus or GPIO module. The returned
 * buffer will be CANBUS_FIRMWARE_SIZE bytes long.
 * @param fd The CANbus module's already open file descriptor
 * @returns Returns a pointer to firmware buffer on success, NULL on error.
 */
const unsigned char *canctl_get_firmware_version(int fd)
{
	static unsigned char fw[CANBUS_FIRMWARE_SIZE];
	unsigned char buf[CANBUS_MSG_SIZE];

	// Clear memory to be safe
	memset(fw, 0, sizeof(fw));
	memset(buf, 0, sizeof(buf));

	// Write
	buf[0] = CANBUS_OUT_FW_VERSION;
	buf[1] = 0; // Data payload is empty
	if (canctl_write(fd, buf, 2) != 2)
		return (NULL);

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (NULL);

	// Make sure the first byte is the correct report ID
	if (buf[0] != CANBUS_IN_FW_VERSION)
		return (NULL);

	memcpy(fw, &buf[1], CANBUS_FIRMWARE_SIZE);
	return (fw);
} // canctl_get_firmware_version()

/**
 * Get the CANbus module's current configuration
 * @param fd The CANbus module's already open file descriptor
 * @returns Returns the CANbus configuration, -1 on error.
 */
canbus_cfg_t canctl_get_config(int fd)
{
	unsigned char buf[CANBUS_MSG_SIZE];
	canbus_cfg_t cfg;

	// Write
	buf[0] = CANBUS_OUT_GET_CONFIG;
	buf[1] = 0; // Data payload is empty
	if (canctl_write(fd, buf, 2) != 2)
		return (-1); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (-1); // @todo Return a better error indicator

	// Make sure the first byte is the correct report ID
	if (buf[0] != CANBUS_IN_GET_CONFIG)
		return (-1); // @todo Return a better error indicator

	cfg = buf[1];
	return (cfg);
} // canctl_get_config()

/**
 * Sets the gateway's CANbus configuration and bus speed. Note that the speed
 * parameter is only used when the mode is CANBUS_CFG_CONFIGURATION.
 * @param fd The already opened CANbus module's file descriptor
 * @param cfg The new CANbus configuration
 * @param speed The CANbus speed from 136kbps to 1Mbps (136000 to 1000000)
 * @returns Returns 0 on success, -1 on error.
 */
int canctl_set_config(int fd, canbus_cfg_t cfg, unsigned int speed)
{
	unsigned char buf[CANBUS_MSG_SIZE];
	int len;

	// Write
	buf[0] = CANBUS_OUT_SET_CONFIG;
	buf[1] = cfg;
	len = 2;
	if (cfg == CANBUS_CFG_CONFIGURATION)
	{
		buf[2] = (speed >> 24) & 0xff;
		buf[3] = (speed >> 16) & 0xff;
		buf[4] = (speed >> 8) & 0xff;
		buf[5] = (speed >> 0) & 0xff;
		len = 6;
	}
	if (canctl_write(fd, buf, len) != len)
		return (-1); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (-1); // @todo Return a better error indicator

	// Make sure the first byte is the correct report ID
	if (buf[0] != CANBUS_IN_SET_CONFIG)
		return (-1); // @todo Return a better error indicator

	return (0);
} // canctl_set_config()

/**
 * Sets the gateway's LED to on, off, or normal operation.
 * @param fd The already opened CANbus module's file descriptor
 * @param mode The new LED mode
 * @returns Returns 0 on success, -1 on error.
 */
int canctl_set_led(int fd, canbus_led_t mode)
{
	unsigned char buf[CANBUS_MSG_SIZE];
	int rpt;

	switch (mode)
	{
		case CANBUS_LED_OFF:
			rpt = CANBUS_OUT_LED_OFF;
			break;
		case CANBUS_LED_ON:
			rpt = CANBUS_OUT_LED_ON;
			break;
		case CANBUS_LED_NORMAL:
			rpt = CANBUS_OUT_LED_NORMAL;
			break;
		default:
			return (-1); // @todo Return a better error indicator
	}

	// Write
	buf[0] = rpt;
	buf[1] = 0; // Data payload is empty
	if (canctl_write(fd, buf, 2) != 2)
		return (-1); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (-1); // @todo Return a better error indicator

	// Make sure the first byte is the correct report ID
	if (buf[0] != rpt)
		return (-1); // @todo Return a better error indicator

	return (0);
} // canctl_set_led()

/**
 * Converts a CANbus configuration enum value to a readable string
 * @param cfg The CANbus configuration to convert
 * @returns A converted, readable string
 */
const char *canctl_config_to_string(canbus_cfg_t cfg)
{
	switch (cfg)
	{
		case CANBUS_CFG_NORMAL: return ("Normal");
		case CANBUS_CFG_DISABLE: return ("Disabled");
		case CANBUS_CFG_LOOPBACK: return ("Loopback");
		case CANBUS_CFG_LISTEN_ONLY: return ("Listen Only");
		case CANBUS_CFG_CONFIGURATION: return ("Configuration");
		case CANBUS_CFG_RESERVED_1: return ("Reserved");
		case CANBUS_CFG_RESERVED_2: return ("Reserved");
		case CANBUS_CFG_LISTEN_ALL_MESSAGE: return ("Listen All Messages");
		case CANBUS_CFG_UNKNOWN: return ("Unknown");
		default: return ("Error");
	}
} // canctl_config_to_string()

/**
 * @todo Document
 */
const unsigned char *canctl_get_error_state(int fd)
{
	static unsigned char estate[CANBUS_FIRMWARE_SIZE];
	unsigned char buf[CANBUS_MSG_SIZE];

	// Write
	buf[0] = CANBUS_OUT_ERROR_STATUS;
	buf[1] = 0; // Data payload is empty
	if (canctl_write(fd, buf, 2) != 2)
		return (NULL); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (NULL); // @todo Return a better error indicator

	// Make sure the first byte is the correct report ID
	if (buf[0] != CANBUS_IN_ERROR_STATUS)
		return (NULL); // @todo Return a better error indicator

	memcpy(estate, &buf[1], CANBUS_ERROR_STATE_SIZE);
	return (estate);
} // canctl_get_error_status()

/**
 * Writes the Pin Type/Direction Settings OR pin data for each GPIO PIN
 * @param fd The already opened GPIO module's file descriptor
 * @returns Returns 0 on success, -1 on error.
 */
int gpio_set_pin(int fd, int op_type, unsigned char *pin_types)
{
	unsigned char buf[GPIO_PIN_COUNT+2];

	// Write command and subcommand
	buf[0] = (op_type == PIN_TYPE) ? GPIO_OUT_SET_PIN_TYPE : GPIO_OUT_SET_PIN_DATA;
	buf[1] = (op_type == PIN_TYPE) ? GPIO_SET_PIN_TYPE_CMD : GPIO_SET_PIN_DATA_CMD;
	buf[2] = pin_types[0];
	buf[3] = pin_types[1];
	buf[4] = pin_types[2];
	buf[5] = pin_types[3];
	buf[6] = pin_types[4];
	buf[7] = pin_types[5];
	buf[8] = pin_types[6];
	buf[9] = pin_types[7];
	if (canctl_write(fd, buf, 10) != 10)
		return (-1); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (-1); // @todo Return a better error indicator

	// Make sure the first byte is the command that was issued
	if (op_type == PIN_TYPE) {
		if (buf[0] != GPIO_IN_SET_PIN_TYPE)
		return (-1); // @todo Return a better error indicator
	} else {
		if (buf[0] != GPIO_IN_SET_PIN_DATA)
		return (-1); // @todo Return a better error indicator
	}
	
	// Make sure the second byte is the correct subcommand response
	if (op_type == PIN_TYPE) {
		if (buf[1] != GPIO_SET_PIN_TYPE_RESPONSE)
		return (-1); // @todo Return a better error indicator
	} else {

		if (buf[1] != GPIO_SET_PIN_DATA_RESPONSE)
		return (-1); // @todo Return a better error indicator
	}


	return (0);
} // gpio_set_pin_type()


/**
 * Reads the Pin Type/Direction Settings or PIN DATA for each GPIO PIN
 * @param fd The already opened GPIO module's file descriptor
 * @returns Returns 0 on success, -1 on error.
 */
int gpio_read_pin(int fd, int op_type, unsigned char *pin_types)
{
	unsigned char buf[GPIO_PIN_COUNT+2];

	// Write command and subcommand
	buf[0] = (op_type == PIN_TYPE) ? GPIO_OUT_READ_PIN_TYPE : GPIO_OUT_READ_PIN_DATA;
	buf[1] = (op_type == PIN_TYPE) ? GPIO_READ_PIN_TYPE_CMD : GPIO_READ_PIN_DATA_CMD;
	
	if (canctl_write(fd, buf, 2) != 2) {
		return (-1); // @todo Return a better error indicator
                
	}

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0) {
		return (-1); // @todo Return a better error indicator
                
	}
	// Make sure the first byte is the command that was issued
	if (op_type == PIN_TYPE) {
		if (buf[0] != GPIO_IN_READ_PIN_TYPE)
		return (-1); // @todo Return a better error indicator
	} else { 
		if (buf[0] != GPIO_IN_READ_PIN_DATA)
		return (-1); // @todo Return a better error indicator
	}

	// Make sure the second byte is the correct subcommand response
	if (op_type == PIN_TYPE) {
		if (buf[1] != GPIO_READ_PIN_TYPE_RESPONSE)
		return (-1); // @todo Return a better error indicator
	} else {
		if (buf[1] != GPIO_READ_PIN_DATA_CMD)
		return (-1); // @todo Return a better error indicator
	}
		

	//Responses were correct, so fill pin type array with values
	pin_types[0] = buf[2];
	pin_types[1] = buf[3];
	pin_types[2] = buf[4];
	pin_types[3] = buf[5];
	pin_types[4] = buf[6];
	pin_types[5] = buf[7];
	pin_types[6] = buf[8];
	pin_types[7] = buf[9];
	
	return (0);
} // gpio_read_pin_type()

/**
 * Get the IO Module SKU or GPIO PIC Board ID
 * @param fd The GPIO module's already open file descriptor
 * @returns Returns , -1 on error.
 */
int gpio_get_iom_or_sku(int fd, int op_select, unsigned char *outbuf)
{
	unsigned char buf[CANBUS_MSG_SIZE];

	// Write
	buf[0] = (op_select == GET_IOM) ? GPIO_OUT_GET_IOM_SKU : GPIO_IN_GET_BOARD_ID;
	buf[1] = 0; // Data payload is empty
	if (canctl_write(fd, buf, 2) != 2)
		return (-1); // @todo Return a better error indicator

	// Read
	memset(buf, 0, sizeof(buf));
	if (canctl_read(fd, buf, sizeof(buf)) < 0)
		return (-1); // @todo Return a better error indicator

	// Make sure the first byte is the correct report ID
 	if (op_select == GET_IOM) { 	
		if (buf[0] != GPIO_IN_GET_IOM_SKU)
			return (-1); // @todo Return a better error indicator
	} else {
		if (buf[0] != GPIO_IN_GET_BOARD_ID)
			return (-1); // @todo Return a better error indicator
	}

	outbuf[0] = buf[1];

	return (0);
} // gpio_get_iom_or_sku()
