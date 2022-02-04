/**
 * @author joelai
 */

#ifndef _H_ALOE_MAIN
#define _H_ALOE_MAIN

/** @mainpage
 *
 * # Introduction
 *
 * - Hack to check class size when build time. Referenced from <a href="https://stackoverflow.com/a/53884709">here</a>
 *   > ```
 *   > char checker(int);
 *   > char checkSizeOfInt[sizeof(usart->ctrla)] = {checker(&checkSizeOfInt)};
 *   > ```
 *   > ```
 *   > note: expected 'int' but argument is of type 'char (*)[4]'
 *   > ```
 */

/** @defgroup ALOE
 * @brief aloe namespace
 *
 * @defgroup ALOE_MISC Miscellaneous
 * @ingroup ALOE
 * @brief Trivial operation.
 *
 * @defgroup ALOE_TIME Time
 * @ingroup ALOE
 * @brief Time variable.
 *
 * @defgroup ALOE_CFG
 * @ingroup ALOE
 * @brief Shared variable
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

//#include <sys/socket.h>
//#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

#include <sys/un.h>

//#include <aloe/ev.h>

#include <compat/openbsd/sys/tree.h>
#include <compat/openbsd/sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup ALOE_EV_MISC
 * @{
 */

#define aloe_trex "🦖" /**< Trex. */
#define aloe_sauropod "🦕" /**< Sauropod. */
#define aloe_lizard "🦎" /**< Lizard. */

#define aloe_min(_a, _b) ((_a) <= (_b) ? (_a) : (_b))
#define aloe_max(_a, _b) ((_a) >= (_b) ? (_a) : (_b))
#define aloe_arraysize(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

/** Stringify. */
#define _aloe_stringify(_s) # _s

/** Stringify support macro expansion. */
#define aloe_stringify(_s) _aloe_stringify(_s)

/** Fetch container object from a containing member object. */
#define aloe_container_of(_obj, _type, _member) \
	((_type *)((_obj) ? ((char*)(_obj) - offsetof(_type, _member)) : NULL))

/** String concatenate. */
#define _aloe_concat(_s1, _s2) _s1 ## _s2

/** String concatenate support macro expansion. */
#define aloe_concat(_s1, _s2) _aloe_concat(_s1, _s2)

#ifdef __GNUC__
#define aloe_offsetof(_s, _m) offsetof(_s, _m)
#else
#define aloe_offsetof(_s, _m) (((_s*)NULL)->_m)
#endif

extern const char** aloe_str_negative_lut;
extern const char** aloe_str_positive_lut;
const char* aloe_str_find(const char **lut, const char *val, size_t len);

/** Generic buffer holder. */
typedef struct aloe_buf_rec {
	void *data; /**< Memory pointer. */
	size_t cap; /**< Memory capacity. */
	size_t lmt; /**< Data size. */
	size_t pos; /**< Data start. */
} aloe_buf_t;

/**
 * fb.pos: Start of valid data
 * fb.lmt: Size of valid data
 *
 * @param fb
 * @param data
 * @param sz
 * @return
 */
size_t aloe_rinbuf_read(aloe_buf_t *buf, void *data, size_t sz);

/**
 * fb.pos: Start of spare
 * fb.lmt: Size of valid data
 *
 * @param fb
 * @param data
 * @param sz
 * @return
 */
size_t aloe_rinbuf_write(aloe_buf_t *buf, const void *data, size_t sz);

#define aloe_buf_clear(_buf) do {(_buf)->lmt = (_buf)->cap; (_buf)->pos = 0;} while (0)
#define aloe_buf_flip(_buf) do {(_buf)->lmt = (_buf)->pos; (_buf)->pos = 0;} while (0)

void aloe_buf_shift_left(aloe_buf_t *buf, size_t offset);

typedef enum aloe_buf_flag_enum {
	aloe_buf_flag_none = 0,
	aloe_buf_flag_retain_rinbuf,
	aloe_buf_flag_retain_index,
} aloe_buf_flag_t;

/**
 *
 * aloe_buf_flag_retain_pos will update lmt if lmt == cap
 *
 * @param buf
 * @param cap
 * @param retain
 * @return
 */
int aloe_buf_expand(aloe_buf_t *buf, size_t cap, aloe_buf_flag_t retain);

/**
 *
 * Update buf index when all fmt fulfill.
 *
 * @param buf
 * @param fmt
 * @param va
 * @return -1 when error or not fulfill formating, otherwise length to append
 */
int aloe_buf_vprintf(aloe_buf_t *buf, const char *fmt, va_list va)
		__attribute__((format(printf, 2, 0)));
int aloe_buf_printf(aloe_buf_t *buf, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));

/**
 *
 * Malloc with max and buf.lmt == cap
 *
 * @param buf
 * @param max
 * @param fmt
 * @param va
 * @return
 */
int aloe_buf_vaprintf(aloe_buf_t *buf, ssize_t max, const char *fmt,
		va_list va) __attribute__((format(printf, 3, 0)));
int aloe_buf_aprintf(aloe_buf_t *buf, ssize_t max, const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));

typedef struct aloe_mod_rec {
	const char *name;
	void* (*init)(void);
	void (*destroy)(void*);
	int (*ioctl)(void*, void*);
} aloe_mod_t;

/**
 *
 * @param f
 * @param fd fd=1 if f is file descriptor, otherwise assume f is path
 * @return
 */
ssize_t aloe_file_size(const void *f, int fd);

/**
 *
 * @param fname
 * @param mode ref to fopen
 * @param fmt
 * @param va
 * @return Size of written or -1 when failure
 */
ssize_t aloe_file_vfprintf2(const char *fname, const char *mode,
		const char *fmt, va_list va) __attribute__((format(printf, 3, 0)));
ssize_t aloe_file_fprintf2(const char *fname, const char *mode,
		const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/** Check the error number indicate nonblocking state. */
#define ALOE_ENO_NONBLOCKING(_e) ((_e) == EAGAIN || (_e) == EINPROGRESS)

/** Set nonblocking flag. */
int aloe_file_nonblock(int fd, int en);

#define aloe_sockaddr_family(_sa) ((((struct sockaddr_in*)(_sa))->sin_family == AF_INET) ? AF_INET : \
		(((struct sockaddr_in6*)(_sa))->sin6_family == AF_INET6) ? AF_INET6 : \
		(((struct sockaddr_un*)(_sa))->sun_family == AF_UNIX) ? AF_UNIX : \
		AF_UNSPEC)

int aloe_ip_listener(struct sockaddr*, int backlog);

#define aloe_ipv6_listener(_p, _bg) aloe_ip_listener(&(struct sockaddr_in6){ \
	.sin6_port = htons(_p), .sin6_family = AF_INET6, .sin6_addr = in6addr_any}, \
	_bg)

#define aloe_ipv4_listener(_p, _bg) aloe_ip_listener((struct sockaddr*)&((struct sockaddr_in){ \
	.sin_port = htons(_p), .sin_family = AF_INET, .sin_addr = {INADDR_ANY}}), \
	_bg)

#define aloe_local_socket_listener(_p, _bg) aloe_ip_listener(&(struct sockaddr_un){ \
	.sun_path = htons(_p), .sun_family = AF_UNIX}, _bg)

/** @} ALOE_EV_MISC */

/** @addtogroup ALOE_EV_TIME
 * @{
 */

/** Formalize time value. */
#define ALOE_TIMESEC_NORM(_sec, _subsec, _subscale) if ((_subsec) >= _subscale) { \
		(_sec) += (_subsec) / _subscale; \
		(_subsec) %= (_subscale); \
	}

/** Compare 2 time value. */
#define ALOE_TIMESEC_CMP(_a_sec, _a_subsec, _b_sec, _b_subsec) ( \
	((_a_sec) > (_b_sec)) ? 1 : \
	((_a_sec) < (_b_sec)) ? -1 : \
	((_a_subsec) > (_b_subsec)) ? 1 : \
	((_a_subsec) < (_b_subsec)) ? -1 : \
	0)

/** Subtraction for time value. */
#define ALOE_TIMESEC_SUB(_a_sec, _a_subsec, _b_sec, _b_subsec, _c_sec, \
		_c_subsec, _subscale) \
	if ((_a_subsec) < (_b_subsec)) { \
		(_c_sec) = (_a_sec) - (_b_sec) - 1; \
		(_c_subsec) = (_subscale) + (_a_subsec) - (_b_subsec); \
	} else { \
		(_c_sec) = (_a_sec) - (_b_sec); \
		(_c_subsec) = (_a_subsec) - (_b_subsec); \
	}

/** Addition for time value. */
#define ALOE_TIMESEC_ADD(_a_sec, _a_subsec, _b_sec, _b_subsec, _c_sec, \
		_c_subsec, _subscale) do { \
	(_c_sec) = (_a_sec) + (_b_sec); \
	(_c_subsec) = (_a_subsec) + (_b_subsec); \
	ALOE_TIMESEC_NORM(_c_sec, _c_subsec, _subscale); \
} while(0)

/** Normalize timeval.
 *
 * |a|
 *
 * @param a
 * @return (a)
 */
struct timeval* aloe_timeval_norm(struct timeval *a);

/** Compare timeval.
 *
 * |a - b|
 *
 * @param a A normalized timeval
 * @param b A normalized timeval
 * @return 1, -1 or 0 if (a) later/early/equal then/to (b)
 */
int aloe_timeval_cmp(const struct timeval *a, const struct timeval *b);

/** Subtraction timeval.
 *
 * c = a - b
 *
 * @param a A normalized timeval, must later then (b)
 * @param b A normalized timeval, must early then (a)
 * @param c Hold result if not NULL
 * @return (c)
 */
struct timeval* aloe_timeval_sub(const struct timeval *a, const struct timeval *b,
		struct timeval *c);

/** Addition timeval.
 *
 * c = a + b
 *
 * @param a
 * @param b
 * @param c
 * @return
 */
struct timeval* aloe_timeval_add(const struct timeval *a, const struct timeval *b,
		struct timeval *c);

/** @} ALOE_EV_TIME */

/** @addtogroup ALOE_EV_CFG
 * @{
 */

typedef enum aloe_cfg_type_enum {
	aloe_cfg_type_void = 0,
	aloe_cfg_type_int,
	aloe_cfg_type_uint,
	aloe_cfg_type_long,
	aloe_cfg_type_ulong,
	aloe_cfg_type_double,
	aloe_cfg_type_pointer,

	aloe_cfg_type_data, /**< set( ,const void*, size_t) */
	aloe_cfg_type_string, /**< set( ,const char*, ...) */
} aloe_cfg_type_t;

void* aloe_cfg_init(void);
void aloe_cfg_destroy(void*);

/**
 *
 * !key && type == aloe_cfg_type_void -> reset all cfg
 *
 * @param
 * @param
 * @param
 */
void* aloe_cfg_set(void*, const char*, aloe_cfg_type_t, ...);

/**
 *
 * !key -> return first iter
 *
 * @param
 * @param
 */
void* aloe_cfg_find(void*, const char*);

/**
 *
 * !prev -> return first iter
 *
 * @param
 * @param
 */
void* aloe_cfg_next(void*, void*);
const char* aloe_cfg_key(void*);
aloe_cfg_type_t aloe_cfg_type(void*);
int aloe_cfg_int(void*);
unsigned aloe_cfg_uint(void*);
long aloe_cfg_long(void*);
unsigned long aloe_cfg_ulong(void*);
double aloe_cfg_double(void*);
void* aloe_cfg_pointer(void*);
const aloe_buf_t* aloe_cfg_data(void*);

/** @} ALOE_EV_CFG */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _H_ALOE_MAIN */
