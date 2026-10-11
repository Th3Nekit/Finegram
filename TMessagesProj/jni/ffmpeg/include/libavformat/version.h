#ifndef AVFORMAT_VERSION_H
#define AVFORMAT_VERSION_H

#include "libavutil/version.h"

#include "version_major.h"

#define LIBAVFORMAT_VERSION_MINOR  12
#define LIBAVFORMAT_VERSION_MICRO 102

#define LIBAVFORMAT_VERSION_INT AV_VERSION_INT(LIBAVFORMAT_VERSION_MAJOR, \
                                               LIBAVFORMAT_VERSION_MINOR, \
                                               LIBAVFORMAT_VERSION_MICRO)
#define LIBAVFORMAT_VERSION     AV_VERSION(LIBAVFORMAT_VERSION_MAJOR,   \
                                           LIBAVFORMAT_VERSION_MINOR,   \
                                           LIBAVFORMAT_VERSION_MICRO)
#define LIBAVFORMAT_BUILD       LIBAVFORMAT_VERSION_INT

#define LIBAVFORMAT_IDENT       "Lavf" AV_STRINGIFY(LIBAVFORMAT_VERSION)

#endif
