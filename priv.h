/**
 * @author joelai
 */

#ifndef _H_ALOE_EV_PRIV
#define _H_ALOE_EV_PRIV

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aloe/main.h>
#include <aloe/ev.h>

/** @defgroup ALOE_PRIV Internal
 * @brief Internal utility.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define log_level_err 1
#define log_level_info 2
#define log_level_debug 3
#define log_level_verb 4

//#define log_m(_lvl, _fmt, _args...) printf(_lvl "%s #%d " _fmt, __func__, __LINE__, ##_args)
#define log_m(_lvl, _fmt, _args...) _log_m((char*)_lvl, __func__, __LINE__, _fmt, ##_args)
#define log_e(_args...) log_m(log_level_err, ##_args)
#define log_i(_args...) log_m(log_level_info, ##_args)
#define log_d(_args...) log_m("Debug ", ##_args)
#define log_v(_args...) log_m("verbose ", ##_args)

__attribute__((format(printf, 4, 5)))
int _log_m(const char *lvl, const char *func_name, int lno,
		const char *fmt, ...);


extern void *cfg_ctx;

#define cfg_key_prefix ""
#define cfg_key_ctrl_path cfg_key_prefix "cfg_key_ctrl_path"
#define cfg_key_ctrl_port cfg_key_prefix "cfg_key_ctrl_port"

extern const char *ctrl_path;
extern int ctrl_port;
#define CTRL_PORT_ANY 0
#define CTRL_PORT_NONE -1

extern void *ev_ctx;

/** Notify event to user. */
typedef struct aloe_ev_ctx_noti_rec {
	int fd;
	aloe_ev_noti_cb_t cb; /**< Callback to user. */
	void *cbarg; /**< Callback argument. */
	unsigned ev_wait; /**< Event to wait. */
	struct timespec due; /**< Monotonic timeout. */
	unsigned ev_noti; /**< Notified event. */
	TAILQ_ENTRY(aloe_ev_ctx_noti_rec) qent;
} aloe_ev_ctx_noti_t;

/** Queue of aloe_ev_noti_t. */
typedef TAILQ_HEAD(aloe_ev_ctx_noti_queue_rec, aloe_ev_ctx_noti_rec) aloe_ev_ctx_noti_queue_t;

/** FD for select(). */
typedef struct aloe_ev_ctx_fd_rec {
	int fd; /**< FD to monitor. */
	aloe_ev_ctx_noti_queue_t noti_q; /**< Queue of aloe_ev_noti_t for this FD. */
	TAILQ_ENTRY(aloe_ev_ctx_fd_rec) qent;
} aloe_ev_ctx_fd_t;

/** Queue of aloe_ev_fd_t. */
typedef TAILQ_HEAD(aloe_ev_ctx_fd_queue_rec, aloe_ev_ctx_fd_rec) aloe_ev_ctx_fd_queue_t;

/** Information about control flow and running context. */
typedef struct aloe_ev_ctx_rec {
	aloe_ev_ctx_fd_queue_t fd_q; /**< Queue to select(). */
	aloe_ev_ctx_fd_queue_t spare_fd_q; /**< Queue for cached memory. */
	aloe_ev_ctx_noti_queue_t noti_q; /**< Queue for ready to notify. */
	aloe_ev_ctx_noti_queue_t spare_noti_q; /**< Queue for cached memory. */
} aloe_ev_ctx_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _H_ALOE_EV_PRIV */
