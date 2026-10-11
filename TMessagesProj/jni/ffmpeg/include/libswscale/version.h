#ifndef SWSCALE_VERSION_H
#define SWSCALE_VERSION_H

#include "libavutil/version.h"

#include "version_major.h"

#define LIBSWSCALE_VERSION_MINOR   5
#define LIBSWSCALE_VERSION_MICRO 102

#define LIBSWSCALE_VERSION_INT  AV_VERSION_INT(LIBSWSCALE_VERSION_MAJOR, \
                                               LIBSWSCALE_VERSION_MINOR, \
                                               LIBSWSCALE_VERSION_MICRO)
#define LIBSWSCALE_VERSION      AV_VERSION(LIBSWSCALE_VERSION_MAJOR, \
                                           LIBSWSCALE_VERSION_MINOR, \
                                           LIBSWSCALE_VERSION_MICRO)
#define LIBSWSCALE_BUILD        LIBSWSCALE_VERSION_INT

#define LIBSWSCALE_IDENT        "SwS" AV_STRINGIFY(LIBSWSCALE_VERSION)

#endif
