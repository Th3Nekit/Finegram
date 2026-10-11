#ifndef AVCODEC_DEFS_H
#define AVCODEC_DEFS_H

#include <stdint.h>
#include <stdlib.h>

#define AV_INPUT_BUFFER_PADDING_SIZE 64

#define AV_EF_CRCCHECK       (1<<0)
#define AV_EF_BITSTREAM      (1<<1)   ///< detect bitstream specification deviations
#define AV_EF_BUFFER         (1<<2)   ///< detect improper bitstream length
#define AV_EF_EXPLODE        (1<<3)   ///< abort decoding on minor error detection

#define AV_EF_IGNORE_ERR     (1<<15)  ///< ignore errors and continue
#define AV_EF_CAREFUL        (1<<16)  ///< consider things that violate the spec, are fast to calculate and have not been seen in the wild as errors
#define AV_EF_COMPLIANT      (1<<17)  ///< consider all spec non compliances as errors
#define AV_EF_AGGRESSIVE     (1<<18)  ///< consider things that a sane encoder/muxer should not do as an error

#define FF_COMPLIANCE_VERY_STRICT   2 ///< Strictly conform to an older more strict version of the spec or reference software.
#define FF_COMPLIANCE_STRICT        1 ///< Strictly conform to all the things in the spec no matter what consequences.
#define FF_COMPLIANCE_NORMAL        0
#define FF_COMPLIANCE_UNOFFICIAL   -1 ///< Allow unofficial extensions
#define FF_COMPLIANCE_EXPERIMENTAL -2 ///< Allow nonstandardized experimental things.

#define AV_PROFILE_UNKNOWN        -99
#define AV_PROFILE_RESERVED      -100

#define AV_PROFILE_AAC_MAIN         0
#define AV_PROFILE_AAC_LOW          1
#define AV_PROFILE_AAC_SSR          2
#define AV_PROFILE_AAC_LTP          3
#define AV_PROFILE_AAC_HE           4
#define AV_PROFILE_AAC_HE_V2       28
#define AV_PROFILE_AAC_LD          22
#define AV_PROFILE_AAC_ELD         38
#define AV_PROFILE_AAC_USAC        41
#define AV_PROFILE_MPEG2_AAC_LOW  128
#define AV_PROFILE_MPEG2_AAC_HE   131

#define AV_PROFILE_DNXHD         0
#define AV_PROFILE_DNXHR_LB      1
#define AV_PROFILE_DNXHR_SQ      2
#define AV_PROFILE_DNXHR_HQ      3
#define AV_PROFILE_DNXHR_HQX     4
#define AV_PROFILE_DNXHR_444     5

#define AV_PROFILE_DTS                20
#define AV_PROFILE_DTS_ES             30
#define AV_PROFILE_DTS_96_24          40
#define AV_PROFILE_DTS_HD_HRA         50
#define AV_PROFILE_DTS_HD_MA          60
#define AV_PROFILE_DTS_EXPRESS        70
#define AV_PROFILE_DTS_HD_MA_X        61
#define AV_PROFILE_DTS_HD_MA_X_IMAX   62

#define AV_PROFILE_EAC3_DDP_ATMOS         30

#define AV_PROFILE_TRUEHD_ATMOS           30

#define AV_PROFILE_MPEG2_422           0
#define AV_PROFILE_MPEG2_HIGH          1
#define AV_PROFILE_MPEG2_SS            2
#define AV_PROFILE_MPEG2_SNR_SCALABLE  3
#define AV_PROFILE_MPEG2_MAIN          4
#define AV_PROFILE_MPEG2_SIMPLE        5

#define AV_PROFILE_H264_CONSTRAINED  (1<<9)  // 8+1; constraint_set1_flag
#define AV_PROFILE_H264_INTRA        (1<<11) // 8+3; constraint_set3_flag

#define AV_PROFILE_H264_BASELINE             66
#define AV_PROFILE_H264_CONSTRAINED_BASELINE (66|AV_PROFILE_H264_CONSTRAINED)
#define AV_PROFILE_H264_MAIN                 77
#define AV_PROFILE_H264_EXTENDED             88
#define AV_PROFILE_H264_HIGH                 100
#define AV_PROFILE_H264_HIGH_10              110
#define AV_PROFILE_H264_HIGH_10_INTRA        (110|AV_PROFILE_H264_INTRA)
#define AV_PROFILE_H264_MULTIVIEW_HIGH       118
#define AV_PROFILE_H264_HIGH_422             122
#define AV_PROFILE_H264_HIGH_422_INTRA       (122|AV_PROFILE_H264_INTRA)
#define AV_PROFILE_H264_STEREO_HIGH          128
#define AV_PROFILE_H264_HIGH_444             144
#define AV_PROFILE_H264_HIGH_444_PREDICTIVE  244
#define AV_PROFILE_H264_HIGH_444_INTRA       (244|AV_PROFILE_H264_INTRA)
#define AV_PROFILE_H264_CAVLC_444            44

#define AV_PROFILE_VC1_SIMPLE   0
#define AV_PROFILE_VC1_MAIN     1
#define AV_PROFILE_VC1_COMPLEX  2
#define AV_PROFILE_VC1_ADVANCED 3

#define AV_PROFILE_MPEG4_SIMPLE                     0
#define AV_PROFILE_MPEG4_SIMPLE_SCALABLE            1
#define AV_PROFILE_MPEG4_CORE                       2
#define AV_PROFILE_MPEG4_MAIN                       3
#define AV_PROFILE_MPEG4_N_BIT                      4
#define AV_PROFILE_MPEG4_SCALABLE_TEXTURE           5
#define AV_PROFILE_MPEG4_SIMPLE_FACE_ANIMATION      6
#define AV_PROFILE_MPEG4_BASIC_ANIMATED_TEXTURE     7
#define AV_PROFILE_MPEG4_HYBRID                     8
#define AV_PROFILE_MPEG4_ADVANCED_REAL_TIME         9
#define AV_PROFILE_MPEG4_CORE_SCALABLE             10
#define AV_PROFILE_MPEG4_ADVANCED_CODING           11
#define AV_PROFILE_MPEG4_ADVANCED_CORE             12
#define AV_PROFILE_MPEG4_ADVANCED_SCALABLE_TEXTURE 13
#define AV_PROFILE_MPEG4_SIMPLE_STUDIO             14
#define AV_PROFILE_MPEG4_ADVANCED_SIMPLE           15

#define AV_PROFILE_JPEG2000_CSTREAM_RESTRICTION_0   1
#define AV_PROFILE_JPEG2000_CSTREAM_RESTRICTION_1   2
#define AV_PROFILE_JPEG2000_CSTREAM_NO_RESTRICTION  32768
#define AV_PROFILE_JPEG2000_DCINEMA_2K              3
#define AV_PROFILE_JPEG2000_DCINEMA_4K              4

#define AV_PROFILE_VP9_0                            0
#define AV_PROFILE_VP9_1                            1
#define AV_PROFILE_VP9_2                            2
#define AV_PROFILE_VP9_3                            3

#define AV_PROFILE_HEVC_MAIN                        1
#define AV_PROFILE_HEVC_MAIN_10                     2
#define AV_PROFILE_HEVC_MAIN_STILL_PICTURE          3
#define AV_PROFILE_HEVC_REXT                        4
#define AV_PROFILE_HEVC_MULTIVIEW_MAIN              6
#define AV_PROFILE_HEVC_SCC                         9

#define AV_PROFILE_VVC_MAIN_10                      1
#define AV_PROFILE_VVC_MAIN_10_444                 33

#define AV_PROFILE_AV1_MAIN                         0
#define AV_PROFILE_AV1_HIGH                         1
#define AV_PROFILE_AV1_PROFESSIONAL                 2

#define AV_PROFILE_MJPEG_HUFFMAN_BASELINE_DCT            0xc0
#define AV_PROFILE_MJPEG_HUFFMAN_EXTENDED_SEQUENTIAL_DCT 0xc1
#define AV_PROFILE_MJPEG_HUFFMAN_PROGRESSIVE_DCT         0xc2
#define AV_PROFILE_MJPEG_HUFFMAN_LOSSLESS                0xc3
#define AV_PROFILE_MJPEG_JPEG_LS                         0xf7

#define AV_PROFILE_SBC_MSBC                         1

#define AV_PROFILE_PRORES_PROXY     0
#define AV_PROFILE_PRORES_LT        1
#define AV_PROFILE_PRORES_STANDARD  2
#define AV_PROFILE_PRORES_HQ        3
#define AV_PROFILE_PRORES_4444      4
#define AV_PROFILE_PRORES_XQ        5

#define AV_PROFILE_PRORES_RAW       0
#define AV_PROFILE_PRORES_RAW_HQ    1

#define AV_PROFILE_ARIB_PROFILE_A 0
#define AV_PROFILE_ARIB_PROFILE_C 1

#define AV_PROFILE_KLVA_SYNC  0
#define AV_PROFILE_KLVA_ASYNC 1

#define AV_PROFILE_EVC_BASELINE             0
#define AV_PROFILE_EVC_MAIN                 1

#define AV_PROFILE_APV_422_10  33
#define AV_PROFILE_APV_422_12  44
#define AV_PROFILE_APV_444_10  55
#define AV_PROFILE_APV_444_12  66
#define AV_PROFILE_APV_4444_10 77
#define AV_PROFILE_APV_4444_12 88
#define AV_PROFILE_APV_400_10  99

#define AV_LEVEL_UNKNOWN                  -99

enum AVFieldOrder {
    AV_FIELD_UNKNOWN,
    AV_FIELD_PROGRESSIVE,
    AV_FIELD_TT,
    AV_FIELD_BB,
    AV_FIELD_TB,
    AV_FIELD_BT,
};

enum AVDiscard{

    AVDISCARD_NONE    =-16,
    AVDISCARD_DEFAULT =  0,
    AVDISCARD_NONREF  =  8,
    AVDISCARD_BIDIR   = 16,
    AVDISCARD_NONINTRA= 24,
    AVDISCARD_NONKEY  = 32,
    AVDISCARD_ALL     = 48,
};

enum AVAudioServiceType {
    AV_AUDIO_SERVICE_TYPE_MAIN              = 0,
    AV_AUDIO_SERVICE_TYPE_EFFECTS           = 1,
    AV_AUDIO_SERVICE_TYPE_VISUALLY_IMPAIRED = 2,
    AV_AUDIO_SERVICE_TYPE_HEARING_IMPAIRED  = 3,
    AV_AUDIO_SERVICE_TYPE_DIALOGUE          = 4,
    AV_AUDIO_SERVICE_TYPE_COMMENTARY        = 5,
    AV_AUDIO_SERVICE_TYPE_EMERGENCY         = 6,
    AV_AUDIO_SERVICE_TYPE_VOICE_OVER        = 7,
    AV_AUDIO_SERVICE_TYPE_KARAOKE           = 8,
    AV_AUDIO_SERVICE_TYPE_NB                   ,
};

typedef struct AVPanScan {

    int id;

    int width;
    int height;

    int16_t position[3][2];
} AVPanScan;

typedef struct AVCPBProperties {

    int64_t max_bitrate;

    int64_t min_bitrate;

    int64_t avg_bitrate;

    int64_t buffer_size;

    uint64_t vbv_delay;
} AVCPBProperties;

AVCPBProperties *av_cpb_properties_alloc(size_t *size);

typedef struct AVProducerReferenceTime {

    int64_t wallclock;
    int flags;
} AVProducerReferenceTime;

typedef struct AVRTCPSenderReport {
    uint32_t ssrc;
    uint64_t ntp_timestamp;
    uint32_t rtp_timestamp;
    uint32_t sender_nb_packets;
    uint32_t sender_nb_bytes;
} AVRTCPSenderReport;

unsigned int av_xiphlacing(unsigned char *s, unsigned int v);

#endif
