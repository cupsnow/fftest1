/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

static int instanceId = 0;
static char mod_name[] = "test1_buf1";

typedef struct {
	int instanceId;

} ctx_t;

static int test_buf1(void) {
	aloe_buf_t respBuf = {0}, respHdr;
#if (HAP_DYNAMIC_MEMORY_ALLOCATION == 0)
	char buf[2000];
#endif
	int hapErr;
	int hdrGap = 200;

#if (HAP_DYNAMIC_MEMORY_ALLOCATION == 0)
	respBuf.data = buf;
	respBuf.cap = sizeof(buf);
#else
    if (!(respBuf.data = (char*)malloc(respBuf.cap = 2000))) {
    	hapErr = ENOSPC;
    	goto finally;
    }
#endif
	aloe_buf_clear(&respBuf);

	// body start at hdrLmt
	respBuf.pos = hdrGap;

	respHdr = respBuf;
	// hdr take body start position as limit
	aloe_buf_flip(&respHdr);

	if (aloe_buf_printf(&respBuf,
			"<html>\r\n"
					"<head>\r\n"
					"<title>admin</title>\r\n"
					"</head>\r\n"
					"<body>\r\n"
					"<form action=\"/admin_fw_upgrade\">\r\n"
					"<label for=\"uri\">URI</label><br/>\r\n"
					"<input type=\"text\" id=\"fw_wget\" name=\"uri\" value=\"%s\"><br/>"
					"</form>\r\n"
					"</body>\r\n"
					"</html>\r\n", "http://a.b.c.d/ota.tar.gz") <= 0) {
		log_e("Insufficient memory for body\n");
		hapErr = ENOMEM;
		goto finally;
	}
	log_d("body length: %d:\n%s\n", (int)respBuf.pos - hdrGap,
			(char*)respBuf.data + hdrGap);

	if (aloe_buf_printf(&respHdr, "HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html; charset=UTF-8\r\n"
			"Content-Length: %d\r\n"
			"\r\n", (int)respBuf.pos - hdrGap) <= 0) {
		log_e("Insufficient memory for hdr\n");
		hapErr = ENOMEM;
		goto finally;
	}

	hdrGap = respHdr.lmt - respHdr.pos;

	log_d("hdr len: %d, gap: %d,\n%s\n", (int)respHdr.pos, hdrGap, (char*)respHdr.data);

	aloe_buf_flip(&respBuf);
	aloe_buf_flip(&respHdr);

	/*
	 * <respBuf>
	 * HTTP/1.1 ...
	 * <respHdr.limit>
	 * gap
	 * body...
	 * <respBuf.limit>
	 */
	respHdr.data = respBuf.data + respHdr.lmt;
	respHdr.pos = respHdr.lmt = respBuf.data + respBuf.lmt - respHdr.data;

	log_d("respBuf: %d, %d, respHdr: %d, %d\n", (int)respBuf.pos, (int)respBuf.lmt,
			(int)respHdr.pos, (int)respHdr.lmt);

	aloe_buf_shift_left(&respHdr, hdrGap);
	respBuf.lmt -= hdrGap;
	((char*)respBuf.data)[respBuf.lmt] = '\0';

	log_d("resp: %d,\n%s\n", (int)respBuf.lmt, (char*)respBuf.data);

	hapErr = 0;
finally:
//    if (hapErr != 0) WriteResponse(session, kHAPIPAccessoryServerResponse_InternalServerError);
#if !(HAP_DYNAMIC_MEMORY_ALLOCATION == 0)
    if (respBuf.data) free(respBuf.data);
#endif
	return hapErr;
}

static void* init(void) {
	ctx_t *ctx;

	if ((ctx = malloc(sizeof(*ctx))) == NULL) {
		log_e("alloc instance\n");
		return NULL;
	}
	ctx->instanceId = instanceId++;
	log_d("%s[%d]\n", mod_name, ctx->instanceId);

	test_buf1();

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

const aloe_mod_t mod_test1_buf1 = {.name = mod_name, .init = &init, .destroy =
		&destroy, .ioctl = &ioctl};
