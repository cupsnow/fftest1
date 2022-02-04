/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

struct timeval* aloe_timeval_norm(struct timeval *a) {
	if (a) ALOE_TIMESEC_NORM(a->tv_sec, a->tv_usec, 1000000ul);
	return a;
}

int aloe_timeval_cmp(const struct timeval *a, const struct timeval *b) {
	return ALOE_TIMESEC_CMP(a->tv_sec, a->tv_usec, b->tv_sec, b->tv_usec);
}

struct timeval* aloe_timeval_sub(const struct timeval *a, const struct timeval *b,
		struct timeval *c) {
	ALOE_TIMESEC_SUB(a->tv_sec, a->tv_usec, b->tv_sec, b->tv_usec,
			c->tv_sec, c->tv_usec, 1000000ul);
	return c;
}

struct timeval* aloe_timeval_add(const struct timeval *a, const struct timeval *b,
		struct timeval *c) {
	ALOE_TIMESEC_ADD(a->tv_sec, a->tv_usec, b->tv_sec, b->tv_usec,
			c->tv_sec, c->tv_usec, 1000000ul);
	return c;
}

