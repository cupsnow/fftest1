/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include "unistd.h"
#include <fcntl.h>
#include <netinet/ip.h>

static int instanceId = 0;
static char mod_name[] = "micloop";

typedef struct conn_rec {
	int fd;
	void *ev, *ctx;
	TAILQ_ENTRY(conn_rec) qent;
} conn_t;
typedef TAILQ_HEAD(conn_queue_rec, conn_rec) conn_queue_t;

typedef struct listener_rec {
	conn_t conn;
	aloe_buf_t name;
} listener_t;

typedef struct {
	int instanceId;
} ctx_t;


static void* init(void) {
	ctx_t *ctx = NULL;
	int r, i;

	if ((ctx = malloc(sizeof(*ctx))) == NULL) {
		r = -1;
		log_e("alloc instance\n");
		goto finally;
	}
	ctx->instanceId = instanceId++;



	r = 0;
finally:
	if (r != 0) {
		if (ctx) {
			free(ctx);
		}
		return NULL;
	}
	return (void*)ctx;
}

static void destroy(void *_ctx) {
	ctx_t *ctx = (ctx_t*)_ctx;

	log_d("%s[%d]\n", mod_name, ctx->instanceId);
	free(ctx);
}

static int ioctl(void *_ctx, void *args) {
	ctx_t *ctx = (ctx_t*)_ctx;

	log_d("%s[%d]\n", mod_name, ctx->instanceId);
	return 0;
}

const aloe_mod_t mod_micloop = {.name = mod_name, .init = &init, .destroy = &destroy,
        .ioctl = &ioctl};
