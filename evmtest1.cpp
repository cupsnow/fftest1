/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

#include <time.h>
#include <getopt.h>

#define CTRL_PATH NULL
#define CTRL_PORT CTRL_PORT_NONE

static struct {
	unsigned quit: 1;
	int log_level;

} impl = {0};

extern "C" {

void *ev_ctx = NULL, *cfg_ctx = NULL;
const char *ctrl_path = CTRL_PATH;
int ctrl_port = CTRL_PORT;

}

static int _log_lvl(const char *lvl) {
	int lvl_n = (int)(unsigned long)lvl;

	if (lvl_n == log_level_err
			|| lvl_n == log_level_info
			|| lvl_n == log_level_debug
			|| lvl_n == log_level_verb) {
		return lvl_n;
	}
	if (strncasecmp(lvl, "err", strlen("err")) == 0) return log_level_err;
	if (strncasecmp(lvl, "inf", strlen("inf")) == 0) return log_level_info;
	if (strncasecmp(lvl, "deb", strlen("deb")) == 0) return log_level_debug;
	if (strncasecmp(lvl, "ver", strlen("ver")) == 0) return log_level_verb;
	return 0;
}

static const char *_log_lvl_str(const char *lvl) {
	int lvl_n = (int)(unsigned long)lvl;

	if (lvl_n == log_level_err) return "ERROR";
	else if (lvl_n == log_level_info) return "INFO";
	else if (lvl_n == log_level_debug) return "Debug";
	else if (lvl_n == log_level_verb) return "verbose";
	else if (lvl && lvl[0]) return lvl;
	return "";
}

__attribute__((format(printf, 4, 0)))
static int _log_v(const char *lvl, const char *func_name, int lno,
		const char *fmt, va_list va) {
	char buf[500];
	aloe_buf_t fb = {.data = buf, .cap = sizeof(buf)};
	int r, lvl_n;
	struct timespec ts;
	struct tm tm;
	FILE *fp;

	if ((lvl_n = _log_lvl(lvl)) > impl.log_level) return 0;
	fp = ((lvl_n >= log_level_info) ? stderr : stdout);

	aloe_buf_clear(&fb);
	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);

	if ((r = aloe_buf_printf(&fb, "[%02d:%02d:%02d:%06d]", tm.tm_hour,
	        tm.tm_min, tm.tm_sec, (int)(ts.tv_nsec / 1000))) <= 0) {
		goto finally;
	}
	if ((r = aloe_buf_printf(&fb, "[%s][%s][#%d]", _log_lvl_str(lvl), func_name,
	        lno)) <= 0) {
		goto finally;
	}
	if ((r = aloe_buf_vprintf(&fb, fmt, va)) < 0) {
		goto finally;
	}
finally:
	aloe_buf_flip(&fb);
	if (fb.lmt > 0) {
		fwrite(fb.data, fb.lmt, 1, fp);
		fflush(stdout);
	}
	return fb.lmt;
}

extern "C"
int _log_m(const char *lvl, const char *func_name, int lno,
        const char *fmt, ...) {
	int r;
	va_list va;
	va_start(va, fmt);
	r = _log_v(lvl, func_name, lno, fmt, va);
	va_end(va);
	return r;
}

static const char opt_short[] = "ht:v";
enum {
	opt_key_reflags = 0x201,
	opt_key_max
};
static struct option opt_long[] = {
	{"help", no_argument, NULL, 'h'},
	{"ctrlpath", required_argument, NULL, 't'},
	{"ctrlport", required_argument, NULL, 'p'},
	{"verbose", no_argument, NULL, 'v'},
	{"reflags", required_argument, NULL, opt_key_reflags},
	{0},
};

static void help(int argc, char **argv) {
	int i;

#if 1
	for (i = 0; i < argc; i++) {
		log_d("argv[%d/%d]: %s\n", i + 1, argc, argv[i]);
	}
#endif

	fprintf(stdout,
"COMMAND\n"
"    %s [OPTIONS]\n"
"\n"
"OPTIONS\n"
"    -h, --help         Show help\n"
"    -t, --ctrlpath=<CTRL>\n"
"                       Unix socket for control interface(\"%s\")\n"
"    -p, --ctrlport=<PORT>\n"
"                       Bind port for control interface"
#if (CTRL_PORT == CTRL_PORT_ANY)
			"(ANY)"
#elif (CTRL_PORT != CTRL_PORT_NONE)
			"(%d)"
#endif
"\n"
"    -v, --verbose      Verbose output (default mimic debug and more)\n"
"\n",
		((argc > 0) && argv && argv[0] ? argv[0] : "Program"),
		(CTRL_PATH ? CTRL_PATH : "")
#if (CTRL_PORT != CTRL_PORT_ANY) && (CTRL_PORT != CTRL_PORT_NONE)
		, CTRL_PORT
#endif
		);
}

int main(int argc, char **argv) {
	int opt_op, opt_idx, r, opt_exit = 0;
	aloe_buf_t buf = {.data = NULL};
	typedef struct mod_rec {
		const aloe_mod_t *op;
		TAILQ_ENTRY(mod_rec) qent;
		void *ctx;
	} mod_t;
	TAILQ_HEAD(mod_queue_rec, mod_rec) mod_q = TAILQ_HEAD_INITIALIZER(mod_q);
	mod_t *mod;

	impl.log_level = log_level_info;
	optind = 0;
	while ((opt_op = getopt_long(argc, argv, opt_short, opt_long,
			&opt_idx)) != -1) {
		if (opt_op == 'h') {
			opt_exit |= 1;
			continue;
		}
		if (opt_op == 't') {
			ctrl_path = optarg;
			continue;
		}
		if (opt_op == 'p') {
			ctrl_port = strtol(optarg, NULL, 10);
			continue;
		}
		if (opt_op == 'v') {
			if (impl.log_level < log_level_verb) impl.log_level++;
			continue;
		}
	}

	if (!ctrl_path || !ctrl_path[0]) ctrl_path = CTRL_PATH;
	if (ctrl_port <= 0 || ctrl_port >= 65535) ctrl_port = CTRL_PORT;

	if (opt_exit) {
		if (opt_exit & 2 && impl.log_level >= log_level_debug) opt_exit |= 1;
		if (opt_exit & 1) help(argc, argv);
		r = 1;
		goto finally;
	}

	if (!(cfg_ctx = aloe_cfg_init())) {
		r = ENOMEM;
		log_e("aloe_cfg_init\n");
		goto finally;
	}

	if (!(ev_ctx = aloe_ev_init())) {
		r = ENOMEM;
		log_e("aloe_ev_init\n");
		goto finally;
	}

#define MOD_INIT(_op) { \
		extern const aloe_mod_t _op; \
		static mod_t mod = {&_op}; \
		if (!(mod.ctx = (*mod.op->init)())) { \
			r = EIO; \
			log_e("init mod: %s\n", mod.op->name); \
			goto finally; \
		} \
		TAILQ_INSERT_TAIL(&mod_q, &mod, qent); \
	}
	MOD_INIT(mod_cli);

#if 0
	MOD_INIT(mod_test1_buf1);
	impl.quit = 1;
#endif
    while (!impl.quit) {
    	aloe_ev_once(ev_ctx);
//    	log_d("enter\n");
    }
	r = 0;
finally:
	while ((mod = TAILQ_LAST(&mod_q, mod_queue_rec))) {
		TAILQ_REMOVE(&mod_q, mod, qent);
		mod->op->destroy(mod->ctx);
	}
	if (ev_ctx) {
		aloe_ev_destroy(ev_ctx);
	}
	if (cfg_ctx) {
		aloe_cfg_destroy(cfg_ctx);
	}
	if (buf.data) free(buf.data);
	return r;
}
