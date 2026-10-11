#ifndef AVCODEC_PACKET_H
#define AVCODEC_PACKET_H

#include <stddef.h>
#include <stdint.h>

#include "libavutil/attributes.h"
#include "libavutil/buffer.h"
#include "libavutil/dict.h"
#include "libavutil/rational.h"
#include "libavutil/version.h"

#include "libavcodec/version_major.h"

enum AVPacketSideDataType {

    AV_PKT_DATA_PALETTE,

    AV_PKT_DATA_NEW_EXTRADATA,

    AV_PKT_DATA_PARAM_CHANGE,

    AV_PKT_DATA_H263_MB_INFO,

    AV_PKT_DATA_REPLAYGAIN,

    AV_PKT_DATA_DISPLAYMATRIX,

    AV_PKT_DATA_STEREO3D,

    AV_PKT_DATA_AUDIO_SERVICE_TYPE,

    AV_PKT_DATA_QUALITY_STATS,

    AV_PKT_DATA_FALLBACK_TRACK,

    AV_PKT_DATA_CPB_PROPERTIES,

    AV_PKT_DATA_SKIP_SAMPLES,

    AV_PKT_DATA_JP_DUALMONO,

    AV_PKT_DATA_STRINGS_METADATA,

    AV_PKT_DATA_SUBTITLE_POSITION,

    AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL,

    AV_PKT_DATA_WEBVTT_IDENTIFIER,

    AV_PKT_DATA_WEBVTT_SETTINGS,

    AV_PKT_DATA_METADATA_UPDATE,

    AV_PKT_DATA_MPEGTS_STREAM_ID,

    AV_PKT_DATA_MASTERING_DISPLAY_METADATA,

    AV_PKT_DATA_SPHERICAL,

    AV_PKT_DATA_CONTENT_LIGHT_LEVEL,

    AV_PKT_DATA_A53_CC,

    AV_PKT_DATA_ENCRYPTION_INIT_INFO,

    AV_PKT_DATA_ENCRYPTION_INFO,

    AV_PKT_DATA_AFD,

    AV_PKT_DATA_PRFT,

    AV_PKT_DATA_ICC_PROFILE,

    AV_PKT_DATA_DOVI_CONF,

    AV_PKT_DATA_S12M_TIMECODE,

    AV_PKT_DATA_DYNAMIC_HDR10_PLUS,

    AV_PKT_DATA_IAMF_MIX_GAIN_PARAM,

    AV_PKT_DATA_IAMF_DEMIXING_INFO_PARAM,

    AV_PKT_DATA_IAMF_RECON_GAIN_INFO_PARAM,

    AV_PKT_DATA_AMBIENT_VIEWING_ENVIRONMENT,

    AV_PKT_DATA_FRAME_CROPPING,

    AV_PKT_DATA_LCEVC,

    AV_PKT_DATA_3D_REFERENCE_DISPLAYS,

    AV_PKT_DATA_RTCP_SR,

     AV_PKT_DATA_EXIF,

    AV_PKT_DATA_NB
};

typedef struct AVPacketSideData {
    uint8_t *data;
    size_t   size;
    enum AVPacketSideDataType type;
} AVPacketSideData;

AVPacketSideData *av_packet_side_data_new(AVPacketSideData **psd, int *pnb_sd,
                                          enum AVPacketSideDataType type,
                                          size_t size, int flags);

AVPacketSideData *av_packet_side_data_add(AVPacketSideData **sd, int *nb_sd,
                                          enum AVPacketSideDataType type,
                                          void *data, size_t size, int flags);

const AVPacketSideData *av_packet_side_data_get(const AVPacketSideData *sd,
                                                int nb_sd,
                                                enum AVPacketSideDataType type);

void av_packet_side_data_remove(AVPacketSideData *sd, int *nb_sd,
                                enum AVPacketSideDataType type);

void av_packet_side_data_free(AVPacketSideData **sd, int *nb_sd);

struct AVFrameSideData;

int av_packet_side_data_from_frame(AVPacketSideData **sd, int *nb_sd,
                                   const struct AVFrameSideData *src, unsigned int flags);

int av_packet_side_data_to_frame(struct AVFrameSideData ***sd, int *nb_sd,
                                 const AVPacketSideData *src, unsigned int flags);

const char *av_packet_side_data_name(enum AVPacketSideDataType type);

typedef struct AVPacket {

    AVBufferRef *buf;

    int64_t pts;

    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;

    int   flags;

    AVPacketSideData *side_data;
    int side_data_elems;

    int64_t duration;

    int64_t pos;

    void *opaque;

    AVBufferRef *opaque_ref;

    AVRational time_base;
} AVPacket;

#if FF_API_INIT_PACKET
attribute_deprecated
typedef struct AVPacketList {
    AVPacket pkt;
    struct AVPacketList *next;
} AVPacketList;
#endif

#define AV_PKT_FLAG_KEY     0x0001 ///< The packet contains a keyframe
#define AV_PKT_FLAG_CORRUPT 0x0002 ///< The packet content is corrupted

#define AV_PKT_FLAG_DISCARD   0x0004

#define AV_PKT_FLAG_TRUSTED   0x0008

#define AV_PKT_FLAG_DISPOSABLE 0x0010

enum AVSideDataParamChangeFlags {
    AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE    = 0x0004,
    AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS     = 0x0008,
};

AVPacket *av_packet_alloc(void);

AVPacket *av_packet_clone(const AVPacket *src);

void av_packet_free(AVPacket **pkt);

#if FF_API_INIT_PACKET

attribute_deprecated
void av_init_packet(AVPacket *pkt);
#endif

int av_new_packet(AVPacket *pkt, int size);

void av_shrink_packet(AVPacket *pkt, int size);

int av_grow_packet(AVPacket *pkt, int grow_by);

int av_packet_from_data(AVPacket *pkt, uint8_t *data, int size);

uint8_t* av_packet_new_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                                 size_t size);

int av_packet_add_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                            uint8_t *data, size_t size);

int av_packet_shrink_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                               size_t size);

uint8_t* av_packet_get_side_data(const AVPacket *pkt, enum AVPacketSideDataType type,
                                 size_t *size);

uint8_t *av_packet_pack_dictionary(AVDictionary *dict, size_t *size);

int av_packet_unpack_dictionary(const uint8_t *data, size_t size,
                                AVDictionary **dict);

void av_packet_free_side_data(AVPacket *pkt);

int av_packet_ref(AVPacket *dst, const AVPacket *src);

void av_packet_unref(AVPacket *pkt);

void av_packet_move_ref(AVPacket *dst, AVPacket *src);

int av_packet_copy_props(AVPacket *dst, const AVPacket *src);

int av_packet_make_refcounted(AVPacket *pkt);

int av_packet_make_writable(AVPacket *pkt);

void av_packet_rescale_ts(AVPacket *pkt, AVRational tb_src, AVRational tb_dst);

struct AVContainerFifo *av_container_fifo_alloc_avpacket(unsigned flags);

#endif
