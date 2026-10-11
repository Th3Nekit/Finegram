/*
 * copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_AVCODEC_H
#define AVCODEC_AVCODEC_H

#include "libavutil/samplefmt.h"
#include "libavutil/attributes.h"
#include "libavutil/avutil.h"
#include "libavutil/buffer.h"
#include "libavutil/channel_layout.h"
#include "libavutil/dict.h"
#include "libavutil/frame.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

#include "codec.h"
#include "codec_id.h"
#include "defs.h"
#include "packet.h"
#include "version_major.h"
#ifndef HAVE_AV_CONFIG_H

#include "version.h"

#include "codec_desc.h"
#include "codec_par.h"
#endif

struct AVCodecParameters;

typedef struct RcOverride{
    int start_frame;
    int end_frame;
    int qscale;
    float quality_factor;
} RcOverride;

#define AV_CODEC_FLAG_UNALIGNED       (1 <<  0)

#define AV_CODEC_FLAG_QSCALE          (1 <<  1)

#define AV_CODEC_FLAG_4MV             (1 <<  2)

#define AV_CODEC_FLAG_OUTPUT_CORRUPT  (1 <<  3)

#define AV_CODEC_FLAG_QPEL            (1 <<  4)

#define AV_CODEC_FLAG_RECON_FRAME     (1 <<  6)

#define AV_CODEC_FLAG_COPY_OPAQUE     (1 <<  7)

#define AV_CODEC_FLAG_FRAME_DURATION  (1 <<  8)

#define AV_CODEC_FLAG_PASS1           (1 <<  9)

#define AV_CODEC_FLAG_PASS2           (1 << 10)

#define AV_CODEC_FLAG_LOOP_FILTER     (1 << 11)

#define AV_CODEC_FLAG_GRAY            (1 << 13)

#define AV_CODEC_FLAG_PSNR            (1 << 15)

#define AV_CODEC_FLAG_INTERLACED_DCT  (1 << 18)

#define AV_CODEC_FLAG_LOW_DELAY       (1 << 19)

#define AV_CODEC_FLAG_GLOBAL_HEADER   (1 << 22)

#define AV_CODEC_FLAG_BITEXACT        (1 << 23)

#define AV_CODEC_FLAG_AC_PRED         (1 << 24)

#define AV_CODEC_FLAG_INTERLACED_ME   (1 << 29)
#define AV_CODEC_FLAG_CLOSED_GOP      (1U << 31)

#define AV_CODEC_FLAG2_FAST           (1 <<  0)

#define AV_CODEC_FLAG2_NO_OUTPUT      (1 <<  2)

#define AV_CODEC_FLAG2_LOCAL_HEADER   (1 <<  3)

#define AV_CODEC_FLAG2_CHUNKS         (1 << 15)

#define AV_CODEC_FLAG2_IGNORE_CROP    (1 << 16)

#define AV_CODEC_FLAG2_SHOW_ALL       (1 << 22)

#define AV_CODEC_FLAG2_EXPORT_MVS     (1 << 28)

#define AV_CODEC_FLAG2_SKIP_MANUAL    (1 << 29)

#define AV_CODEC_FLAG2_RO_FLUSH_NOOP  (1 << 30)

#define AV_CODEC_FLAG2_ICC_PROFILES   (1U << 31)

#define AV_CODEC_EXPORT_DATA_MVS         (1 << 0)

#define AV_CODEC_EXPORT_DATA_PRFT        (1 << 1)

#define AV_CODEC_EXPORT_DATA_VIDEO_ENC_PARAMS (1 << 2)

#define AV_CODEC_EXPORT_DATA_FILM_GRAIN (1 << 3)

#define AV_CODEC_EXPORT_DATA_ENHANCEMENTS (1 << 4)

#define AV_GET_BUFFER_FLAG_REF (1 << 0)

#define AV_GET_ENCODE_BUFFER_FLAG_REF (1 << 0)

#define AV_CODEC_RECEIVE_FRAME_FLAG_SYNCHRONOUS (1 << 0)

typedef struct AVCodecContext {

    const AVClass *av_class;
    int log_level_offset;

    enum AVMediaType codec_type;
    const struct AVCodec  *codec;
    enum AVCodecID     codec_id;

    unsigned int codec_tag;

    void *priv_data;

    struct AVCodecInternal *internal;

    void *opaque;

    int64_t bit_rate;

    int flags;

    int flags2;

    uint8_t *extradata;
    int extradata_size;

    AVRational time_base;

    AVRational pkt_timebase;

    AVRational framerate;

    int delay;

    int width, height;

    int coded_width, coded_height;

    AVRational sample_aspect_ratio;

    enum AVPixelFormat pix_fmt;

    enum AVPixelFormat sw_pix_fmt;

    enum AVColorPrimaries color_primaries;

    enum AVColorTransferCharacteristic color_trc;

    enum AVColorSpace colorspace;

    enum AVColorRange color_range;

    enum AVChromaLocation chroma_sample_location;

    enum AVFieldOrder field_order;

    int refs;

    int has_b_frames;

    int slice_flags;
#define SLICE_FLAG_CODED_ORDER    0x0001 ///< draw_horiz_band() is called in coded order instead of display
#define SLICE_FLAG_ALLOW_FIELD    0x0002 ///< allow draw_horiz_band() with field slices (MPEG-2 field pics)
#define SLICE_FLAG_ALLOW_PLANE    0x0004 ///< allow draw_horiz_band() with 1 component at a time (SVQ1)

    void (*draw_horiz_band)(struct AVCodecContext *s,
                            const AVFrame *src, int offset[AV_NUM_DATA_POINTERS],
                            int y, int type, int height);

    enum AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat * fmt);

    int max_b_frames;

    float b_quant_factor;

    float b_quant_offset;

    float i_quant_factor;

    float i_quant_offset;

    float lumi_masking;

    float temporal_cplx_masking;

    float spatial_cplx_masking;

    float p_masking;

    float dark_masking;

     int nsse_weight;

    int me_cmp;

    int me_sub_cmp;

    int mb_cmp;

    int ildct_cmp;
#define FF_CMP_SAD          0
#define FF_CMP_SSE          1
#define FF_CMP_SATD         2
#define FF_CMP_DCT          3
#define FF_CMP_PSNR         4
#define FF_CMP_BIT          5
#define FF_CMP_RD           6
#define FF_CMP_ZERO         7
#define FF_CMP_VSAD         8
#define FF_CMP_VSSE         9
#define FF_CMP_NSSE         10
#define FF_CMP_W53          11
#define FF_CMP_W97          12
#define FF_CMP_DCTMAX       13
#define FF_CMP_DCT264       14
#define FF_CMP_MEDIAN_SAD   15
#define FF_CMP_CHROMA       256

    int dia_size;

    int last_predictor_count;

    int me_pre_cmp;

    int pre_dia_size;

    int me_subpel_quality;

    int me_range;

    int mb_decision;
#define FF_MB_DECISION_SIMPLE 0        ///< uses mb_cmp
#define FF_MB_DECISION_BITS   1        ///< chooses the one which needs the fewest bits
#define FF_MB_DECISION_RD     2        ///< rate distortion

    uint16_t *intra_matrix;

    uint16_t *inter_matrix;

    uint16_t *chroma_intra_matrix;

#if FF_API_INTRA_DC_PRECISION

    attribute_deprecated
    int intra_dc_precision;
#endif

    int mb_lmin;

    int mb_lmax;

    int bidir_refine;

    int keyint_min;

    int gop_size;

    int mv0_threshold;

    int slices;

    int sample_rate;

    enum AVSampleFormat sample_fmt;

    AVChannelLayout ch_layout;

    int frame_size;

    int block_align;

    int cutoff;

    enum AVAudioServiceType audio_service_type;

    enum AVSampleFormat request_sample_fmt;

    int initial_padding;

    int trailing_padding;

    int seek_preroll;

    int (*get_buffer2)(struct AVCodecContext *s, AVFrame *frame, int flags);

    int bit_rate_tolerance;

    int global_quality;

    int compression_level;
#define FF_COMPRESSION_DEFAULT -1

    float qcompress;
    float qblur;

    int qmin;

    int qmax;

    int max_qdiff;

    int rc_buffer_size;

    int rc_override_count;
    RcOverride *rc_override;

    int64_t rc_max_rate;

    int64_t rc_min_rate;

    float rc_max_available_vbv_use;

    float rc_min_vbv_overflow_use;

    int rc_initial_buffer_occupancy;

    int trellis;

    char *stats_out;

    char *stats_in;

    int workaround_bugs;
#define FF_BUG_AUTODETECT       1  ///< autodetection
#define FF_BUG_XVID_ILACE       4
#define FF_BUG_UMP4             8
#define FF_BUG_NO_PADDING       16
#define FF_BUG_AMV              32
#define FF_BUG_QPEL_CHROMA      64
#define FF_BUG_STD_QPEL         128
#define FF_BUG_QPEL_CHROMA2     256
#define FF_BUG_DIRECT_BLOCKSIZE 512
#define FF_BUG_EDGE             1024
#define FF_BUG_HPEL_CHROMA      2048
#define FF_BUG_DC_CLIP          4096
#define FF_BUG_MS               8192 ///< Work around various bugs in Microsoft's broken decoders.
#define FF_BUG_TRUNCATED       16384
#define FF_BUG_IEDGE           32768

    int strict_std_compliance;

    int error_concealment;
#define FF_EC_GUESS_MVS   1
#define FF_EC_DEBLOCK     2
#define FF_EC_FAVOR_INTER 256

    int debug;
#define FF_DEBUG_PICT_INFO   1
#define FF_DEBUG_RC          2
#define FF_DEBUG_BITSTREAM   4
#define FF_DEBUG_MB_TYPE     8
#define FF_DEBUG_QP          16
#define FF_DEBUG_DCT_COEFF   0x00000040
#define FF_DEBUG_SKIP        0x00000080
#define FF_DEBUG_STARTCODE   0x00000100
#define FF_DEBUG_ER          0x00000400
#define FF_DEBUG_MMCO        0x00000800
#define FF_DEBUG_BUGS        0x00001000
#define FF_DEBUG_BUFFERS     0x00008000
#define FF_DEBUG_THREADS     0x00010000
#define FF_DEBUG_GREEN_MD    0x00800000
#define FF_DEBUG_NOMC        0x01000000

    int err_recognition;

    const struct AVHWAccel *hwaccel;

    void *hwaccel_context;

    AVBufferRef *hw_frames_ctx;

    AVBufferRef *hw_device_ctx;

    int hwaccel_flags;

    int extra_hw_frames;

    uint64_t error[AV_NUM_DATA_POINTERS];

    int dct_algo;
#define FF_DCT_AUTO    0
#define FF_DCT_FASTINT 1
#define FF_DCT_INT     2
#define FF_DCT_MMX     3
#define FF_DCT_ALTIVEC 5
#define FF_DCT_FAAN    6
#define FF_DCT_NEON    7

    int idct_algo;
#define FF_IDCT_AUTO          0
#define FF_IDCT_INT           1
#define FF_IDCT_SIMPLE        2
#define FF_IDCT_SIMPLEMMX     3
#define FF_IDCT_ARM           7
#define FF_IDCT_ALTIVEC       8
#define FF_IDCT_SIMPLEARM     10
#define FF_IDCT_XVID          14
#define FF_IDCT_SIMPLEARMV5TE 16
#define FF_IDCT_SIMPLEARMV6   17
#define FF_IDCT_FAAN          20
#define FF_IDCT_SIMPLENEON    22
#define FF_IDCT_SIMPLEAUTO    128

     int bits_per_coded_sample;

    int bits_per_raw_sample;

    int thread_count;

    int thread_type;
#define FF_THREAD_FRAME   1 ///< Decode more than one frame at once
#define FF_THREAD_SLICE   2 ///< Decode more than one part of a single frame at once

    int active_thread_type;

    int (*execute)(struct AVCodecContext *c, int (*func)(struct AVCodecContext *c2, void *arg), void *arg2, int *ret, int count, int size);

    int (*execute2)(struct AVCodecContext *c, int (*func)(struct AVCodecContext *c2, void *arg, int jobnr, int threadnr), void *arg2, int *ret, int count);

     int profile;

     int level;

#if FF_API_CODEC_PROPS

    attribute_deprecated
    unsigned properties;
#define FF_CODEC_PROPERTY_LOSSLESS        0x00000001
#define FF_CODEC_PROPERTY_CLOSED_CAPTIONS 0x00000002
#define FF_CODEC_PROPERTY_FILM_GRAIN      0x00000004
#endif

    enum AVDiscard skip_loop_filter;

    enum AVDiscard skip_idct;

    enum AVDiscard skip_frame;

    int skip_alpha;

    int skip_top;

    int skip_bottom;

     int lowres;

    const struct AVCodecDescriptor *codec_descriptor;

    char *sub_charenc;

    int sub_charenc_mode;
#define FF_SUB_CHARENC_MODE_DO_NOTHING  -1  ///< do nothing (demuxer outputs a stream supposed to be already in UTF-8, or the codec is bitmap for instance)
#define FF_SUB_CHARENC_MODE_AUTOMATIC    0  ///< libavcodec will select the mode itself
#define FF_SUB_CHARENC_MODE_PRE_DECODER  1  ///< the AVPacket data needs to be recoded to UTF-8 before being fed to the decoder, requires iconv
#define FF_SUB_CHARENC_MODE_IGNORE       2  ///< neither convert the subtitles, nor check them for valid UTF-8

    int subtitle_header_size;
    uint8_t *subtitle_header;

    uint8_t *dump_separator;

    char *codec_whitelist;

    AVPacketSideData *coded_side_data;
    int            nb_coded_side_data;

    int export_side_data;

    int64_t max_pixels;

    int apply_cropping;

    int discard_damaged_percentage;

    int64_t max_samples;

    int (*get_encode_buffer)(struct AVCodecContext *s, AVPacket *pkt, int flags);

    int64_t frame_num;

    int        *side_data_prefer_packet;

    unsigned nb_side_data_prefer_packet;

    AVFrameSideData  **decoded_side_data;
    int             nb_decoded_side_data;

    enum AVAlphaMode alpha_mode;
} AVCodecContext;

typedef struct AVHWAccel {

    const char *name;

    enum AVMediaType type;

    enum AVCodecID id;

    enum AVPixelFormat pix_fmt;

    int capabilities;
} AVHWAccel;

#define AV_HWACCEL_CODEC_CAP_EXPERIMENTAL 0x0200

#define AV_HWACCEL_FLAG_IGNORE_LEVEL (1 << 0)

#define AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH (1 << 1)

#define AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH (1 << 2)

#define AV_HWACCEL_FLAG_UNSAFE_OUTPUT (1 << 3)

enum AVSubtitleType {
    SUBTITLE_NONE,

    SUBTITLE_BITMAP,

    SUBTITLE_TEXT,

    SUBTITLE_ASS,
};

#define AV_SUBTITLE_FLAG_FORCED 0x00000001

typedef struct AVSubtitleRect {
    int x;
    int y;
    int w;
    int h;
    int nb_colors;

    uint8_t *data[4];
    int linesize[4];

    int flags;
    enum AVSubtitleType type;

    char *text;

    char *ass;
} AVSubtitleRect;

typedef struct AVSubtitle {
    uint16_t format;
    uint32_t start_display_time;
    uint32_t end_display_time;
    unsigned num_rects;
    AVSubtitleRect **rects;
    int64_t pts;
} AVSubtitle;

unsigned avcodec_version(void);

const char *avcodec_configuration(void);

const char *avcodec_license(void);

AVCodecContext *avcodec_alloc_context3(const AVCodec *codec);

void avcodec_free_context(AVCodecContext **avctx);

const AVClass *avcodec_get_class(void);

const AVClass *avcodec_get_subtitle_rect_class(void);

int avcodec_parameters_from_context(struct AVCodecParameters *par,
                                    const AVCodecContext *codec);

int avcodec_parameters_to_context(AVCodecContext *codec,
                                  const struct AVCodecParameters *par);

int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);

void avsubtitle_free(AVSubtitle *sub);

int avcodec_default_get_buffer2(AVCodecContext *s, AVFrame *frame, int flags);

int avcodec_default_get_encode_buffer(AVCodecContext *s, AVPacket *pkt, int flags);

void avcodec_align_dimensions(AVCodecContext *s, int *width, int *height);

void avcodec_align_dimensions2(AVCodecContext *s, int *width, int *height,
                               int linesize_align[AV_NUM_DATA_POINTERS]);

int avcodec_decode_subtitle2(AVCodecContext *avctx, AVSubtitle *sub,
                             int *got_sub_ptr, const AVPacket *avpkt);

int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt);

int avcodec_receive_frame_flags(AVCodecContext *avctx, AVFrame *frame, unsigned flags);

int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);

int avcodec_send_frame(AVCodecContext *avctx, const AVFrame *frame);

int avcodec_receive_packet(AVCodecContext *avctx, AVPacket *avpkt);

int avcodec_get_hw_frames_parameters(AVCodecContext *avctx,
                                     AVBufferRef *device_ref,
                                     enum AVPixelFormat hw_pix_fmt,
                                     AVBufferRef **out_frames_ref);

enum AVCodecConfig {
    AV_CODEC_CONFIG_PIX_FORMAT,
    AV_CODEC_CONFIG_FRAME_RATE,
    AV_CODEC_CONFIG_SAMPLE_RATE,
    AV_CODEC_CONFIG_SAMPLE_FORMAT,
    AV_CODEC_CONFIG_CHANNEL_LAYOUT,
    AV_CODEC_CONFIG_COLOR_RANGE,
    AV_CODEC_CONFIG_COLOR_SPACE,
    AV_CODEC_CONFIG_ALPHA_MODE,
};

int avcodec_get_supported_config(const AVCodecContext *avctx,
                                 const AVCodec *codec, enum AVCodecConfig config,
                                 unsigned flags, const void **out_configs,
                                 int *out_num_configs);

enum AVPictureStructure {
    AV_PICTURE_STRUCTURE_UNKNOWN,
    AV_PICTURE_STRUCTURE_TOP_FIELD,
    AV_PICTURE_STRUCTURE_BOTTOM_FIELD,
    AV_PICTURE_STRUCTURE_FRAME,
};

typedef struct AVCodecParserContext {
    void *priv_data;
    const struct AVCodecParser *parser;
    int64_t frame_offset;
    int64_t cur_offset;

    int64_t next_frame_offset;

    int pict_type;

    int repeat_pict;
    int64_t pts;
    int64_t dts;

    int64_t last_pts;
    int64_t last_dts;
    int fetch_timestamp;

#define AV_PARSER_PTS_NB 4
    int cur_frame_start_index;
    int64_t cur_frame_offset[AV_PARSER_PTS_NB];
    int64_t cur_frame_pts[AV_PARSER_PTS_NB];
    int64_t cur_frame_dts[AV_PARSER_PTS_NB];

    int flags;
#define PARSER_FLAG_COMPLETE_FRAMES           0x0001
#define PARSER_FLAG_ONCE                      0x0002

#define PARSER_FLAG_FETCHED_OFFSET            0x0004
#define PARSER_FLAG_USE_CODEC_TS              0x1000

    int64_t offset;
    int64_t cur_frame_end[AV_PARSER_PTS_NB];

    int key_frame;

    int dts_sync_point;

    int dts_ref_dts_delta;

    int pts_dts_delta;

    int64_t cur_frame_pos[AV_PARSER_PTS_NB];

    int64_t pos;

    int64_t last_pos;

    int duration;

    enum AVFieldOrder field_order;

    enum AVPictureStructure picture_structure;

    int output_picture_number;

    int width;
    int height;

    int coded_width;
    int coded_height;

    int format;
} AVCodecParserContext;

typedef struct AVCodecParser {
#if FF_API_PARSER_CODECID
    int codec_ids[7];
#else
    enum AVCodecID codec_ids[7];
#endif
#if FF_API_PARSER_PRIVATE

    attribute_deprecated
    int priv_data_size;
    attribute_deprecated
    int (*parser_init)(AVCodecParserContext *s);

    attribute_deprecated
    int (*parser_parse)(AVCodecParserContext *s,
                        AVCodecContext *avctx,
                        const uint8_t **poutbuf, int *poutbuf_size,
                        const uint8_t *buf, int buf_size);
    attribute_deprecated
    void (*parser_close)(AVCodecParserContext *s);
    attribute_deprecated
    int (*split)(AVCodecContext *avctx, const uint8_t *buf, int buf_size);
#endif
} AVCodecParser;

const AVCodecParser *av_parser_iterate(void **opaque);

#if FF_API_PARSER_CODECID
AVCodecParserContext *av_parser_init(int codec_id);
#else
AVCodecParserContext *av_parser_init(enum AVCodecID codec_id);
#endif

int av_parser_parse2(AVCodecParserContext *s,
                     AVCodecContext *avctx,
                     uint8_t **poutbuf, int *poutbuf_size,
                     const uint8_t *buf, int buf_size,
                     int64_t pts, int64_t dts,
                     int64_t pos);

void av_parser_close(AVCodecParserContext *s);

int avcodec_encode_subtitle(AVCodecContext *avctx, uint8_t *buf, int buf_size,
                            const AVSubtitle *sub);

unsigned int avcodec_pix_fmt_to_codec_tag(enum AVPixelFormat pix_fmt);

enum AVPixelFormat avcodec_find_best_pix_fmt_of_list(const enum AVPixelFormat *pix_fmt_list,
                                            enum AVPixelFormat src_pix_fmt,
                                            int has_alpha, int *loss_ptr);

enum AVPixelFormat avcodec_default_get_format(struct AVCodecContext *s, const enum AVPixelFormat * fmt);

void avcodec_string(char *buf, int buf_size, AVCodecContext *enc, int encode);

int avcodec_default_execute(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2),void *arg, int *ret, int count, int size);
int avcodec_default_execute2(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2, int, int),void *arg, int *ret, int count);

int avcodec_fill_audio_frame(AVFrame *frame, int nb_channels,
                             enum AVSampleFormat sample_fmt, const uint8_t *buf,
                             int buf_size, int align);

void avcodec_flush_buffers(AVCodecContext *avctx);

int av_get_audio_frame_duration(AVCodecContext *avctx, int frame_bytes);

void av_fast_padded_malloc(void *ptr, unsigned int *size, size_t min_size);

void av_fast_padded_mallocz(void *ptr, unsigned int *size, size_t min_size);

int avcodec_is_open(AVCodecContext *s);

#endif
