# Bugs and Known Issues

( ) 010 - CANbus loopback test makes no note of the current configuration before starting the loopback test. It ends by setting the module in Normal operation mode, but this may not have been the original mode.

## Completed

(x) 001 - When specifying the file path manually (using -p flag), the program exits with error code -1 for no reason after opening the device handle.

(x) 002 - When allowed to serach dynamically for a device, and the device found is NOT hidraw0 (i.e. it is hidraw2 or hidrawNNN), then the program will not connect to it if you select "y" at the input prompt.

(x) 003 - During CAN write mode, messages are *always* flagged as invalid and thus are never written.

(x) 004 - CAN Read mode does not exit gracefully on Ctrl+c, but instead throws an ERROR due to Interrupted system call.

(x) 005 - Bug in CAN read mode where the buffer isn't cleared between successive reads

(x) 006 - Does not work yet: CAN USB self test

(x) 008 - Does not work yet: Main Menu > Get CAN error status

(x) 009 - Setting CAN read timeout through Main Menu has no effect.

(x) 007 - Does not work yet: CANbus self test

(x) 011 - Does not work yet: CANbus loopback test
