/**
 * @file args.h
 * @date 2017-03-24
 */

#ifndef ARGS_H_
#define ARGS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "cfg.h"
#include "version.h"
#include <argp.h>
#include <stdlib.h>
#include <string.h>

const char *argp_program_version = PROGRAM_VERSION;
const char *argp_program_bug_address = BUG_ADDRESS;

/**
 * PROGRAM DOCUMENTATION - field 4 in struct argp
 */
static const char prog_doc[] = "\n"
	"CANbus Controller (for Dell IoT Gateways)";

/**
 * NON-OPTION (REQUIRED) ARGUMENTS - field 3 in struct argp
 */
static const char args_doc[] = "";

/**
 * ARGUMENT OPTIONS - field 1 in struct argp
 * Order: { name, key, arg, flags, doc, group }
 * "struct argp_option" is defined in argp.h
 */
static const struct argp_option options[] = {
	{ "listhids", 'l', 0, 0, "List all HIDs on system and exit", 0 },
	{ "path", 'p', "PATH", 0, "CANbus module's path, e.g. /dev/hidraw0. "
		"Specifying this flag forces the program to use this device file "
		"path instead of searching dynamically. Default=(null)", 0 },
	{ "timeout", 't', "MSEC", 0, "Milliseconds to wait during read. "
		"Default=10000", 0 },
	{ "verbose", 'v', 0, 0, "Print more messages", 0 },
	{ 0, 0, 0, 0, 0, 0 }
};

/**
 * ARGUMENT PARSER - field 2 in struct argp
 * Order of parameters: key, arg, state
 * This is where the actual arguments are parsed
 * and processed and assigned to variables.
 * @param key @todo Document
 * @param arg @todo Document
 * @param state @todo Document
 * @returns @todo Document
 */
static error_t parser(int key, char *arg, struct argp_state *state)
{
	cfg_t *cfg = (cfg_t *)state->input;
	switch (key)
	{
		case 'l': // --listhids
			cfg->list_hids = 1;
			break;
		case 'p': // --path
			memset(cfg->path, 0, sizeof(cfg->path)); // Clear buffer
			memcpy(cfg->path, arg, sizeof(cfg->path)-1); // Leave last byte 0
			break;
		case 't': // --timeout
		{
			char *endptr;
			cfg->timeout_ms = strtol(arg, &endptr, 10);
			if (cfg->timeout_ms == LONG_MIN || cfg->timeout_ms == LONG_MAX)
			{
				printf("WARNING: Underflow on overflow occurred for --timeout "
					"argument. Defaulting to %d ms\n",
					CANBUS_DEFAULT_TIMEOUT_MS);
				cfg->timeout_ms = CANBUS_DEFAULT_TIMEOUT_MS;
			}
			if (arg == endptr)
				printf("WARNING: Invalid entry for --timeout argument. "
					"Defaulting to %d ms\n", CANBUS_DEFAULT_TIMEOUT_MS);
				cfg->timeout_ms = CANBUS_DEFAULT_TIMEOUT_MS;
			break;
		}
		case ARGP_KEY_ARG:
		case ARGP_KEY_END:
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/**
 * ARGP structure itself. This is a combination of all the things that were
 * initialized above. It is used in argp_parse(), which is called at the
 * beginning of main().
 */
static const struct argp argp = {
	options, // const struct argp_option *options
	parser, // argp_parser_t parser
	args_doc, // const char *args_doc
	prog_doc, // const char *prog_doc
	NULL, // argp_children *
	NULL, // char *(*help_filter)(int, char **, char **)
	NULL // char *argp_domain
};

#ifdef __cplusplus
}
#endif

#endif // ARGS_H_
