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

#ifndef AVFORMAT_AVFORMAT_H
#define AVFORMAT_AVFORMAT_H

#include <stdio.h>

#include "libavcodec/codec_par.h"
#include "libavcodec/defs.h"
#include "libavcodec/packet.h"

#include "libavutil/dict.h"
#include "libavutil/log.h"

#include "avio.h"
#include "libavformat/version_major.h"
#ifndef HAVE_AV_CONFIG_H

#include "libavformat/version.h"

#include "libavutil/frame.h"
#include "libavcodec/codec.h"
#endif

struct AVFormatContext;
struct AVFrame;

/**
 * @defgroup metadata_api Public Metadata API
 * @{
 * @ingroup libavf
 * The metadata API allows libavformat to export metadata tags to a client
 * application when demuxing. Conversely it allows a client application to
 * set metadata when muxing.
 *
 * Metadata is exported or set as pairs of key/value strings in the 'metadata'
 * fields of the AVFormatContext, AVStream, AVChapter and AVProgram structs
 * using the @ref lavu_dict "AVDictionary" API. Like all strings in FFmpeg,
 * metadata is assumed to be UTF-8 encoded Unicode. Note that metadata
 * exported by demuxers isn't checked to be valid UTF-8 in most cases.
 *
 * Important concepts to keep in mind:
 * -  Keys are unique; there can never be 2 tags with the same key. This is
 *    also meant semantically, i.e., a demuxer should not knowingly produce
 *    several keys that are literally different but semantically identical.
 *    E.g., key=Author5, key=Author6. In this example, all authors must be
 *    placed in the same tag.
 * -  Metadata is flat, not hierarchical; there are no subtags. If you
 *    want to store, e.g., the email address of the child of producer Alice
 *    and actor Bob, that could have key=alice_and_bobs_childs_email_address.
 * -  Several modifiers can be applied to the tag name. This is done by
 *    appending a dash character ('-') and the modifier name in the order
 *    they appear in the list below -- e.g. foo-eng-sort, not foo-sort-eng.
 *    -  language -- a tag whose value is localized for a particular language
 *       is appended with the ISO 639-2/B 3-letter language code.
 *       For example: Author-ger=Michael, Author-eng=Mike
 *       The original/default language is in the unqualified "Author" tag.
 *       A demuxer should set a default if it sets any translated tag.
 *    -  sorting  -- a modified version of a tag that should be used for
 *       sorting will have '-sort' appended. E.g. artist="The Beatles",
 *       artist-sort="Beatles, The".
 * - Some protocols and demuxers support metadata updates. After a successful
 *   call to av_read_frame(), AVFormatContext.event_flags or AVStream.event_flags
 *   will be updated to indicate if metadata changed. In order to detect metadata
 *   changes on a stream, you need to loop through all streams in the AVFormatContext
 *   and check their individual event_flags.
 *
 * -  Demuxers attempt to export metadata in a generic format, however tags
 *    with no generic equivalents are left as they are stored in the container.
 *    Follows a list of generic tag names:
 *
 @verbatim
 album        -- name of the set this work belongs to
 album_artist -- main creator of the set/album, if different from artist.
                 e.g. "Various Artists" for compilation albums.
 artist       -- main creator of the work
 comment      -- any additional description of the file.
 composer     -- who composed the work, if different from artist.
 copyright    -- name of copyright holder.
 creation_time-- date when the file was created, preferably in ISO 8601.
 date         -- date when the work was created, preferably in ISO 8601.
 disc         -- number of a subset, e.g. disc in a multi-disc collection.
 encoder      -- name/settings of the software/hardware that produced the file.
 encoded_by   -- person/group who created the file.
 filename     -- original name of the file.
 genre        -- <self-evident>.
 language     -- main language in which the work is performed, preferably
                 in ISO 639-2 format. Multiple languages can be specified by
                 separating them with commas.
 performer    -- artist who performed the work, if different from artist.
                 E.g for "Also sprach Zarathustra", artist would be "Richard
                 Strauss" and performer "London Philharmonic Orchestra".
 publisher    -- name of the label/publisher.
 service_name     -- name of the service in broadcasting (channel name).
 service_provider -- name of the service provider in broadcasting.
 title        -- name of the work.
 track        -- number of this work in the set, can be in form current/total.
 variant_bitrate -- the total bitrate of the bitrate variant that the current stream is part of
 @endverbatim
 *
 * Look in the examples section for an application example how to use the Metadata API.
 *
 * @}
 */

/* packet functions */

/**
 * Allocate and read the payload of a packet and initialize its
 * fields with default values.
 *
 * @param s    associated IO context
 * @param pkt packet
 * @param size desired payload size
 * @return >0 (read size) if OK, AVERROR_xxx otherwise
 */
int av_get_packet(AVIOContext *s, AVPacket *pkt, int size);

int av_append_packet(AVIOContext *s, AVPacket *pkt, int size);

struct AVCodecTag;

typedef struct AVProbeData {
    const char *filename;
    unsigned char *buf;
    int buf_size;
    const char *mime_type;
} AVProbeData;

#define AVPROBE_SCORE_RETRY (AVPROBE_SCORE_MAX/4)
#define AVPROBE_SCORE_STREAM_RETRY (AVPROBE_SCORE_MAX/4-1)

#define AVPROBE_SCORE_EXTENSION  50 ///< score for file extension
#define AVPROBE_SCORE_MIME_BONUS 30 ///< score added for matching mime type
#define AVPROBE_SCORE_MAX       100 ///< maximum score

#define AVPROBE_PADDING_SIZE 32             ///< extra allocated bytes at the end of the probe buffer

#define AVFMT_NOFILE        0x0001
#define AVFMT_NEEDNUMBER    0x0002

#define AVFMT_EXPERIMENTAL  0x0004
#define AVFMT_SHOW_IDS      0x0008
#define AVFMT_GLOBALHEADER  0x0040
#define AVFMT_NOTIMESTAMPS  0x0080
#define AVFMT_GENERIC_INDEX 0x0100
#define AVFMT_TS_DISCONT    0x0200
#define AVFMT_VARIABLE_FPS  0x0400
#define AVFMT_NODIMENSIONS  0x0800
#define AVFMT_NOSTREAMS     0x1000
#define AVFMT_NOBINSEARCH   0x2000
#define AVFMT_NOGENSEARCH   0x4000
#define AVFMT_NO_BYTE_SEEK  0x8000
#define AVFMT_TS_NONSTRICT 0x20000

#define AVFMT_TS_NEGATIVE  0x40000

#define AVFMT_SEEK_TO_PTS   0x4000000

typedef struct AVOutputFormat {
    const char *name;

    const char *long_name;
    const char *mime_type;
    const char *extensions;

    enum AVCodecID audio_codec;
    enum AVCodecID video_codec;
    enum AVCodecID subtitle_codec;

    int flags;

    const struct AVCodecTag * const *codec_tag;

    const AVClass *priv_class;
} AVOutputFormat;

typedef struct AVInputFormat {

    const char *name;

    const char *long_name;

    int flags;

    const char *extensions;

    const struct AVCodecTag * const *codec_tag;

    const AVClass *priv_class;

    const char *mime_type;
} AVInputFormat;

enum AVStreamParseType {
    AVSTREAM_PARSE_NONE,
    AVSTREAM_PARSE_FULL,
    AVSTREAM_PARSE_HEADERS,
    AVSTREAM_PARSE_TIMESTAMPS,
    AVSTREAM_PARSE_FULL_ONCE,
    AVSTREAM_PARSE_FULL_RAW,

};

typedef struct AVIndexEntry {
    int64_t pos;
    int64_t timestamp;

#define AVINDEX_KEYFRAME 0x0001
#define AVINDEX_DISCARD_FRAME  0x0002

    int flags:2;
    int size:30;
    int min_distance;
} AVIndexEntry;

#define AV_DISPOSITION_DEFAULT              (1 << 0)

#define AV_DISPOSITION_DUB                  (1 << 1)

#define AV_DISPOSITION_ORIGINAL             (1 << 2)

#define AV_DISPOSITION_COMMENT              (1 << 3)

#define AV_DISPOSITION_LYRICS               (1 << 4)

#define AV_DISPOSITION_KARAOKE              (1 << 5)

#define AV_DISPOSITION_FORCED               (1 << 6)

#define AV_DISPOSITION_HEARING_IMPAIRED     (1 << 7)

#define AV_DISPOSITION_VISUAL_IMPAIRED      (1 << 8)

#define AV_DISPOSITION_CLEAN_EFFECTS        (1 << 9)

#define AV_DISPOSITION_ATTACHED_PIC         (1 << 10)

#define AV_DISPOSITION_TIMED_THUMBNAILS     (1 << 11)

#define AV_DISPOSITION_NON_DIEGETIC         (1 << 12)

#define AV_DISPOSITION_CAPTIONS             (1 << 16)

#define AV_DISPOSITION_DESCRIPTIONS         (1 << 17)

#define AV_DISPOSITION_METADATA             (1 << 18)

#define AV_DISPOSITION_DEPENDENT            (1 << 19)

#define AV_DISPOSITION_STILL_IMAGE          (1 << 20)

#define AV_DISPOSITION_MULTILAYER           (1 << 21)

int av_disposition_from_string(const char *disp);

const char *av_disposition_to_string(int disposition);

#define AV_PTS_WRAP_IGNORE      0   ///< ignore the wrap
#define AV_PTS_WRAP_ADD_OFFSET  1   ///< add the format specific offset on wrap detection
#define AV_PTS_WRAP_SUB_OFFSET  -1  ///< subtract the format specific offset on wrap detection

typedef struct AVStream {

    const AVClass *av_class;

    int index;

    int id;

    AVCodecParameters *codecpar;

    void *priv_data;

    AVRational time_base;

    int64_t start_time;

    int64_t duration;

    int64_t nb_frames;

    int disposition;

    enum AVDiscard discard;

    AVRational sample_aspect_ratio;

    AVDictionary *metadata;

    AVRational avg_frame_rate;

    AVPacket attached_pic;

    int event_flags;

#define AVSTREAM_EVENT_FLAG_METADATA_UPDATED 0x0001

#define AVSTREAM_EVENT_FLAG_NEW_PACKETS (1 << 1)

    AVRational r_frame_rate;

    int pts_wrap_bits;
} AVStream;

typedef struct AVStreamGroupTileGrid {
    const AVClass *av_class;

    unsigned int nb_tiles;

    int coded_width;

    int coded_height;

    struct {

        unsigned int idx;

        int horizontal;

        int vertical;
    } *offsets;

    uint8_t background[4];

    int horizontal_offset;

    int vertical_offset;

    int width;

    int height;

    AVPacketSideData *coded_side_data;

    int nb_coded_side_data;
} AVStreamGroupTileGrid;

typedef struct AVStreamGroupLCEVC {
    const AVClass *av_class;

    unsigned int lcevc_index;

    int width;

    int height;
} AVStreamGroupLCEVC;

enum AVStreamGroupParamsType {
    AV_STREAM_GROUP_PARAMS_NONE,
    AV_STREAM_GROUP_PARAMS_IAMF_AUDIO_ELEMENT,
    AV_STREAM_GROUP_PARAMS_IAMF_MIX_PRESENTATION,
    AV_STREAM_GROUP_PARAMS_TILE_GRID,
    AV_STREAM_GROUP_PARAMS_LCEVC,
};

struct AVIAMFAudioElement;
struct AVIAMFMixPresentation;

typedef struct AVStreamGroup {

    const AVClass *av_class;

    void *priv_data;

    unsigned int index;

    int64_t id;

    enum AVStreamGroupParamsType type;

    union {
        struct AVIAMFAudioElement *iamf_audio_element;
        struct AVIAMFMixPresentation *iamf_mix_presentation;
        struct AVStreamGroupTileGrid *tile_grid;
        struct AVStreamGroupLCEVC *lcevc;
    } params;

    AVDictionary *metadata;

    unsigned int nb_streams;

    AVStream **streams;

    int disposition;
} AVStreamGroup;

struct AVCodecParserContext *av_stream_get_parser(const AVStream *s);

#define AV_PROGRAM_RUNNING 1

typedef struct AVProgram {
    int            id;
    int            flags;
    enum AVDiscard discard;
    unsigned int   *stream_index;
    unsigned int   nb_stream_indexes;
    AVDictionary *metadata;

    int program_num;
    int pmt_pid;
    int pcr_pid;
    int pmt_version;

    int64_t start_time;
    int64_t end_time;

    int64_t pts_wrap_reference;
    int pts_wrap_behavior;
} AVProgram;

#define AVFMTCTX_NOHEADER      0x0001

#define AVFMTCTX_UNSEEKABLE    0x0002

typedef struct AVChapter {
    int64_t id;
    AVRational time_base;
    int64_t start, end;
    AVDictionary *metadata;
} AVChapter;

typedef int (*av_format_control_message)(struct AVFormatContext *s, int type,
                                         void *data, size_t data_size);

typedef int (*AVOpenCallback)(struct AVFormatContext *s, AVIOContext **pb, const char *url, int flags,
                              const AVIOInterruptCB *int_cb, AVDictionary **options);

enum AVDurationEstimationMethod {
    AVFMT_DURATION_FROM_PTS,
    AVFMT_DURATION_FROM_STREAM,
    AVFMT_DURATION_FROM_BITRATE
};

typedef struct AVFormatContext {

    const AVClass *av_class;

    const struct AVInputFormat *iformat;

    const struct AVOutputFormat *oformat;

    void *priv_data;

    AVIOContext *pb;

    int ctx_flags;

    unsigned int nb_streams;

    AVStream **streams;

    unsigned int nb_stream_groups;

    AVStreamGroup **stream_groups;

    unsigned int nb_chapters;
    AVChapter **chapters;

    char *url;

    int64_t start_time;

    int64_t duration;

    int64_t bit_rate;

    unsigned int packet_size;
    int max_delay;

    int flags;
#define AVFMT_FLAG_GENPTS       0x0001 ///< Generate missing pts even if it requires parsing future frames.
#define AVFMT_FLAG_IGNIDX       0x0002 ///< Ignore index.
#define AVFMT_FLAG_NONBLOCK     0x0004 ///< Do not block when reading packets from input.
#define AVFMT_FLAG_IGNDTS       0x0008 ///< Ignore DTS on frames that contain both DTS & PTS
#define AVFMT_FLAG_NOFILLIN     0x0010 ///< Do not infer any values from other values, just return what is stored in the container
#define AVFMT_FLAG_NOPARSE      0x0020 ///< Do not use AVParsers, you also must set AVFMT_FLAG_NOFILLIN as the filling code works on frames and no parsing -> no frames. Also seeking to frames can not work if parsing to find frame boundaries has been disabled
#define AVFMT_FLAG_NOBUFFER     0x0040 ///< Do not buffer frames when possible
#define AVFMT_FLAG_CUSTOM_IO    0x0080 ///< The caller has supplied a custom AVIOContext, don't avio_close() it.
#define AVFMT_FLAG_DISCARD_CORRUPT  0x0100 ///< Discard frames marked corrupted
#define AVFMT_FLAG_FLUSH_PACKETS    0x0200 ///< Flush the AVIOContext every packet.

#define AVFMT_FLAG_BITEXACT         0x0400
#define AVFMT_FLAG_SORT_DTS    0x10000 ///< try to interleave outputted packets by dts (using this flag can slow demuxing down)
#define AVFMT_FLAG_FAST_SEEK   0x80000 ///< Enable fast, but inaccurate seeks for some formats
#define AVFMT_FLAG_AUTO_BSF   0x200000 ///< Add bitstream filters as requested by the muxer

    int64_t probesize;

    int64_t max_analyze_duration;

    const uint8_t *key;
    int keylen;

    unsigned int nb_programs;
    AVProgram **programs;

    enum AVCodecID video_codec_id;

    enum AVCodecID audio_codec_id;

    enum AVCodecID subtitle_codec_id;

    enum AVCodecID data_codec_id;

    AVDictionary *metadata;

    int64_t start_time_realtime;

    int fps_probe_size;

    int error_recognition;

    AVIOInterruptCB interrupt_callback;

    int debug;
#define FF_FDEBUG_TS        0x0001

    int max_streams;

    unsigned int max_index_size;

    unsigned int max_picture_buffer;

    int64_t max_interleave_delta;

    int max_ts_probe;

    int max_chunk_duration;

    int max_chunk_size;

    int max_probe_packets;

    int strict_std_compliance;

    int event_flags;

#define AVFMT_EVENT_FLAG_METADATA_UPDATED 0x0001

    int avoid_negative_ts;
#define AVFMT_AVOID_NEG_TS_AUTO             -1 ///< Enabled when required by target format
#define AVFMT_AVOID_NEG_TS_DISABLED          0 ///< Do not shift timestamps even when they are negative.
#define AVFMT_AVOID_NEG_TS_MAKE_NON_NEGATIVE 1 ///< Shift timestamps so they are non negative
#define AVFMT_AVOID_NEG_TS_MAKE_ZERO         2 ///< Shift timestamps so that they start at 0

    int audio_preload;

    int use_wallclock_as_timestamps;

    int skip_estimate_duration_from_pts;

    int avio_flags;

    enum AVDurationEstimationMethod duration_estimation_method;

    int64_t skip_initial_bytes;

    unsigned int correct_ts_overflow;

    int seek2any;

    int flush_packets;

    int probe_score;

    int format_probesize;

    char *codec_whitelist;

    char *format_whitelist;

    char *protocol_whitelist;

    char *protocol_blacklist;

    int io_repositioned;

    const struct AVCodec *video_codec;

    const struct AVCodec *audio_codec;

    const struct AVCodec *subtitle_codec;

    const struct AVCodec *data_codec;

    int metadata_header_padding;

    void *opaque;

    av_format_control_message control_message_cb;

    int64_t output_ts_offset;

    uint8_t *dump_separator;

    int (*io_open)(struct AVFormatContext *s, AVIOContext **pb, const char *url,
                   int flags, AVDictionary **options);

    int (*io_close2)(struct AVFormatContext *s, AVIOContext *pb);

    int64_t duration_probesize;

    char *name;
} AVFormatContext;

unsigned avformat_version(void);

const char *avformat_configuration(void);

const char *avformat_license(void);

int avformat_network_init(void);

int avformat_network_deinit(void);

const AVOutputFormat *av_muxer_iterate(void **opaque);

const AVInputFormat *av_demuxer_iterate(void **opaque);

AVFormatContext *avformat_alloc_context(void);

void avformat_free_context(AVFormatContext *s);

const AVClass *avformat_get_class(void);

const AVClass *av_stream_get_class(void);

const AVClass *av_stream_group_get_class(void);

const char *avformat_stream_group_name(enum AVStreamGroupParamsType type);

AVStreamGroup *avformat_stream_group_create(AVFormatContext *s,
                                            enum AVStreamGroupParamsType type,
                                            AVDictionary **options);

AVStream *avformat_new_stream(AVFormatContext *s, const struct AVCodec *c);

int avformat_stream_group_add_stream(AVStreamGroup *stg, AVStream *st);

AVProgram *av_new_program(AVFormatContext *s, int id);

int avformat_alloc_output_context2(AVFormatContext **ctx, const AVOutputFormat *oformat,
                                   const char *format_name, const char *filename);

const AVInputFormat *av_find_input_format(const char *short_name);

const AVInputFormat *av_probe_input_format(const AVProbeData *pd, int is_opened);

const AVInputFormat *av_probe_input_format2(const AVProbeData *pd,
                                            int is_opened, int *score_max);

const AVInputFormat *av_probe_input_format3(const AVProbeData *pd,
                                            int is_opened, int *score_ret);

int av_probe_input_buffer2(AVIOContext *pb, const AVInputFormat **fmt,
                           const char *url, void *logctx,
                           unsigned int offset, unsigned int max_probe_size);

int av_probe_input_buffer(AVIOContext *pb, const AVInputFormat **fmt,
                          const char *url, void *logctx,
                          unsigned int offset, unsigned int max_probe_size);

int avformat_open_input(AVFormatContext **ps, const char *url,
                        const AVInputFormat *fmt, AVDictionary **options);

int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);

AVProgram *av_find_program_from_stream(AVFormatContext *ic, AVProgram *last, int s);

void av_program_add_stream_index(AVFormatContext *ac, int progid, unsigned int idx);

int av_find_best_stream(AVFormatContext *ic,
                        enum AVMediaType type,
                        int wanted_stream_nb,
                        int related_stream,
                        const struct AVCodec **decoder_ret,
                        int flags);

int av_read_frame(AVFormatContext *s, AVPacket *pkt);

int av_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp,
                  int flags);

int avformat_seek_file(AVFormatContext *s, int stream_index, int64_t min_ts, int64_t ts, int64_t max_ts, int flags);

int avformat_flush(AVFormatContext *s);

int av_read_play(AVFormatContext *s);

int av_read_pause(AVFormatContext *s);

enum AVFormatCommandID {

    AVFORMAT_COMMAND_RTSP_SET_PARAMETER,
};

typedef struct AVRTSPCommandRequest {

    AVDictionary *headers;

    size_t body_len;

    char *body;
} AVRTSPCommandRequest;

typedef struct AVRTSPResponse {

    int status_code;

    char *reason;

    size_t body_len;

    unsigned char *body;
} AVRTSPResponse;

int avformat_send_command(AVFormatContext *s, enum AVFormatCommandID id, void *data);

int avformat_receive_command_reply(AVFormatContext *s, enum AVFormatCommandID id, void **data_out);

void avformat_close_input(AVFormatContext **s);

#define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
#define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
#define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes
#define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number

#define AVSTREAM_INIT_IN_WRITE_HEADER 0 ///< stream parameters initialized in avformat_write_header
#define AVSTREAM_INIT_IN_INIT_OUTPUT  1 ///< stream parameters initialized in avformat_init_output

av_warn_unused_result
int avformat_write_header(AVFormatContext *s, AVDictionary **options);

av_warn_unused_result
int avformat_init_output(AVFormatContext *s, AVDictionary **options);

int av_write_frame(AVFormatContext *s, AVPacket *pkt);

int av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt);

int av_write_uncoded_frame(AVFormatContext *s, int stream_index,
                           struct AVFrame *frame);

int av_interleaved_write_uncoded_frame(AVFormatContext *s, int stream_index,
                                       struct AVFrame *frame);

int av_write_uncoded_frame_query(AVFormatContext *s, int stream_index);

int av_write_trailer(AVFormatContext *s);

const AVOutputFormat *av_guess_format(const char *short_name,
                                      const char *filename,
                                      const char *mime_type);

enum AVCodecID av_guess_codec(const AVOutputFormat *fmt, const char *short_name,
                              const char *filename, const char *mime_type,
                              enum AVMediaType type);

int av_get_output_timestamp(struct AVFormatContext *s, int stream,
                            int64_t *dts, int64_t *wall);

void av_hex_dump(FILE *f, const uint8_t *buf, int size);

void av_hex_dump_log(void *avcl, int level, const uint8_t *buf, int size);

void av_pkt_dump2(FILE *f, const AVPacket *pkt, int dump_payload, const AVStream *st);

void av_pkt_dump_log2(void *avcl, int level, const AVPacket *pkt, int dump_payload,
                      const AVStream *st);

enum AVCodecID av_codec_get_id(const struct AVCodecTag * const *tags, unsigned int tag);

unsigned int av_codec_get_tag(const struct AVCodecTag * const *tags, enum AVCodecID id);

int av_codec_get_tag2(const struct AVCodecTag * const *tags, enum AVCodecID id,
                      unsigned int *tag);

int av_find_default_stream_index(AVFormatContext *s);

int av_index_search_timestamp(AVStream *st, int64_t timestamp, int flags);

int avformat_index_get_entries_count(const AVStream *st);

const AVIndexEntry *avformat_index_get_entry(AVStream *st, int idx);

const AVIndexEntry *avformat_index_get_entry_from_timestamp(AVStream *st,
                                                            int64_t wanted_timestamp,
                                                            int flags);

int av_add_index_entry(AVStream *st, int64_t pos, int64_t timestamp,
                       int size, int distance, int flags);

void av_url_split(char *proto,         int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname,      int hostname_size,
                  int *port_ptr,
                  char *path,          int path_size,
                  const char *url);

void av_dump_format(AVFormatContext *ic,
                    int index,
                    const char *url,
                    int is_output);

#define AV_FRAME_FILENAME_FLAGS_MULTIPLE          1  ///< Allow multiple %d
#define AV_FRAME_FILENAME_FLAGS_IGNORE_TRUNCATION 2  ///< Ignore truncated output instead of returning an error

int av_get_frame_filename2(char *buf, int buf_size,
                          const char *path, int number, int flags);

int av_get_frame_filename(char *buf, int buf_size,
                          const char *path, int number);

int av_filename_number_test(const char *filename);

int av_sdp_create(AVFormatContext *ac[], int n_files, char *buf, int size);

int av_match_ext(const char *filename, const char *extensions);

int avformat_query_codec(const AVOutputFormat *ofmt, enum AVCodecID codec_id,
                         int std_compliance);

struct AVBPrint;

int av_mime_codec_str(const AVCodecParameters *par,
                      AVRational frame_rate, struct AVBPrint *out);

const struct AVCodecTag *avformat_get_riff_video_tags(void);

const struct AVCodecTag *avformat_get_riff_audio_tags(void);

const struct AVCodecTag *avformat_get_mov_video_tags(void);

const struct AVCodecTag *avformat_get_mov_audio_tags(void);

AVRational av_guess_sample_aspect_ratio(AVFormatContext *format, AVStream *stream,
                                        struct AVFrame *frame);

AVRational av_guess_frame_rate(AVFormatContext *ctx, AVStream *stream,
                               struct AVFrame *frame);

int avformat_match_stream_specifier(AVFormatContext *s, AVStream *st,
                                    const char *spec);

int avformat_queue_attached_pictures(AVFormatContext *s);

#if FF_API_INTERNAL_TIMING
enum AVTimebaseSource {
    AVFMT_TBCF_AUTO = -1,
    AVFMT_TBCF_DECODER,
    AVFMT_TBCF_DEMUXER,
#if FF_API_R_FRAME_RATE
    AVFMT_TBCF_R_FRAMERATE,
#endif
};

attribute_deprecated
int avformat_transfer_internal_stream_timing_info(const AVOutputFormat *ofmt,
                                                  AVStream *ost, const AVStream *ist,
                                                  enum AVTimebaseSource copy_tb);

attribute_deprecated
AVRational av_stream_get_codec_timebase(const AVStream *st);
#endif

#endif
