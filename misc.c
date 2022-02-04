/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "priv.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>


static const char* _aloe_str_negative_lut[] = {
	"no", "negative", "n", "none", "null", "empty", "false", "failure",
	"lose", "lost", "loser", NULL
};

static const char* _aloe_str_positive_lut[] = {
	"yes", "affirmative", "positive", "y", "any", "true", "success",
	"get", "got", "found", "win", "winner", "good", NULL
};

const char** aloe_str_negative_lut = _aloe_str_negative_lut;
const char** aloe_str_positive_lut = _aloe_str_positive_lut;

const char* aloe_str_find(const char **lut, const char *val, size_t len) {
	const char **r;

	if (!val) return NULL;

	for (r = lut; *r; r++) {
		if (len) {
			if (strncasecmp(*r, val, len) == 0) return *r;
		} else {
			if (strcasecmp(*r, val) == 0) return *r;
		}
	}
	return NULL;
}

size_t aloe_rinbuf_read(aloe_buf_t *buf, void *data, size_t sz) {
	size_t rw_sz, ret_sz;

	if (sz > buf->lmt) sz = buf->lmt;
	ret_sz = sz;

	// rinbuf max continuous readable size: min(lmt, (cap - pos))
	while ((rw_sz = aloe_min(sz, buf->cap - buf->pos)) > 0) {
		memcpy(data, (char*)buf->data + buf->pos, rw_sz);
		buf->pos = (buf->pos + rw_sz) % buf->cap;
		buf->lmt -= rw_sz;
		if (rw_sz >= sz) break;
		sz -= rw_sz;
		data = (char*)data + rw_sz;
	}
	return ret_sz;
}

size_t aloe_rinbuf_write(aloe_buf_t *buf, const void *data, size_t sz) {
	size_t rw_sz, ret_sz;

	ret_sz = aloe_min(sz, buf->cap - buf->lmt);

	// rinbuf max continuous writable position: wpos = ((pos + lmt) % cap)
	while ((rw_sz = aloe_min(sz, buf->cap - buf->lmt)) > 0) {
		int rw_pos = (buf->pos + buf->lmt) % buf->cap;
		if (rw_sz > buf->cap - rw_pos) rw_sz = buf->cap - rw_pos;
		memcpy((char*)buf->data + rw_pos, data, rw_sz);
		buf->lmt += rw_sz;
		if (rw_sz >= sz) break;
		sz -= rw_sz;
		data = (char*)data + rw_sz;
	}
	return ret_sz;
}

void aloe_buf_shift_left(aloe_buf_t *buf, size_t offset) {
	if (!buf->data || offset > buf->pos || buf->pos > buf->lmt) return;
	memmove(buf->data, (char*)buf->data + offset, buf->pos - offset);
	buf->pos -= offset;
	buf->lmt -= offset;
}

int aloe_buf_expand(aloe_buf_t *buf, size_t cap, aloe_buf_flag_t retain) {
	void *data;

	if (cap <= 0 || buf->cap >= cap) return 0;
	if (!(data = malloc(cap))) return ENOMEM;
	if (buf->data) {
		if (retain == aloe_buf_flag_retain_rinbuf) {
			aloe_rinbuf_read(buf, data, buf->lmt);
			buf->pos = 0;
		} else if (retain == aloe_buf_flag_retain_index) {
			memcpy(data, (char*)buf->data, buf->pos);
			if (buf->lmt == buf->cap) buf->lmt = cap;
		}
		free(buf->data);
	} else if (retain == aloe_buf_flag_retain_index) {
		buf->lmt = cap;
	}
	buf->data = data;
	buf->cap = cap;
	return 0;
}

int aloe_buf_vprintf(aloe_buf_t *buf, const char *fmt, va_list va) {
	int r;

	r = vsnprintf((char*)buf->data + buf->pos, buf->lmt - buf->pos, fmt, va);
	if (r < 0 || r >= buf->lmt - buf->pos) return -1;
	buf->pos += r;
	return r;
}

int aloe_buf_printf(aloe_buf_t *buf, const char *fmt, ...) {
	int r;
	va_list va;

	if (!fmt) return 0;
	va_start(va, fmt);
	r = aloe_buf_vprintf(buf, fmt, va);
	va_end(va);
	return r;
}

int aloe_buf_vaprintf(aloe_buf_t *buf, ssize_t max, const char *fmt,
		va_list va) {
	int r;

	if (!fmt || !fmt[0]) return 0;

	if (max == 0 || buf->lmt != buf->cap) {
		return aloe_buf_vprintf(buf, fmt, va);
	}
	if (aloe_buf_expand(buf, ((max > 0 && max < 32) ? max : 32),
			aloe_buf_flag_retain_index) != 0) {
		return -1;
	}

	while (1) {
		va_list vb;

#if __STDC_VERSION__ < 199901L
#  warning "va_copy() may require C99"
#endif
		va_copy(vb, va);
		r = aloe_buf_vprintf(buf, fmt, vb);
		if (r < 0 || r >= buf->cap) {
			if (max > 0 && buf->cap >= max) return -1;
			r = buf->cap * 2;
			if (max > 0 && r > max) r = max;
			if (aloe_buf_expand(buf, r, aloe_buf_flag_retain_index) != 0) {
				va_end(vb);
				return -1;
			}
			va_end(vb);
			continue;
		}
		va_end(vb);
		return r;
	};
}

int aloe_buf_aprintf(aloe_buf_t *buf, ssize_t max, const char *fmt, ...) {
	int r;
	va_list va;

	if (!fmt) return 0;
	va_start(va, fmt);
	r = aloe_buf_vaprintf(buf, max, fmt, va);
	va_end(va);
	return r;
}

ssize_t aloe_file_size(const void *f, int fd) {
	struct stat st;
	int r;

	if (fd == 1) {
		r = fstat((int)(long)f, &st);
	} else {
		r = stat((char*)f, &st);
	}
	if (r == 0) return st.st_size;
	r = errno;
	if (r == ENOENT) return 0;
	return -1;
}

ssize_t aloe_file_vfprintf2(const char *fname, const char *mode,
		const char *fmt, va_list va) {
	int r;
	FILE *fp = NULL;

	if (!fmt) return 0;
	if (!(fp = fopen(fname, mode))) {
		r = errno;
//		log_e("Failed open %s: %s\n", fname, strerror(r));
		r = -1;
		goto finally;
	}
	if ((r = vfprintf(fp, fmt, va)) < 0) {
		r = errno;
//		log_e("Failed write %s: %s\n", fname, strerror(r));
		r = -1;
		goto finally;
	}
	fflush(fp);
finally:
	if (fp) fclose(fp);
	return r;
}

ssize_t aloe_file_fprintf2(const char *fname, const char *mode,
		const char *fmt, ...) {
	va_list va;
	int r;

	va_start(va, fmt);
	r = aloe_file_vfprintf2(fname, mode, fmt, va);
	va_end(va);
	return r;
}

int aloe_file_nonblock(int fd, int en) {
	int r;

	if ((r = fcntl(fd, F_GETFL, NULL)) == -1) {
		r = errno;
		log_e("Failed to get file flag: %s(%d)\n", strerror(r), r);
		return r;
	}
	if (en) r |= O_NONBLOCK;
	else r &= (~O_NONBLOCK);
	if ((r = fcntl(fd, F_SETFL, r)) != 0) {
		r = errno;
		log_e("Failed to set nonblocking file flag: %s(%d)\n", strerror(r), r);
		return r;
	}
	return 0;
}

int aloe_ip_listener(struct sockaddr *sa, int backlog) {
	int fd = -1, r, af = aloe_sockaddr_family(sa);
	socklen_t sa_len;

	switch (af) {
	case AF_INET:
		sa_len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		sa_len = sizeof(struct sockaddr_in6);
		break;
	case AF_UNIX:
		sa_len = sizeof(struct sockaddr_un);
		break;
	default:
		log_e("Unknown socket type\n");
		return -1;
	}

	if ((fd = socket(af, SOCK_STREAM, 0)) == -1) {
		r = errno;
		log_e("Failed create ip socket, %s(%d)\n", strerror(r), r);
		return -1;
	}
	if (af == AF_INET || af == AF_INET6) {
		r = 1;
		if ((r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r))) < 0) {
			r = errno;
			close(fd);
			log_e("Failed set ip socket reuseaddr, %s(%d)\n", strerror(r), r);
			return -1;
		}
	}
	if ((r = bind(fd, sa, sa_len)) < 0) {
		r = errno;
		close(fd);
		log_e("Failed bind ip socket, %s(%d)\n", strerror(r), r);
		return -1;
	}
	if ((r = listen(fd, backlog)) < 0) {
		r = errno;
		close(fd);
		log_e("Failed listen ip socket, %s(%d)\n", strerror(r), r);
		return -1;
	}
	return fd;
}

