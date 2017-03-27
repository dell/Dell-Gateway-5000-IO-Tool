/**
 * @file cfg.h
 * @date 2017-03-24
 */

#ifndef CFG_H_
#define CFG_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "canctl.h"

typedef struct cfg
{
	int list_hids;
	char path[256];
	long int timeout_ms;
	int verbose;
} cfg_t;

#ifdef __cplusplus
}
#endif

#endif // CFG_H_
