/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

typedef struct aloe_cfg_rec {
	aloe_cfg_type_t type;
	aloe_buf_t fb_key;
	union {
		aloe_buf_t fb;
	} val;
	TAILQ_ENTRY(aloe_cfg_rec) qent;
	RB_ENTRY(aloe_cfg_rec) rbent;
} aloe_cfg_t;

typedef TAILQ_HEAD(aloe_cfg_queue_rec, aloe_cfg_rec) aloe_cfg_queue_t;

typedef RB_HEAD(aloe_cfg_rbtree_rec, aloe_cfg_rec) aloe_cfg_rbtree_t;

RB_PROTOTYPE(aloe_cfg_rbtree_rec, aloe_cfg_rec, rbent, );

typedef struct aloe_cfg_ctx_rec {
	aloe_cfg_rbtree_t cfg_rb;
	aloe_cfg_queue_t spare_q;
} aloe_cfg_ctx_t;

static int aloe_cfg_cmp(aloe_cfg_t *a, aloe_cfg_t *b) {
	return strcmp(a->fb_key.data, b->fb_key.data);
}

RB_GENERATE(aloe_cfg_rbtree_rec, aloe_cfg_rec, rbent, aloe_cfg_cmp);

static aloe_cfg_t* cfg_rb_find(aloe_cfg_rbtree_t *rbtree, const char *_key) {
	if (!_key) return RB_MIN(aloe_cfg_rbtree_rec, rbtree);
	return RB_FIND(aloe_cfg_rbtree_rec, rbtree,
			&(aloe_cfg_t){.fb_key = {.data = (void*)_key}});
}

static void cfg_reset_val(aloe_cfg_t *cfg) {
	if (cfg->type == aloe_cfg_type_data ||
			cfg->type == aloe_cfg_type_string) {
		aloe_buf_t *fb = (aloe_buf_t*)&cfg->val;
		if (fb->data) free(fb->data);
	}
	cfg->type = aloe_cfg_type_void;
}

static void cfg_free(aloe_cfg_t *cfg) {
	cfg_reset_val(cfg);
	if (cfg->fb_key.data) free(cfg->fb_key.data);
	free(cfg);
}

static int cfg_set_val(aloe_cfg_t *cfg, aloe_cfg_type_t type, va_list va) {

	switch (type) {
	default:
		return EINVAL;
	case aloe_cfg_type_void:
		cfg_reset_val(cfg);
		break;
	case aloe_cfg_type_int:
		*(int*)&cfg->val = va_arg(va, int);
		break;
	case aloe_cfg_type_uint:
		*(unsigned*)&cfg->val = va_arg(va, unsigned);
		break;
	case aloe_cfg_type_long:
		*(long*)&cfg->val = va_arg(va, long);
		break;
	case aloe_cfg_type_ulong:
		*(unsigned long*)&cfg->val = va_arg(va, unsigned long);
		break;
	case aloe_cfg_type_double:
		*(double*)&cfg->val = va_arg(va, double);
		break;
	case aloe_cfg_type_pointer: {
		*(const void**)&cfg->val = va_arg(va, const void*);
		break;
	}
	case aloe_cfg_type_data: {
		/* set( ,const void*, size_t) */
		aloe_buf_t *buf = (aloe_buf_t*)&cfg->val;
		const void *data = va_arg(va, const void*);
		size_t sz;

		if (!data) {
			buf->lmt = 0;
			break;
		}
		sz = va_arg(va, size_t);
		if (aloe_buf_expand(buf, sz + 1, 0) != 0) return ENOMEM;
		memcpy(buf->data, data, sz);
		((char*)buf->data)[buf->lmt = sz] = '\0';
		break;
	}
	case aloe_cfg_type_string: {
		/* set( ,const char*, ...) */
		aloe_buf_t *buf = (aloe_buf_t*)&cfg->val;
		const void *fmt = va_arg(va, const void*);

		aloe_buf_clear(buf);
		if (aloe_buf_aprintf(buf, -1, fmt, va) < 0) return ENOMEM;
		aloe_buf_flip(buf);
		break;
	}
	} /* switch (type) */
	cfg->type = type;
	return 0;
}

void* aloe_cfg_init(void) {
	aloe_cfg_ctx_t *ctx = NULL;
	if (!(ctx = malloc(sizeof(*ctx)))) {
		log_e("Failed malloc for cfg ctx\n");
		return NULL;
	}
	RB_INIT(&ctx->cfg_rb);
	TAILQ_INIT(&ctx->spare_q);
	return (void*)ctx;
}

void aloe_cfg_destroy(void *_ctx) {
	aloe_cfg_ctx_t *ctx = (aloe_cfg_ctx_t*)_ctx;
	aloe_cfg_t *cfg;

	while ((cfg = TAILQ_FIRST(&ctx->spare_q))) {
		TAILQ_REMOVE(&ctx->spare_q, cfg, qent);
		cfg_free(cfg);
	}

	while ((cfg = RB_MIN(aloe_cfg_rbtree_rec, &ctx->cfg_rb))) {
		RB_REMOVE(aloe_cfg_rbtree_rec, &ctx->cfg_rb, cfg);
		cfg_free(cfg);
	}
	free(ctx);
}

void* aloe_cfg_set(void *_ctx, const char *key, aloe_cfg_type_t type, ...) {
	aloe_cfg_ctx_t *ctx = (aloe_cfg_ctx_t*)_ctx;
	int r;
	aloe_cfg_t *cfg;
	struct {
		unsigned cfg_rb_inq: 1;
	} flag = {0};
	va_list va;

	if (!key || type == aloe_cfg_type_void) {
		int i = 0;

		while ((cfg = cfg_rb_find(&ctx->cfg_rb, key))) {
			RB_REMOVE(aloe_cfg_rbtree_rec, &ctx->cfg_rb, cfg);
			cfg_reset_val(cfg);
			TAILQ_INSERT_TAIL(&ctx->spare_q, cfg, qent);
			i++;
		}
		return NULL;
	}

	if ((cfg = cfg_rb_find(&ctx->cfg_rb, key))) {
		flag.cfg_rb_inq = 1;
	} else if ((cfg = TAILQ_FIRST(&ctx->spare_q))) {
		TAILQ_REMOVE(&ctx->spare_q, cfg, qent);
	} else if ((cfg = calloc(1, sizeof(*cfg)))) {
		;
	} else {
		log_e("Failed alloc cfg\n");
		return NULL;
	}

	aloe_buf_clear(&cfg->fb_key);
	if (!flag.cfg_rb_inq && aloe_buf_aprintf(&cfg->fb_key, 60, "%s", key) <= 0) {
		log_e("Failed set cfg key\n");
		cfg_free(cfg);
		return NULL;
	}

	va_start(va, type);
	r = cfg_set_val(cfg, type, va);
	va_end(va);
	if (r != 0 && !flag.cfg_rb_inq) {
		log_e("Failed set cfg val\n");
		cfg_free(cfg);
		return NULL;
	}

	if (!flag.cfg_rb_inq) {
		RB_INSERT(aloe_cfg_rbtree_rec, &ctx->cfg_rb, cfg);
	}
	return (void*)cfg;
}

void* aloe_cfg_find(void *_ctx, const char *key) {
	return (void*)cfg_rb_find(&((aloe_cfg_ctx_t*)_ctx)->cfg_rb, key);
}

void* aloe_cfg_next(void *_ctx, void *_prev) {
	if (!_prev) return (void*)cfg_rb_find(&((aloe_cfg_ctx_t*)_ctx)->cfg_rb, NULL);
	return RB_NEXT(aloe_cfg_rbtree_rec, &((aloe_cfg_ctx_t*)_ctx)->cfg_rb,
			(aloe_cfg_t*)_prev);
}

const char* aloe_cfg_key(void *_cfg) {
	return (const char*)((aloe_cfg_t*)_cfg)->fb_key.data;
}

aloe_cfg_type_t aloe_cfg_type(void *_cfg) {
	return ((aloe_cfg_t*)_cfg)->type;
}

int aloe_cfg_int(void *_cfg) {
	return *(int*)&((aloe_cfg_t*)_cfg)->val;
}

unsigned aloe_cfg_uint(void *_cfg) {
	return *(unsigned*)&((aloe_cfg_t*)_cfg)->val;
}

long aloe_cfg_long(void *_cfg) {
	return *(long*)&((aloe_cfg_t*)_cfg)->val;
}

unsigned long aloe_cfg_ulong(void *_cfg) {
	return *(unsigned long*)&((aloe_cfg_t*)_cfg)->val;
}

double aloe_cfg_double(void *_cfg) {
	return *(double*)&((aloe_cfg_t*)_cfg)->val;
}

void* aloe_cfg_pointer(void *_cfg) {
	return *(void**)&((aloe_cfg_t*)_cfg)->val;
}

const aloe_buf_t* aloe_cfg_data(void *_cfg) {
	return (const aloe_buf_t*)&((aloe_cfg_t*)_cfg)->val;
}
