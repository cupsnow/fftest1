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
static char mod_name[] = "cli";

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
	conn_t ctrl_listener[3];
	conn_queue_t ctrl_conn;
} ctx_t;

static void ctrl_on_input(conn_t *listener, const aloe_buf_t *buf) {
	log_d("Control command append: %s\n", (char*)buf.data);
}

static void ctrl_on_read(int fd, unsigned ev_noti, void *cbarg) {
	conn_t *listener = (conn_t*)cbarg;
	ctx_t *ctx = (ctx_t*)listener->ctx;
	aloe_buf_t buf = {.data = NULL};
	int r;

	log_d("instanceId: %d, ev_noti: %d\n", ctx->instanceId, ev_noti);

	if ((aloe_buf_expand(&buf, 500, aloe_buf_flag_none)) != 0) {
		log_e("Failed alloc memory to get control command\n");
		goto finally;
	}
	aloe_buf_clear(&buf);
	r = read(fd, buf.data, buf.lmt - buf.pos - 1);
	if (r < 0) {
		r = errno;
		if (ALOE_ENO_NONBLOCKING(r) || r == EINTR) {
			r = 0;
			goto finally;
		}
		log_e("Failed read control command: %s\n", strerror(r));
		goto finally;
	}
	if (r == 0) {
		log_d("Remote closed control command\n");
		goto finally;
	}
	((char*)buf.data)[buf.pos += r] = '\0';
	aloe_buf_flip(&buf);
	log_d("Control command append: %s\n", (char*)buf.data);
	r = 0;
finally:
	if (!(listener->ev = aloe_ev_put(ev_ctx, listener->fd, &ctrl_on_read,
			listener, aloe_ev_flag_read, ALOE_EV_INFINITE, 0))) {
		log_e("Failed schedule read unix socket\n");
	}
	if (buf.data) free(buf.data);
}

static void ctrl_on_accept(int fd, unsigned ev_noti, void *cbarg) {
	conn_t *listener = (conn_t*)cbarg;
	ctx_t *ctx = (ctx_t*)listener->ctx;
//	conn_t *conn = NULL;
	int r;

	log_d("instanceId: %d, ev_noti: %d\n", ctx->instanceId, ev_noti);
//	if ((conn = malloc(sizeof(*conn))) == NULL) {
//		r = ENOMEM;
//		log_e("Alloc memory for remote connection\n");
//		goto finally;
//	}
	r = 0;
//finally:
	if (r != 0) {

	}
}

static void* init(void) {
	ctx_t *ctx = NULL;
	int r, i;

	if ((ctx = malloc(sizeof(*ctx))) == NULL) {
		r = -1;
		log_e("alloc instance\n");
		goto finally;
	}
	TAILQ_INIT(&ctx->ctrl_conn);
	for (i = 0; i < aloe_arraysize(ctx->ctrl_listener); i++) {
		conn_t *conn = &ctx->ctrl_listener[i];

		conn->fd = -1;
		conn->ev = NULL;
		conn->ctx = ctx;
	}
	ctx->instanceId = instanceId++;

	if (ctrl_path && ctrl_path[0]) {
		conn_t *listener = &ctx->ctrl_listener[0];

		mkfifo(ctrl_path, 0666);
		if ((listener->fd = open(ctrl_path, O_RDWR, 0666)) == -1) {
			r = errno;
			log_e("Failed open unix socket path %s, %s\n",
					ctrl_path, strerror(r));
			goto finally;
		}
		if ((r = aloe_file_nonblock(listener->fd, 1)) != 0) {
			log_e("Failed set unix socket nonblock\n");
			goto finally;
		}
		if (!(listener->ev = aloe_ev_put(ev_ctx, listener->fd, &ctrl_on_read,
				listener, aloe_ev_flag_read, ALOE_EV_INFINITE, 0))) {
			r = -1;
			log_e("Failed schedule read unix socket\n");
			goto finally;
		}

		log_i("%s[%d], Control interface: %s\n", mod_name, ctx->instanceId,
		        ctrl_path);
	}

	if (ctrl_port != CTRL_PORT_NONE) {
		conn_t *listener = &ctx->ctrl_listener[1];
		int _ctrl_port = ctrl_port == CTRL_PORT_ANY ? 0 : ctrl_port;
		struct sockaddr_in sa;
		socklen_t sa_len;

		if ((listener->fd = aloe_ip_listener((struct sockaddr*)
				&(struct sockaddr_in){.sin_port = htons(_ctrl_port),
				.sin_family = AF_INET, .sin_addr = {INADDR_ANY}}, 3)) == -1) {
			r = -1;
			log_e("Failed listen ip port %d\n", _ctrl_port);
			goto finally;
		}
		sa_len = sizeof(sa);
		if ((r = getsockname(listener->fd, (struct sockaddr*)&sa, &sa_len)) != 0) {
			r = errno;
			log_e("Failed checkout listen ip port\n");
			goto finally;
		}

		if ((r = aloe_file_nonblock(listener->fd, 1)) != 0) {
			log_e("Failed set ip port nonblock\n");
			goto finally;
		}
		if (!(listener->ev = aloe_ev_put(ev_ctx, listener->fd, &ctrl_on_accept,
				listener, aloe_ev_flag_read, ALOE_EV_INFINITE, 0))) {
			r = -1;
			log_e("Failed schedule read ip port\n");
			goto finally;
		}

		log_i("%s[%d], Control port: %d\n", mod_name, ctx->instanceId,
		        ntohs(sa.sin_port));
	}

	r = 0;
finally:
	if (r != 0) {
		if (ctx) {
			for (i = 0; i < aloe_arraysize(ctx->ctrl_listener); i++) {
				conn_t *conn = &ctx->ctrl_listener[i];
				if (conn->fd != -1) close(conn->fd);
				if (!conn->ev) aloe_ev_cancel(ev_ctx, conn->ev);
			}
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

const aloe_mod_t mod_cli = {.name = mod_name, .init = &init, .destroy = &destroy,
        .ioctl = &ioctl};
