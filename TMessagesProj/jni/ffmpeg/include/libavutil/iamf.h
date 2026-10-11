#ifndef AVUTIL_IAMF_H
#define AVUTIL_IAMF_H

#include <stdint.h>
#include <stddef.h>

#include "attributes.h"
#include "avassert.h"
#include "channel_layout.h"
#include "dict.h"
#include "rational.h"

enum AVIAMFAnimationType {
    AV_IAMF_ANIMATION_TYPE_STEP,
    AV_IAMF_ANIMATION_TYPE_LINEAR,
    AV_IAMF_ANIMATION_TYPE_BEZIER,
};

typedef struct AVIAMFMixGain {
    const AVClass *av_class;

    unsigned int subblock_duration;

    enum AVIAMFAnimationType animation_type;

    AVRational start_point_value;

    AVRational end_point_value;

    AVRational control_point_value;

    AVRational control_point_relative_time;
} AVIAMFMixGain;

typedef struct AVIAMFDemixingInfo {
    const AVClass *av_class;

    unsigned int subblock_duration;

    unsigned int dmixp_mode;
} AVIAMFDemixingInfo;

typedef struct AVIAMFReconGain {
    const AVClass *av_class;

    unsigned int subblock_duration;

    uint8_t recon_gain[6][12];
} AVIAMFReconGain;

enum AVIAMFParamDefinitionType {

    AV_IAMF_PARAMETER_DEFINITION_MIX_GAIN,

    AV_IAMF_PARAMETER_DEFINITION_DEMIXING,

    AV_IAMF_PARAMETER_DEFINITION_RECON_GAIN,
};

typedef struct AVIAMFParamDefinition {
    const AVClass *av_class;

    size_t subblocks_offset;

    size_t subblock_size;

    unsigned int nb_subblocks;

    enum AVIAMFParamDefinitionType type;

    unsigned int parameter_id;

    unsigned int parameter_rate;

    unsigned int duration;

    unsigned int constant_subblock_duration;
} AVIAMFParamDefinition;

const AVClass *av_iamf_param_definition_get_class(void);

AVIAMFParamDefinition *av_iamf_param_definition_alloc(enum AVIAMFParamDefinitionType type,
                                                      unsigned int nb_subblocks, size_t *size);

static av_always_inline void*
av_iamf_param_definition_get_subblock(const AVIAMFParamDefinition *par, unsigned int idx)
{
    av_assert0(idx < par->nb_subblocks);
    return (void *)((uint8_t *)par + par->subblocks_offset + idx * par->subblock_size);
}

enum AVIAMFAmbisonicsMode {
    AV_IAMF_AMBISONICS_MODE_MONO,
    AV_IAMF_AMBISONICS_MODE_PROJECTION,
};

#define AV_IAMF_LAYER_FLAG_RECON_GAIN (1 << 0)

typedef struct AVIAMFLayer {
    const AVClass *av_class;

    AVChannelLayout ch_layout;

    unsigned int flags;

    unsigned int output_gain_flags;

    AVRational output_gain;

    enum AVIAMFAmbisonicsMode ambisonics_mode;

    AVRational *demixing_matrix;

    unsigned int nb_demixing_matrix;
} AVIAMFLayer;

enum AVIAMFAudioElementType {
    AV_IAMF_AUDIO_ELEMENT_TYPE_CHANNEL,
    AV_IAMF_AUDIO_ELEMENT_TYPE_SCENE,
};

typedef struct AVIAMFAudioElement {
    const AVClass *av_class;

    AVIAMFLayer **layers;

    unsigned int nb_layers;

    AVIAMFParamDefinition *demixing_info;

    AVIAMFParamDefinition *recon_gain_info;

    enum AVIAMFAudioElementType audio_element_type;

    unsigned int default_w;
} AVIAMFAudioElement;

const AVClass *av_iamf_audio_element_get_class(void);

AVIAMFAudioElement *av_iamf_audio_element_alloc(void);

AVIAMFLayer *av_iamf_audio_element_add_layer(AVIAMFAudioElement *audio_element);

void av_iamf_audio_element_free(AVIAMFAudioElement **audio_element);

enum AVIAMFHeadphonesMode {

    AV_IAMF_HEADPHONES_MODE_STEREO,

    AV_IAMF_HEADPHONES_MODE_BINAURAL,
};

typedef struct AVIAMFSubmixElement {
    const AVClass *av_class;

    unsigned int audio_element_id;

    AVIAMFParamDefinition *element_mix_config;

    AVRational default_mix_gain;

    enum AVIAMFHeadphonesMode headphones_rendering_mode;

    AVDictionary *annotations;
} AVIAMFSubmixElement;

enum AVIAMFSubmixLayoutType {

    AV_IAMF_SUBMIX_LAYOUT_TYPE_LOUDSPEAKERS = 2,

    AV_IAMF_SUBMIX_LAYOUT_TYPE_BINAURAL = 3,
};

typedef struct AVIAMFSubmixLayout {
    const AVClass *av_class;

    enum AVIAMFSubmixLayoutType layout_type;

    AVChannelLayout sound_system;

    AVRational integrated_loudness;

    AVRational digital_peak;

    AVRational true_peak;

    AVRational dialogue_anchored_loudness;

    AVRational album_anchored_loudness;
} AVIAMFSubmixLayout;

typedef struct AVIAMFSubmix {
    const AVClass *av_class;

    AVIAMFSubmixElement **elements;

    unsigned int nb_elements;

    AVIAMFSubmixLayout **layouts;

    unsigned int nb_layouts;

    AVIAMFParamDefinition *output_mix_config;

    AVRational default_mix_gain;
} AVIAMFSubmix;

typedef struct AVIAMFMixPresentation {
    const AVClass *av_class;

    AVIAMFSubmix **submixes;

    unsigned int nb_submixes;

    AVDictionary *annotations;
} AVIAMFMixPresentation;

const AVClass *av_iamf_mix_presentation_get_class(void);

AVIAMFMixPresentation *av_iamf_mix_presentation_alloc(void);

AVIAMFSubmix *av_iamf_mix_presentation_add_submix(AVIAMFMixPresentation *mix_presentation);

AVIAMFSubmixElement *av_iamf_submix_add_element(AVIAMFSubmix *submix);

AVIAMFSubmixLayout *av_iamf_submix_add_layout(AVIAMFSubmix *submix);

void av_iamf_mix_presentation_free(AVIAMFMixPresentation **mix_presentation);

#endif
