/**
 * @author joelai
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#define jl_m(_lvl, _fmt, _args...) printf(_lvl "%s #%d " _fmt, __func__, __LINE__, ##_args)
#define jl_e(_args...) jl_m("ERROR ", ##_args)
#define jl_d(_args...) jl_m("Debug ", ##_args)

#define jl_arraysize(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

#define AU_MS2SZ(_rate, _ch, _samp, _ms) ( \
		(_rate) * (_ch) * (_samp) * (_ms) / 1000)
#define AUBUF_SZ AU_MS2SZ(44100, 2, sizeof(float), 50)
#define AUBUF_THR (AUBUF_SZ / 2)

typedef struct {
	AVFormatContext *fmt_ctx;

    const AVCodec *codec;
    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser;
    AVPacket *pkt;
	AVFrame *frm;

	SwrContext *swr_ctx;

	void *aufd, *ofd;
	void *aubuf;
	size_t aubuf_pos, aubuf_len;

	struct {
		size_t aubuf_prog; /**< current input buffer progress */
		size_t au_prog; /**< current input process progress */
		size_t samp_prog; /**< current sample progress */
	} stat;

} dec_t;

static const char *opt_ifn = "fftest1.aac";
static const char *opt_ofmt = "s32le";
static const char *opt_ofn = NULL;

static const char opt_short[] = "-:hi:f:o:";
static struct option opt_long[] = {
	{"help", no_argument, NULL, 'h'},
	{"input", required_argument, NULL, 'i'},
	{"format", required_argument, NULL, 'f'},
	{"output", required_argument, NULL, 'o'},
	{0},
};

static void help(int argc, char **argv) {
	int i;

#if 1
	for (i = 0; i < argc; i++) {
		jl_d("argv[%d/%d]: %s\n", i + 1, argc, argv[i]);
	}
#endif

	printf(
		"COMMAND\n"
		"    %s [OPTIONS]\n"
		"\n"
		"OPTIONS\n"
		"    -h, --help      Show help\n"
		"    -i, --input[=INPUT]\n"
		"                    Input aac file [default: %s]\n"
		"    -f, --format[=FORMAT]\n"
		"                    Output sample format [default: %s]\n"
		"    -o, --output[=] Output file\n"
		"\n",
		(argc > 0) && argv && argv[0] ? argv[0] : "Program",
		opt_ifn, opt_ofmt);
}

static int decode(dec_t *dec, void *ctx) {
	int res;
	char err_str[100];

	if ((res = avcodec_send_packet(dec->codec_ctx, dec->pkt)) < 0) {
		av_strerror(res, err_str, sizeof(err_str));
		jl_e("err[%d]: %s\n", res, err_str);
		return res;
	}
	while (res >= 0) {
		int samp_sz;

		res = avcodec_receive_frame(dec->codec_ctx, dec->frm);
		if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
			samp_sz = av_get_bytes_per_sample(dec->codec_ctx->sample_fmt);
			jl_d("sample size: %d, channels: %d, input %ld/%ld"
					", samples: %ld(%d added)\n", samp_sz, dec->codec_ctx->channels,
					(long)dec->stat.au_prog, (long)dec->stat.aubuf_prog,
					(long)dec->stat.samp_prog, dec->frm->nb_samples);
			return 0;
		} else if (res < 0) {
			av_strerror(res, err_str, sizeof(err_str));
			jl_e("err[%d]: %s\n", res, err_str);
			return res;
		}
		if ((samp_sz = av_get_bytes_per_sample(
				dec->codec_ctx->sample_fmt)) < 0) {
			jl_e("get sample size\n");
			return -1;
		}
		dec->stat.samp_prog += dec->frm->nb_samples;
		jl_d("sample size: %d, channels: %d, input %ld/%ld"
				", samples: %ld(%d added)\n", samp_sz, dec->codec_ctx->channels,
				(long)dec->stat.au_prog, (long)dec->stat.aubuf_prog,
				(long)dec->stat.samp_prog, dec->frm->nb_samples);
		if (dec->ofd) {
			int i, ch;

			for (i = 0; i < dec->frm->nb_samples; i++) {
				for (ch = 0; ch < dec->codec_ctx->channels; ch++) {
					fwrite(dec->frm->data[ch] + samp_sz * i, 1, samp_sz,
							dec->ofd);
				}
			}
		}
	}
	return res;
}

int main(int argc, char **argv) {
	int opt_op, opt_idx, i;
	dec_t *dec = NULL;
	const char *str;

	if (argc > 1) opt_ifn = argv[1];
	optind = 0;
	while ((opt_op = getopt_long(argc, argv, opt_short, opt_long,
			&opt_idx)) != -1) {
		if (opt_op == 'h') {
			help(argc, argv);
			goto finally;
		}
		if (opt_op == 'i' || opt_op == 1) {
			opt_ifn = optarg;
			continue;
		}
		if (opt_op == 'f') {
			opt_ofmt = optarg;
			continue;
		}
		if (opt_op == 'o') {
			opt_ofn = optarg;
			continue;
		}
	}

	if ((dec = (dec_t*)calloc(1, sizeof(*dec) + AUBUF_SZ +
			AV_INPUT_BUFFER_PADDING_SIZE)) == NULL) {
		jl_e("ENOMEM\n");
		goto finally;
	}
	dec->aubuf = (void*)(dec + 1);

	if ((dec->pkt = av_packet_alloc()) == NULL) {
		jl_e("alloc dec pkt\n");
		goto finally;
	}
    if ((dec->codec = avcodec_find_decoder(AV_CODEC_ID_AAC)) == NULL) {
        jl_e("Codec not find!\n");
		goto finally;
    }
    if ((dec->parser = av_parser_init(dec->codec->id)) == NULL) {
        jl_e("Parser not find!\n");
		goto finally;
    }
    if ((dec->codec_ctx = avcodec_alloc_context3(dec->codec)) == NULL) {
        jl_e("alloc codex\n");
		goto finally;
	}
    if (avcodec_open2(dec->codec_ctx, dec->codec, NULL) < 0) {
        jl_e("open codex\n");
		goto finally;
    }

	if (opt_ofn && (dec->ofd = (void*)fopen(opt_ofn, "wb")) == NULL) {
        jl_e("open %s\n", opt_ofn);
		goto finally;
	}

	if ((dec->aufd = (void*)fopen(opt_ifn, "rb")) == NULL) {
        jl_e("open %s\n", opt_ifn);
		goto finally;
	}

	if ((dec->frm = av_frame_alloc()) == NULL) {
		jl_e("alloc dec frm\n");
		goto finally;
	}

	dec->aubuf_pos = 0;
	dec->aubuf_len = fread(dec->aubuf, 1, AUBUF_SZ, dec->aufd);
	if (dec->aubuf_len < 0) {
		fclose((FILE*)dec->aufd);
		dec->aufd = NULL;
		jl_d("eof\n");
	} else if (dec->aubuf_len > 0) {
		dec->stat.aubuf_prog += dec->aubuf_len;
		jl_d("input %ld/%ld(%d added) bytes\n", (long)dec->stat.au_prog,
				(long)dec->stat.aubuf_prog, (int)dec->aubuf_len);
	}
	while (dec->aubuf_len > 0) {
		int psz;
        if ((psz = av_parser_parse2(dec->parser, dec->codec_ctx,
				&dec->pkt->data, &dec->pkt->size,
				(char*)dec->aubuf + dec->aubuf_pos, dec->aubuf_len,
				AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0)) < 0) {
			jl_e("parse aac\n");
			goto finally;
		}
		dec->aubuf_pos += psz;
		dec->aubuf_len -= psz;
		dec->stat.au_prog += psz;
		if (dec->pkt->size && (decode(dec, NULL)) < 0) {
			jl_e("decode aac\n");
			goto finally;
		}
		if ((dec->aubuf_len < AUBUF_THR) && dec->aufd) {
			if (dec->aubuf_len < 0) {
				dec->aubuf_len = 0;
			} else if (dec->aubuf_len > 0) {
				memmove(dec->aubuf, (char*)dec->aubuf + dec->aubuf_pos,
						dec->aubuf_len);
			}
			dec->aubuf_pos = 0;
			psz = fread((char*)dec->aubuf + dec->aubuf_len, 1,
					AUBUF_SZ - dec->aubuf_len, dec->aufd);
			if (psz < 0) {
				fclose((FILE*)dec->aufd);
				dec->aufd = NULL;
				jl_d("eof\n");
			} else if (psz > 0) {
				dec->aubuf_len += psz;
				dec->stat.aubuf_prog += psz;
				jl_d("input %ld/%ld(%d added) bytes\n", (long)dec->stat.au_prog,
						(long)dec->stat.aubuf_prog, psz);
			}
		}
	}
	dec->pkt->data = NULL; dec->pkt->size = 0;
	decode(dec, NULL);

	jl_d("channels: %d, sample rate: %d, sample format: %s\n",
			dec->codec_ctx->channels, dec->codec_ctx->sample_rate,
			av_get_sample_fmt_name(dec->codec_ctx->sample_fmt));

finally:
	if (dec->aufd) fclose((FILE*)dec->aufd);
	if (dec->ofd) fclose((FILE*)dec->ofd);
	if (dec->codec_ctx) avcodec_free_context(&dec->codec_ctx);
	if (dec->parser) av_parser_close(dec->parser);
	if (dec->swr_ctx) swr_free(&dec->swr_ctx);
	if (dec->frm) av_frame_free(&dec->frm);
	if (dec->pkt) av_packet_free(&dec->pkt);
	return 0;
}
