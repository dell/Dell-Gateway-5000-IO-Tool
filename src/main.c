/**
 * @file main.c
 * @date 2017-03-24
 */

#include "version.h"
#include "cfg.h"
#include "args.h"
#include "canctl.h"

// #include <linux/types.h>
#include <linux/input.h> // BUS_* macros
#include <linux/hidraw.h>
#include <sys/ioctl.h>
// #include <sys/types.h>
// #include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h> // for searching /dev directory
#include <signal.h>
#include <time.h>

// Used during printf() output in some cases for making text pretty. A
// negative number means left-align the text and fill with spaces on the right.
#define PAD -15

// Default program configuration
static cfg_t cfg = {
	.path = { 0 },
	.timeout_ms = CANBUS_DEFAULT_TIMEOUT_MS,
	.list_hids = 0,
	.verbose = 0
};

// This static ints is used as the file descriptors for the CANbus/GPIO module.
// It is opened in the beginning part of main() using open().
static int fd = -1;
static int fd_can = -1;
static int fd_gpio = -1;

// This int serves as a global variable used during the read and write
// operation modes. It is used in conjunction with the signal handling
// function handle_signal_while_reading_or_writing().
static int keep_reading_or_writing;

// Various UI menu and helper functions
static void mnu_write(void);
static void mnu_read(void);
static void mnu_set_config(void);
static void mnu_set_led(void);
static void mnu_usb_self_test(void);
static void mnu_can_self_test(void);
static void mnu_can_loopback_test(void);
static void mnu_set_timeout(void);
static const char *bus_to_str(int bus);
static void list_hids(void);
static void list_gpio_pin(int type_or_data);
static void handle_signal_while_reading_or_writing(int signo);
static void flush_stdin(void);
static void print_bytes(FILE *fs, unsigned char *buf, size_t len, char pad);
static void mnu_gpio_set_pin(int type_or_data);
static void mnu_gpio_get_iom_or_sku(int op_select);

/**
 * Main program entry point
 * @param argc Number of command-line arguments
 * @param argv Command-line arguments in string array
 * @returns Returns 0 on success. Otherwise, error.
 */
int main(int argc, char **argv)
{
	int rc; // Used for the return code of almost all functions
	int keep_going; // Used to control while loops
	unsigned char buf[256]; // Generic buffer

	/* BEGIN ARGUMENT PARSING STUFF */
	const char *env_extras = "ARGP_HELP_FMT=dup-args,no-dup-args-note";
	putenv((char *)env_extras);
	argp_parse(&argp, argc, argv, 0, 0, &cfg);
	/* END ARGUMENT PARSING STUFF */

	if (cfg.list_hids)
	{
		list_hids();
		return (0);
	}

	canctl_set_timeout_ms(cfg.timeout_ms);

	// If the user supplied a --path PATH argument, then skip
	// this if-statement. Otherwise, search through the /dev
	// directory for HID devices until finding the CANbus HID.
	if (strlen(cfg.path) == 0)
	{
		printf("Searching for CANbus and GPIO modules\n");

		DIR *dir;
		struct dirent *ent;
		char *devdir = "/dev/";
                int is_gpio = 0;

		if ((dir = opendir(devdir)) == NULL)
		{
			printf("ERROR: Could not search /dev directory: %s",
				strerror(errno));
			return (-1);
		}

		// Loop through all the entries in this directory looking for hidraw
		//keep_going = 1;
		//while ((ent = readdir(dir)) != NULL && keep_going)
		while ((ent = readdir(dir)) != NULL)
		{
			// Found a directory entry. Parse it to see if it's hidraw*
			if (strstr(ent->d_name, "hidraw") != NULL)
			{
				// This directory is /dev/hidraw*, so check if it's CANbus.
				// Make the absolute file path to pass to open()
				char devpath[strlen(devdir) + strlen(ent->d_name) + 1];
				memset(devpath, 0, sizeof(devpath));
				strncpy(devpath, devdir, strlen(devdir));
				strncat(devpath, ent->d_name, strlen(ent->d_name));

				// Open the device in non-blocking mode
				if ((fd = open(devpath, O_RDWR|O_NONBLOCK)) < 0)
				{
					printf("WARNING: Found potential module at %s, but a "
						"problem occurred trying to open it: %s\n",
						devpath, strerror(errno));
					errno = 0; // Clear error just in case
					continue;
				}

				// Device is now open. Get device's raw name
				memset(buf, 0, sizeof(buf));
				rc = ioctl(fd, HIDIOCGRAWNAME(sizeof(buf)), buf);
				if (rc < 0)
				{
					close(fd);
					fd = -1;
					continue;
				}

				// Check if this /dev/hidraw* is a CANbus or GPIO device
				if ((strstr((char *)buf, "CANBus") == NULL) && (strstr((char *)buf, "GPIO") == NULL))
				{
					close(fd);
					fd = -1;
					continue; // This HID device is NOT a CANbus nor a GPIO device
				} else { //Drill down to whether it's a GPIO device
                                     if (strstr((char *)buf, "GPIO") != NULL) {
					is_gpio = 1;
				     }
				}



				// To make it here, this is a potential CANbus module.
				// Print information about it to the user.
				printf("Found device:\n");
				printf("  %*s: %s\n", PAD, "File path", devpath);
				printf("  %*s: %s\n", PAD, "Description", buf);

				// Get some other information about this device.
				// First get the bus type, vendor and product ids
				struct hidraw_devinfo info;
				memset(&info, 0, sizeof(info));
				if ((rc = ioctl(fd, HIDIOCGRAWINFO, &info)) < 0)
				{
					printf("  %*s: unknown\n", PAD, "Bus type");
					printf("  %*s: unknown\n", PAD, "Vendor ID");
					printf("  %*s: unknown\n", PAD, "Product ID");
				}
				else
				{
					printf("  %*s: %d (%s)\n", PAD, "Bus type",
						info.bustype, bus_to_str(info.bustype));
					printf("  %*s: 0x%04x\n", PAD, "Vendor ID", info.vendor);
					printf("  %*s: 0x%04x\n", PAD, "Product ID", info.product);
				}

				// Now get the physical address
				memset(buf, 0, sizeof(buf));
				if ((rc = ioctl(fd, HIDIOCGRAWPHYS(256), buf)) < 0)
					printf("  %*s: unknown\n", PAD, "Phys. Address");
				else
					printf("  %*s: %s\n", PAD, "Phys. Address", buf);

				int ans;
				printf("\nDo you want to use this device (y/n)? ");
				ans = getchar();
				flush_stdin();
				switch (ans)
				{
					case 'Y':
					case 'y':
						if (is_gpio) {
							//If we are setting up a GPIO device, copy the descriptor
							fd_gpio = fd;
							is_gpio = 0;
						} else {
							fd_can = fd;
						}
						//keep_going = 0;
						//break;
						continue;
					default:
						printf("ERROR: Invalid input. Skipping device.\n");
					case 'N':
					case 'n':
						is_gpio = 0;
						close(fd);
						fd = -1;
						continue;
				}
			} // if (strstr(ent->d_name, "hidraw") != NULL)
		} // while ((ent = readdir(dir)) != NULL && keep_going)
		closedir(dir);

		// Check to make sure a device was found
		if ((fd_can < 0) && (fd_gpio < 0))
		{
			printf("ERROR: No CANbus or GPIO devices found or none selected\n");
			return (-1);
		} else {
			printf("CANBus Device Status: %s\n", (fd_can < 0) ? "NOT FOUND/UNSELECTED" : "SELECTED");
			printf("GPIO Device Status: %s\n", (fd_gpio < 0) ? "NOT FOUND/UNSELECTED" : "SELECTED");
		}
	}
	else
	{
		// Open the device in non-blocking mode
		if ((fd = open(cfg.path, O_RDWR|O_NONBLOCK)) < 0)
		{
			printf("ERROR: Could not open device at %s: %s\n",
				cfg.path, strerror(errno));
			return (-1);
		}
		//Find out whether this is a CAN or GPIO device
		int ans;
		printf("\nIs this a CAN device or a GPIO Device? (1 = CAN, 2 = GPIO)");
		ans = getchar();
		flush_stdin();
		switch (ans)
		{
			case '1':
				fd_can = fd;
				printf("\nCAN Device Chosen.\n");
			case '2':
				fd_gpio = fd;
				printf("\nGPIO Device Chosen.\n");
			default:
				printf("ERROR: Please Choose one of the above. Exiting...\n");
		}
	}

	// To get to this point the device MUST be found and MUST be opened.
	// Now ask the user what they want to do.

	keep_going = 1;
	while (keep_going)
	{
		printf(
			"\nMAIN MENU:\n"
			"1 - CANBus Read...\n"
			"2 - CANBus Write...\n"
			"3 - Get firmware version (GPIO and CANBus)\n"
			"4 - Get CANBus configuration mode\n"
			"5 - Set configuration mode...\n"
			"6 - USB Channel self test (CANBus Device path)\n"
			"7 - CANbus self test\n"
			"8 - CANbus loopback test\n"
			"9 - Get CANBus error status\n"
			"10- Set CANBus LED...\n"
			"11- List all HID devices\n"
			"12- Set CANBus read timeout...\n"
			"13- Get CANBus read timeout\n"
			"14- Get GPIO pin direction settings\n"
			"15- Set GPIO pin directions\n"
			"16- Get GPIO pin Data\n"
			"17- Set GPIO pin Data (for OUTPUT pins only)\n"
			"18- Get GPIO board ID\n"
			"19- Get IO Module SKU ID (GPIO Device Path)\n"
			"0 - Quit\n"
			"> ");

		int userinput;
		if ((rc = scanf("%d.*[^\n]", &userinput)) == EOF || rc == 0)
		{ // An error occurred or no matches were found
			flush_stdin();
			userinput = -1;
		}

		// Now do what the user wanted
		switch (userinput)
		{
			case 1: // Read...
				mnu_read();
				break;
			case 2: // Write...
				mnu_write();
				break;
			case 3: // Get firmware version
			{
				printf("\n");
				const unsigned char *can_fw;
				const unsigned char *gpio_fw;
				
				if(fd_can < 0) { 
					printf("CANBus Device Not Selected, skipping\n"); 
				} 
				else {
					if ((can_fw = canctl_get_firmware_version(fd_can)) == NULL)
					printf("ERROR: A problem occurred retrieving CANBus firmware version\n");
					else
					{
						printf("CANBus Firmware Version: ");
						for (int i = 0; i < CANBUS_FIRMWARE_SIZE; i++)
							printf("%02x ", can_fw[i]);
						printf("\n");
					}
				}

				if(fd_gpio < 0) { 
					printf("GPIO Device Not Selected, skipping\n"); 
				} 
				else {
					if ((gpio_fw = canctl_get_firmware_version(fd_gpio)) == NULL)
					printf("ERROR: A problem occurred retrieving GPIO firmware version\n");
					else
					{
						printf("GPIO Firmware Version: ");
						for (int i = 0; i < CANBUS_FIRMWARE_SIZE; i++)
							printf("%02x ", gpio_fw[i]);
						printf("\n");
					}
				}

				break;
			}
			case 4: // Get configuration mode
				printf("\nCurrent configuration: %s\n",
					canctl_config_to_string(canctl_get_config(fd_can)));
				break;
			case 5: // Set configuration mode...
				mnu_set_config();
				break;
			case 6: // USB self test
				mnu_usb_self_test();
				break;
			case 7: // CANbus self test
				mnu_can_self_test();
				break;
			case 8: // CANbus loopback test
				mnu_can_loopback_test();
				break;
			case 9: // Get error status
			{
				const unsigned char *estate;
				if ((estate = canctl_get_error_state(fd_can)) == NULL)
					printf("ERROR: A problem occurred\n");
				else
				{
					printf("\nERROR STATUS:\n");
					printf("  %*s: %d\n", PAD, "Tx Errors", estate[0]);
					printf("  %*s: %d\n", PAD, "Rx Errors", estate[1]);
					printf("  %*s: %s\n", PAD, "TxRx Warning",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_TXRX_WARN));
					printf("  %*s: %s\n", PAD, "Rx Warning",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_RX_WARN));
					printf("  %*s: %s\n", PAD, "Tx Warning",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_TX_WARN));
					printf("  %*s: %s\n", PAD, "Rx Bus Passive",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_RX_PASSIVE));
					printf("  %*s: %s\n", PAD, "Tx Bus Passive",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_TX_PASSIVE));
					printf("  %*s: %s\n", PAD, "Tx Bus Off",
						GET_ESTATE_STR(estate[2], CANBUS_ESTATE_TX_OFF));
				}
				break;
			}
			case 10: // Set LED on/off/default...
				mnu_set_led();
				break;
			case 11: // List all HID devices
				list_hids();
				break;
			case 12: // Set read timeout...
				mnu_set_timeout();
				break;
			case 13: // Get read timeout
				printf("Timeout (ms): %d\n", canctl_get_timeout_ms());
				break;
			case 14: // List GPIO pin type settings
                                list_gpio_pin(PIN_TYPE); 
				break;
			case 15: // Set GPIO pin type settings
                                mnu_gpio_set_pin(PIN_TYPE);
				break;
			case 16: // List GPIO pin data
                                list_gpio_pin(PIN_DATA); 
				break;
			case 17: // Set GPIO pin type settings
                                mnu_gpio_set_pin(PIN_DATA);
				break;
			case 18: // Get GPIO board ID
				mnu_gpio_get_iom_or_sku(GET_BOARD_ID);
				break;
			case 19: // Get IO Module SKU ID (GPIO Device Path)
				mnu_gpio_get_iom_or_sku(GET_IOM);
				break;
			case 0: // Quit
				keep_going = 0;
				break;
			default: // userinput was probably -1 because of an error
				printf("ERROR: Invalid input. Please try again.\n");
				break;
		}
	} // end while(keep_going)

	printf("Closing devices\n");
	if(!(fd_can < 0)) close(fd_can);
	if(!(fd_gpio < 0)) close(fd_gpio);
	printf("Bye\n");
	return (0);
} // main()

/**
 * Reads character-by-character from stdin until EOF or '\n' is found,
 * discarding each character as it goes. Thus, clearing the stdin buffer.
 */
void flush_stdin(void)
{
	int c;
	while ((c = fgetc(stdin)) != EOF && c != '\n') ;
	return;
} // flush_stdin()

/**
 * This signal handler gets registered in the mnu_read() and mnu_write()
 * functions. It catches all signals, but only processes SIGINT signals.
 * It simply clears a global flag (keep_reading_or_writing) which is used for
 * loop control in the mnu_read() and mnu_write() routines.
 * @param signo The function typedef specifies this as the caught signal number
 */
void handle_signal_while_reading_or_writing(int signo)
{
	// If this signal wasn't SIGINT, ignore it
	if (signo != SIGINT) return;
	keep_reading_or_writing = 0;
} // handle_signal_while_reading_or_writing()

/**
 * Presents a menu to the user for setting the CANbus module's configuration
 */
void mnu_set_config(void)
{
	int rc;
	canbus_cfg_t mode;
	int keep_going = 1;

	while (keep_going)
	{
		printf("\nCurrent configuration: %s\n",
			canctl_config_to_string(canctl_get_config(fd_can)));
		printf(
			"\nCONFIGURATION MODES:\n"
			"1 - Normal\n"
			"2 - Disabled\n"
			"3 - Loopback\n"
			"4 - Listen Only\n"
			"5 - Configuration\n"
			"6 - Listen All Messages\n"
			"0 - Go back\n"
			"> ");

		int userinput;
		if ((rc = scanf("%d.*[^\n]", &userinput)) == EOF || rc == 0)
		{ // An error or no matches were found, so flush stdin
			flush_stdin();
			userinput = -1;
		}

		// Now do what the user wanted
		switch (userinput)
		{
			case 1: // Normal
				mode = CANBUS_CFG_NORMAL;
				keep_going = 0;
				break;
			case 2: // Disabled
				mode = CANBUS_CFG_DISABLE;
				keep_going = 0;
				break;
			case 3: // Loopback
				mode = CANBUS_CFG_LOOPBACK;
				keep_going = 0;
				break;
			case 4: // Listen Only
				mode = CANBUS_CFG_LISTEN_ONLY;
				keep_going = 0;
				break;
			case 5: // Configuration
				mode = CANBUS_CFG_CONFIGURATION;
				keep_going = 0;
				break;
			case 6: // Listen All Messages
				mode = CANBUS_CFG_LISTEN_ALL_MESSAGE;
				keep_going = 0;
				break;
			case 0: // Go back
				return;
			default: // Unknown error occurred
				printf("ERROR: Invalid input. Please try again.\n");
				break;

		}
	} // end while (keep_going)

	/**
	 * @todo Ask the user what speed they want to set when they pick
	 * CANBUS_CFG_CONFIGURATION. Otherwise default to CANBUS_MAX_BPS always
	 */

	// Write a new configuration
	if ((rc = canctl_set_config(fd_can, mode, CANBUS_MAX_BPS)) < 0)
		printf("ERROR: An unknown problem occurred\n");

	printf("Wrote config, now verifying\n");
	canbus_cfg_t c = canctl_get_config(fd_can);
	if (c == mode)
		printf("Success. New configuration: %s\n", canctl_config_to_string(c));
	else
		printf("ERROR: New configuration could not be verified\n");

} // mnu_set_config()

/**
 * Enters into "Read" mode -- a hands off mode where any data that
 * that arrives on the CANbus port will be displayed to the screen.
 * This mode will continue until the user presses Ctrl+c (SIGINT).
 */
void mnu_read(void)
{
	int rc, nbytes;
	struct sigaction act, oldact;
	act.sa_handler = handle_signal_while_reading_or_writing;
	keep_reading_or_writing = 1;

	printf("\n");

	/**
	 * @todo Check the current CANbus configuration.
	 * Some configurations don't allow for reading.
	 */

	// Try to set the SIGINT signal handler so that during the
	// continuous read the user can press Ctrl+c to stop.
	if ((rc = sigaction(SIGINT, &act, &oldact)) < 0)
	{
		printf("WARNING: Could not set stop signal for read "
			"mode. Will only read once.\n");
		keep_reading_or_writing = 0;
	}
	else
	{
		printf("Now entering read mode. Press Ctrl+c to exit...\n");
	}
	// Read from the CANbus module. This do-while loop ends when
	// the user presses Ctrl+c, or after one iteration if there was
	// an error setting the signal handler above.
	do
	{
		unsigned char buf[CANBUS_MSG_SIZE];
		memset(buf, 0, sizeof(buf));
		if ((nbytes = canctl_read(fd_can, buf, sizeof(buf))) < 0)
		{
			if (errno != EINTR)
				printf("ERROR: A problem occurred: %s\n", strerror(errno));
			else
				printf("\nLeaving read mode\n");
			break;
		}
		else if (nbytes == 0)
		{
			printf("Timeout\n");
		}
		else
		{
			printf("Read %d bytes:\n", nbytes);
			print_bytes(stdout, buf, nbytes, 2);
		}
	} while (keep_reading_or_writing);
	// Reset the old SIGINT action, if it was originally changed
	if (rc == 0)
		sigaction(SIGINT, &oldact, NULL);
	return;
} // mnu_read()

/**
 * Enters into "Write" mode -- an interactive mode where the user can execute
 * manual CANbus commands 1-by-1. The user is expected to know the commands
 * and payloads to send. This mode ends when the user presses Ctrl+c.
 */
void mnu_write(void)
{
	int rc, nbytes, c, msg_valid;
	size_t i; // For indexing through loops
	struct sigaction act, oldact;
	char userinput[CANBUS_MSG_SIZE * 2]; // Arbitrarily big enough
	unsigned char msg[CANBUS_MSG_SIZE]; // CANbus message output
	char *tok; // A token of user input
	long num; // The potential number returned from strtol()
	fd_set rdset;

	act.sa_handler = handle_signal_while_reading_or_writing;
	keep_reading_or_writing = 1;

	printf("\n");

	/**
	 * @todo Check the current CANbus configuration.
	 * Some configurations don't allow for writing.
	 */

	// Try to set the SIGINT signal handler so that during the
	// continuous write the user can press Ctrl+c to stop.
	if ((rc = sigaction(SIGINT, &act, &oldact)) < 0)
	{
		printf("WARNING: Could not set stop signal for write "
			"mode. Will only read once.\n");
		keep_reading_or_writing = 0;
	}
	else
	{
		printf(
			"All messages must conform to the following rules:\n"
			"  - at least 2 bytes\n"
			"  - no longer than 64 bytes\n"
			"  - in hex format\n"
			"  - space-delimited per byte\n"
			"  - first byte is the report descriptor (the command)\n"
			"  - second byte begins the payload, or 0 if no payload\n"
			"e.g. To get the firmware version you would issue 'ec 0'\n"
			"\n"
			"Now entering write mode. Press Ctrl+c to exit...\n");
	}
	// Write to the CANbus module. This do-while loop ends when
	// the user presses Ctrl+c or after one iteration if there was
	// an error setting the signal handler above.
	flush_stdin();
	do
	{
		memset(userinput, 0, sizeof(userinput));

		// Since we need to catch the Ctrl+c signal, we must use select().
		// Thus, we need to set up the fd_set for listening on stdin
		FD_ZERO(&rdset);
		FD_SET(STDIN_FILENO, &rdset);

		printf("> ");
		fflush(stdout); // Force the "> " to be printed

		// Get user input. The select() call will return when it is
		// interrupted via a system signal (such as SIGINT), or when stdin
		// is available for reading (after user presses ENTER).
		if ((rc = select(STDIN_FILENO+1, &rdset, NULL, NULL, NULL)) < 0)
		{
			if (errno != EINTR)
				printf("ERROR: A problem occurred: %s\n", strerror(errno));
			else
				printf("\nLeaving write mode\n");
			break;
		}
		else if (FD_ISSET(STDIN_FILENO, &rdset))
		{ // STDIN is ready for reading
			// Pull off the stdin bytes one-by-one filling up the userinput
			// buffer as we go. We stop putting the bytes into the buffer
			// as soon as it's full, but we continue flush stdin regardless
			i = 0;
			while ((c = fgetc(stdin)) != EOF && c != '\n')
				if (i < sizeof(userinput)-1) // -1 because last byte is '\0'
					userinput[i++] = c;
		}
		else
		{ // This should never happen with only stdin in the select()
			printf("ERROR: An unknown problem occurred\n");
			break;
		}

		// At this point, the user has entered something and it's in the
		// buffer, but we don't know what it is exactly, so now loop over
		// its contents and validate the string is a worthy CAN message.
		// Only loop over sizeof(msg), however, and not sizeof(userinput)
		// because it is msg we are ultimately going to send, and thus can
		// accomodate this amount of data.

		memset(msg, 0, sizeof(msg));
		msg_valid = 1; // Assume valid until proven otherwise
		for (i = 0; i < sizeof(msg); i++)
		{
			if (i == 0)
				// The first time through strtok() needs the input string
				tok = strtok(userinput, " "); // Parse once a space is found
			else
				// All subsequent times strtok() just needs NULL as input
				tok = strtok(NULL, " ");  // Parse once a space is found

			// If tok is NULL then we're done with the user input
			if (tok == NULL)
				break;

			// Get rid of all instances of '\r' and '\n', independently
			tok[strcspn(tok, "\r\n")] = 0;

			// Do the conversion to hex
			char *endptr;
			num = strtol(tok, &endptr, 16);
			if (endptr == tok || num > 255 || num < 0)
			{
				printf("ERROR: Index [%ld] '%s' is invalid. Try again.\n",
					i, tok);
				msg_valid = 0;
				break;
			}
			msg[i] = (unsigned char)num;
		} // for (i = 0; i<sizeof(msg); i++)
		// If the message is not valid, loop back to top
		if (!msg_valid)
		{
			printf("ERROR: The message was not valid.\n");
			continue;
		}
		// Finally, to get to this point we've verified the user input
		// was correct and put all the bytes into 'msg'. Now write it.
		if ((nbytes = canctl_write(fd_can, msg, i)) < 0)
		{
			printf("ERROR: Could not send message\n");
			continue;
		}
		printf("Wrote %d bytes\n", nbytes);
	} while (keep_reading_or_writing);
	// Reset the old SIGINT action, if it was originally changed
	if (rc == 0)
		sigaction(SIGINT, &oldact, NULL);
	return;
} // mnu_write()

/**
 * Presents the user with menu options concerning operation of the
 * CANbus LED. Operation modes include two override options (On and Off),
 * and one normal option (Normal). In Normal mode, the LED blinks when
 * data is incoming or outgoing.
 */
void mnu_set_led(void)
{
	int rc;
	canbus_led_t mode = CANBUS_LED_UNKNOWN;
	int keep_going = 1;

	while (keep_going)
	{
		printf(
			"\nLED MODES:\n"
			"1 - Off\n"
			"2 - On\n"
			"3 - Normal\n"
			"0 - Go back\n"
			"> ");

		int userinput;
		if ((rc = scanf("%d.*[^\n]", &userinput)) == EOF || rc == 0)
		{ // An error or no matches were found, so flush stdin
			flush_stdin();
			userinput = -1;
		}

		// Now do what the user wanted
		switch (userinput)
		{
			case 1: // Off
				mode = CANBUS_LED_OFF;
				keep_going = 0;
				break;
			case 2: // On
				mode = CANBUS_LED_ON;
				keep_going = 0;
				break;
			case 3: // Normal
				mode = CANBUS_LED_NORMAL;
				keep_going = 0;
				break;
			case 0: // Go back
				return;
			default: // Unknown error occurred
				printf("ERROR: Invalid input. Try again.\n");
				break;
		}
	}

	if (canctl_set_led(fd_can, mode) < 0)
		printf("ERROR: A problem occurred setting the LED mode\n");
	else
		printf("Success\n");

	return;
} // mnu_set_led()

/**
 * @todo Document
 */
const char *bus_to_str(int bus)
{
	switch (bus)
	{
		case BUS_USB:
			return "USB";
		case BUS_HIL:
			return "HIL";
		case BUS_BLUETOOTH:
			return "Bluetooth";
		case BUS_VIRTUAL:
			return "Virtual";
		default:
			return "Other";
	}
} // bus_to_str()

/**
 * @todo Document
 */
void print_bytes(FILE *fs, unsigned char *buf, size_t len, char pad)
{
	if (pad < 0) pad *= -1;
	char spaces[pad+1];
	memset(spaces, ' ', sizeof(spaces));
	spaces[sizeof(spaces)-1] = 0;

	size_t ncols = 16;
	size_t nrows = len / ncols;
	size_t rem = len % ncols;
	if (rem > 0) nrows++;

	// @todo Add color to stdout output
	if (fs == stdout)
	{
		for (size_t r = 0; r < nrows; r++)
		{
			printf("%s[%02ld-%02ld] ", spaces, r*ncols, r*ncols + ncols - 1);
			size_t i;
			for (size_t c = 0; c < ncols; c++)
			{
				i = r * ncols + c;
				if (i < len)
					printf("%02x ", buf[i]);
			}
			printf("\n");
		}
	}
	else
	{
		for (size_t i = 0; i < len; i++)
			fprintf(fs, "%02x ", buf[i]);
		if (len > 0)
			fprintf(fs, "\n");
	}
} // print_bytes()

/**
 * @todo Document
 */
void list_hids(void)
{
	int rc, tmpfd;
	DIR *dir;
	struct dirent *ent;
	char *devdir = "/dev/";
	unsigned char buf[256];

	if ((dir = opendir(devdir)) == NULL)
	{
		printf("ERROR: Could not search /dev directory: %s",
			strerror(errno));
		return;
	}

	// Loop through all the entries in this directory looking for hidraw
	while ((ent = readdir(dir)) != NULL)
	{
		// Found a directory entry. Parse it to see if it's hidraw*
		if (strstr(ent->d_name, "hidraw") != NULL)
		{
			// This directory is /dev/hidraw*
			char devpath[strlen(devdir) + strlen(ent->d_name) + 1];
			memset(devpath, 0, sizeof(devpath));
			strncpy(devpath, devdir, strlen(devdir));
			strncat(devpath, ent->d_name, strlen(ent->d_name));
			// Open the device in non-blocking mode
			if ((tmpfd = open(devpath, O_RDONLY|O_NONBLOCK)) < 0)
			{
				printf("WARNING: Found HID at %s, but could not open it: %s\n",
					devpath, strerror(errno));
				errno = 0; // Clear error just in case
				continue;
			}
			// Device is now open. Get device's raw name
			memset(buf, 0, sizeof(buf));
			rc = ioctl(tmpfd, HIDIOCGRAWNAME(sizeof(buf)), buf);
			if (rc < 0)
			{
				close(tmpfd);
				tmpfd = -1;
				continue;
			}
			// Print information to the user.
			printf("\nFound device:\n");
			printf("  %*s: %s\n", PAD, "File path", devpath);
			printf("  %*s: %s\n", PAD, "Description", buf);
			// Get some other information about this device.
			// First get the bus type, vendor and product ids
			struct hidraw_devinfo info;
			memset(&info, 0, sizeof(info));
			if ((rc = ioctl(tmpfd, HIDIOCGRAWINFO, &info)) < 0)
			{
				printf("  %*s: unknown\n", PAD, "Bus type");
				printf("  %*s: unknown\n", PAD, "Vendor ID");
				printf("  %*s: unknown\n", PAD, "Product ID");
			}
			else
			{
				printf("  %*s: %d (%s)\n", PAD, "Bus type",
					info.bustype, bus_to_str(info.bustype));
				printf("  %*s: 0x%04x\n", PAD, "Vendor ID", info.vendor);
				printf("  %*s: 0x%04x\n", PAD, "Product ID", info.product);
			}
			// Now get the physical address
			memset(buf, 0, sizeof(buf));
			if ((rc = ioctl(tmpfd, HIDIOCGRAWPHYS(256), buf)) < 0)
				printf("  %*s: unknown\n", PAD, "Phys. Address");
			else
				printf("  %*s: %s\n", PAD, "Phys. Address", buf);

			// Now get the report descriptor size
			int desc_size = 0;
			if ((rc = ioctl(tmpfd, HIDIOCGRDESCSIZE, &desc_size)) < 0)
				printf("  %*s: unknown\n", PAD, "Rpt Desc Size");
			else
				printf("  %*s: %d\n", PAD, "Rpt Desc Size", desc_size);

			// Now get the report descriptor
			struct hidraw_report_descriptor rpt_desc;
			rpt_desc.size = desc_size;
			if ((rc = ioctl(tmpfd, HIDIOCGRDESC, &rpt_desc)) < 0)
				printf("  %*s: unknown\n", PAD, "Rpt Desc");
			else
			{
				printf("  %*s:\n", PAD, "Rpt Descriptor");
				print_bytes(stdout, rpt_desc.value, rpt_desc.size, 4);
			}
		} // if (strstr(ent->d_name, "hidraw") != NULL)
	} // while ((ent = readdir(dir)) != NULL && keep_going)
	closedir(dir);
} // list_hids()

/**
 * Presents a menu to the user to change the current read timeout.
 */
void mnu_set_timeout(void)
{
	int rc;
	int current_timeout_ms = canctl_get_timeout_ms();
	int userinput;
	while (1)
	{
		printf(
			"\nSET TIMEOUT:\n"
			"- Current timeout (ms): %d\n"
			"> ", current_timeout_ms);
		if ((rc = scanf("%d.*[^\n]", &userinput)) == EOF || rc == 0)
		{ // An error occurred or no matches were found
			flush_stdin();
			printf("ERROR: Invalid value. Try again.\n");
			continue;
		}
		canctl_set_timeout_ms(userinput);
		// cfg.timeout_ms = userinput;
		printf("Success\n");
		break;
	}
} // mnu_set_timeout()

/**
 * Performs a USB self test.
 */
void mnu_usb_self_test(void)
{
	int rc;
	unsigned char buf_tx[CANBUS_MSG_SIZE];
	unsigned char buf_rx[CANBUS_MSG_SIZE];
	unsigned long num_bins = 256;
	unsigned long num_rand = (unsigned long) RAND_MAX + 1;
	unsigned long bin_size = num_rand / num_bins;
	unsigned long defect = num_rand % num_bins;

	printf("\n");

	// Fill up the buffer with random bytes
	memset(buf_tx, 0, sizeof(buf_tx));
	buf_tx[0] = CANBUS_OUT_USB_TEST;
	srand(time(NULL));
	for (size_t i = 1; i < sizeof(buf_tx); i++)
	{
		long x;
		do
		{
			x = random();
		} while (num_rand - defect <= (unsigned long)x);
		buf_tx[i] = (unsigned char)(x / bin_size);
	}
	printf("Writing %ld random bytes:\n", sizeof(buf_tx));
	print_bytes(stdout, buf_tx, sizeof(buf_tx), 2);

	// Write
	if ((rc = canctl_write(fd_can, buf_tx, sizeof(buf_tx))) < 0)
	{
		printf("ERROR: A problem occurred: %s\n", strerror(errno));
		return;
	}
	else if (rc != sizeof(buf_tx))
	{
		printf("ERROR: Wrote an unexected number of bytes: "
			"expected %ld, wrote %d\n", sizeof(buf_tx), rc);
		return;
	}

	// Read
	memset(buf_rx, 0, sizeof(buf_rx));
	if ((rc = canctl_read(fd_can, buf_rx, sizeof(buf_rx))) < 0)
	{
		printf("ERROR: A problem occurred: %s\n", strerror(errno));
		return;
	}
	else if (rc != sizeof(buf_rx))
	{
		printf("ERROR: Read an unexected number of bytes: "
			"expected %ld, read %d\n", sizeof(buf_rx), rc);
		return;
	}
	else
	{
		printf("Received %d bytes:\n", rc);
		print_bytes(stdout, buf_rx, rc, 2);
		printf("Checking validity\n");
		for (int i = 0; i < rc; i++)
		{
			if (buf_tx[i] != buf_rx[i])
			{
				printf("ERROR: Index [%d]: expected %02x, received %02x\n",
					i, buf_tx[i], buf_rx[i]);
				return;
			}
		} // for (size_t i = 0; i < rc; i++)
		printf("Success. Bytes are valid.\n");
	}
} // mnu_usb_self_test()

/**
 * @todo Document
 */
void mnu_can_self_test(void)
{
	printf("\n");
	printf("FEATURE NOT YET COMPLETE!\n");
} // mnu_can_self_test()

/**
 * Puts the CANbus module into internal loopback mode. Pegatron's document
 * says to do the following:
 * Step 1: Set CAN configuration to configuration mode
 * Step 2: Validate configuration
 * Step 3: Set CAN configuration to loopback mode
 * Step 4: Validate configuration
 * Step 5: Send a CAN data frame
 * Step 6: Receive CAN data frame (loopback)
 * Step 7: Set CAN configuration to configuration mode
 * Step 8: Validate configuration
 * Step 9: Set CAN configuration to normal mode
 * Step 10: Validate configuration
 */
void mnu_can_loopback_test()
{
	unsigned char buf_tx[CANBUS_MSG_SIZE];
	unsigned char buf_rx[CANBUS_MSG_SIZE];
	int rc, txlen;

	printf("\n");

	// Step 1 and 2
	if (canctl_set_config(fd_can, CANBUS_CFG_CONFIGURATION, CANBUS_MAX_BPS) < 0)
	{
		printf("ERROR: A problem occurred setting the configuration to %s\n",
			canctl_config_to_string(CANBUS_CFG_CONFIGURATION));
		return;
	}

	// Step 3 and 4
	if (canctl_set_config(fd_can, CANBUS_CFG_LOOPBACK, CANBUS_MAX_BPS) < 0)
	{
		printf("ERROR: A problem occurred setting the configuration to %s\n",
			canctl_config_to_string(CANBUS_CFG_LOOPBACK));
		return;
	}

	memset(buf_tx, 0, sizeof(buf_tx));
	buf_tx[0] = CANBUS_OUT_SEND_DATA;
	buf_tx[1] = 0x01; // Send one frame
	buf_tx[2] = 0x1d; // Start of frame, 29-bit ID size
	buf_tx[3] = 0x1f; // ID[31:24]
	buf_tx[4] = 0xff; // ID[23:16]
	buf_tx[5] = 0xff; // ID[15:8]
	buf_tx[6] = 0xff; // ID[7:0]
	buf_tx[7] = 0x08; // Data payload size, 8 bytes
	buf_tx[8] = 0x08; // Data
	buf_tx[9] = 0x07; // Data
	buf_tx[10] = 0x06; // Data
	buf_tx[11] = 0x05; // Data
	buf_tx[12] = 0x04; // Data
	buf_tx[13] = 0x03; // Data
	buf_tx[14] = 0x02; // Data
	buf_tx[15] = 0x01; // Data
	txlen = 16;
	printf("Writing %d bytes:\n", txlen);
	print_bytes(stdout, buf_tx, txlen, 2);

	// Step 5 - Write
	if ((rc = canctl_write(fd_can, buf_tx, txlen)) < 0)
	{
		printf("ERROR: A problem occurred: %s\n", strerror(errno));
		return;
	}
	else if (rc != txlen)
	{
		printf("ERROR: Wrote an unexected number of bytes: "
			"expected %d, wrote %d\n", txlen, rc);
		return;
	}

	// Step 6 - Read
	memset(buf_rx, 0, sizeof(buf_rx));
	if ((rc = canctl_read(fd_can, buf_rx, sizeof(buf_rx))) < 0)
	{
		printf("ERROR: A problem occurred: %s\n", strerror(errno));
		return;
	}
	else if (rc < txlen)
	{
		printf("ERROR: Read an unexpected number of bytes: "
			"expected %d, read %d\n", txlen, rc);
		return;
	}

	// Loop over the returned bytes and make sure they're the same as what
	// was transmitted. Only check up to txlen bytes, not rc, because the
	// CAN firmware seems to always return 64 bytes no matter what.
	printf("Received %d bytes:\n", txlen < rc ? txlen : rc);
	print_bytes(stdout, buf_rx, txlen < rc ? txlen : rc, 2);
	printf("Checking validity\n");
	for (int i = 0; i < txlen; i++)
	{
		if (buf_tx[i] != buf_rx[i])
		{
			printf("ERROR: Index [%d]: expected %02x, received %02x\n",
				i, buf_tx[i], buf_rx[i]);
			return;
		}
	} // for (size_t i = 0; i < rc; i++)
	printf("Success. Bytes are valid.\n");

	// Step 7 and 8
	if (canctl_set_config(fd_can, CANBUS_CFG_CONFIGURATION, CANBUS_MAX_BPS) < 0)
	{
		printf("ERROR: A problem occurred setting the configuration to %s\n",
			canctl_config_to_string(CANBUS_CFG_CONFIGURATION));
		return;
	}

	// Step 9 and 10
	if (canctl_set_config(fd_can, CANBUS_CFG_NORMAL, CANBUS_MAX_BPS) < 0)
	{
		printf("ERROR: A problem occurred setting the configuration to %s\n",
			canctl_config_to_string(CANBUS_CFG_NORMAL));
		return;
	}

	return;
} // mnu_can_loopback_test()

/**
 * Presents a menu to the user for setting the I/O direction (type) or data output value of each GPIO pin
 */
void mnu_gpio_set_pin(int type_or_data)
{
	int rc;
	int keep_going = 1;
	int current_pin = 1; //Index for 
        unsigned char buf[GPIO_PIN_COUNT]; //buf for 

	(type_or_data == PIN_TYPE) ? printf("\nEnter Pin Direction Settings for GPIO 1-8\n") : printf("\nEnter Pin Data Settings for GPIO 1-8\n");


	// Put GPIO command and subcommand into the buffer
        //buf[0] = (type_or_data == PIN_TYPE) ? GPIO_OUT_SET_PIN_TYPE  : GPIO_OUT_SET_PIN_DATA; 
        //buf[1] = (type_or_data == PIN_TYPE) ? GPIO_OUT_READ_PIN_TYPE : GPIO_OUT_READ_PIN_DATA; 

	while (keep_going)
	{
	(type_or_data == PIN_TYPE) ? printf("Set Pin [%d] (0 = Output, 1 = Input, 2 = Start Over, 5 = Exit): ", current_pin) : printf("Set Pin [%d] (0 = Set to 0/LOW, 1 = Set to 1/HIGH, 2 = Start Over, 5 = Exit): ", current_pin);
              
		int userinput;
		if ((rc = scanf("%d.*[^\n]", &userinput)) == EOF || rc == 0)
		{ // An error or no matches were found, so flush stdin
			flush_stdin();
			userinput = -1;
		}

		// Collect user input until all pins are set
		switch (userinput)
		{
			case 0: // OUTPUT/LOW
	                        (type_or_data == PIN_TYPE) ? printf("Pin %d will be set to OUTPUT\n", current_pin) : printf("Pin %d will be set to 0/LOW \n", current_pin);
				buf[current_pin-1] = 0x00;
				keep_going = (current_pin < GPIO_PIN_COUNT) ? 1 : 0;
			        current_pin++;	
				break;
			
			case 1: // INPUT/HIGH 
	                        (type_or_data == PIN_TYPE) ? printf("Pin %d will be set to INPUT\n", current_pin) : printf("Pin %d will be set to 1/HIGH \n", current_pin);
				buf[current_pin-1] = 0x01;
				keep_going = (current_pin < GPIO_PIN_COUNT) ? 1 : 0;
			        current_pin++;	
				break;
			
			case 2: // Start Over
	                        printf("START OVER\n");
				keep_going = 1;
			        current_pin = 1;	
				break;

			case 5: // Exit the function
				return;

			default: // Unknown error occurred
				printf("ERROR: Invalid input. Please try again.\n");
				break;

		}

	} // end while (keep_going)

	// 
	if (gpio_set_pin(fd_gpio, type_or_data, buf) < 0)
	{
		(type_or_data == PIN_TYPE) ? printf("ERROR: A problem occurred setting the GPIO pin types.\n") : printf("ERROR: A problem occurred setting the GPIO pin Data.\n");
		return;
	}

} // end mnu_set_gpio_pin()

void list_gpio_pin(int type_or_data) {

	unsigned char buf[GPIO_PIN_COUNT];
	memset(buf, 0, sizeof(buf));
	if(gpio_read_pin(fd_gpio, type_or_data, buf) < 0) {
		printf("ERROR: A problem occurred retrieving GPIO pin status.\n");
		return;
        
	} else {
		for (int i = 0; i < GPIO_PIN_COUNT; i++){
                	switch (buf[i]) 
			{
				case 0x00:
        			(type_or_data == PIN_TYPE) ? printf("Pin %d is set as an OUTPUT\n", i+1) : printf("Pin %d voltage is at 0/LOW\n", i+1);
				break;

				case 0x01:
        			(type_or_data == PIN_TYPE) ? printf("Pin %d is set as an INPUT\n", i+1) : printf("Pin %d voltage is at 1/HIGH\n", i+1);
				break;

				default:
				printf("Pin %d -- UNRECOGNIZED VALUE\n", i+1);
				break;
			}
		}
	}	


} //end list_gpio_pin_types()



void mnu_gpio_get_iom_or_sku(int op_select) {
unsigned char buf[CANBUS_MSG_SIZE];

memset(buf, 0, sizeof(buf));
	if(gpio_get_iom_or_sku(fd_gpio, op_select, buf) < 0) {
		printf("ERROR: A problem occurred getting board ID or IOM SKU.\n");
		return;
	} else {
		(op_select == GET_IOM) ? printf("IOM SKU: ") : printf("Board ID: ");
		printf("%02x\n", buf[0]);
	}

}//end mnu_gpio_get_iom_or_sku()
