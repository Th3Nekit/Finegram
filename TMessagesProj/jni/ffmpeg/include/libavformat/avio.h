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
#ifndef AVFORMAT_AVIO_H
#define AVFORMAT_AVIO_H

#include <stdint.h>
#include <stdio.h>

#include "libavutil/attributes.h"
#include "libavutil/dict.h"
#include "libavutil/log.h"

#include "libavformat/version_major.h"

#define AVIO_SEEKABLE_NORMAL (1 << 0)

#define AVIO_SEEKABLE_TIME   (1 << 1)

typedef struct AVIOInterruptCB {
    int (*callback)(void*);
    void *opaque;
} AVIOInterruptCB;

enum AVIODirEntryType {
    AVIO_ENTRY_UNKNOWN,
    AVIO_ENTRY_BLOCK_DEVICE,
    AVIO_ENTRY_CHARACTER_DEVICE,
    AVIO_ENTRY_DIRECTORY,
    AVIO_ENTRY_NAMED_PIPE,
    AVIO_ENTRY_SYMBOLIC_LINK,
    AVIO_ENTRY_SOCKET,
    AVIO_ENTRY_FILE,
    AVIO_ENTRY_SERVER,
    AVIO_ENTRY_SHARE,
    AVIO_ENTRY_WORKGROUP,
};

typedef struct AVIODirEntry {
    char *name;
    int type;
    int utf8;

    int64_t size;
    int64_t modification_timestamp;

    int64_t access_timestamp;

    int64_t status_change_timestamp;

    int64_t user_id;
    int64_t group_id;
    int64_t filemode;
} AVIODirEntry;

typedef struct AVIODirContext AVIODirContext;

enum AVIODataMarkerType {

    AVIO_DATA_MARKER_HEADER,

    AVIO_DATA_MARKER_SYNC_POINT,

    AVIO_DATA_MARKER_BOUNDARY_POINT,

    AVIO_DATA_MARKER_UNKNOWN,

    AVIO_DATA_MARKER_TRAILER,

    AVIO_DATA_MARKER_FLUSH_POINT,
};

typedef struct AVIOContext {

    const AVClass *av_class;

    unsigned char *buffer;
    int buffer_size;
    unsigned char *buf_ptr;
    unsigned char *buf_end;

    void *opaque;

    int (*read_packet)(void *opaque, uint8_t *buf, int buf_size);
    int (*write_packet)(void *opaque, const uint8_t *buf, int buf_size);
    int64_t (*seek)(void *opaque, int64_t offset, int whence);
    int64_t pos;
    int eof_reached;
    int error;
    int write_flag;
    int max_packet_size;
    int min_packet_size;

    unsigned long checksum;
    unsigned char *checksum_ptr;
    unsigned long (*update_checksum)(unsigned long checksum, const uint8_t *buf, unsigned int size);

    int (*read_pause)(void *opaque, int pause);

    int64_t (*read_seek)(void *opaque, int stream_index,
                         int64_t timestamp, int flags);

    int seekable;

    int direct;

    const char *protocol_whitelist;

    const char *protocol_blacklist;

    int (*write_data_type)(void *opaque, const uint8_t *buf, int buf_size,
                           enum AVIODataMarkerType type, int64_t time);

    int ignore_boundary_point;

    unsigned char *buf_ptr_max;

    int64_t bytes_read;

    int64_t bytes_written;
} AVIOContext;

const char *avio_find_protocol_name(const char *url);

int avio_check(const char *url, int flags);

int avio_open_dir(AVIODirContext **s, const char *url, AVDictionary **options);

int avio_read_dir(AVIODirContext *s, AVIODirEntry **next);

int avio_close_dir(AVIODirContext **s);

void avio_free_directory_entry(AVIODirEntry **entry);

AVIOContext *avio_alloc_context(
                  unsigned char *buffer,
                  int buffer_size,
                  int write_flag,
                  void *opaque,
                  int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
                  int (*write_packet)(void *opaque, const uint8_t *buf, int buf_size),
                  int64_t (*seek)(void *opaque, int64_t offset, int whence));

void avio_context_free(AVIOContext **s);

void avio_w8(AVIOContext *s, int b);
void avio_write(AVIOContext *s, const unsigned char *buf, int size);
void avio_wl64(AVIOContext *s, uint64_t val);
void avio_wb64(AVIOContext *s, uint64_t val);
void avio_wl32(AVIOContext *s, unsigned int val);
void avio_wb32(AVIOContext *s, unsigned int val);
void avio_wl24(AVIOContext *s, unsigned int val);
void avio_wb24(AVIOContext *s, unsigned int val);
void avio_wl16(AVIOContext *s, unsigned int val);
void avio_wb16(AVIOContext *s, unsigned int val);

int avio_put_str(AVIOContext *s, const char *str);

int avio_put_str16le(AVIOContext *s, const char *str);

int avio_put_str16be(AVIOContext *s, const char *str);

void avio_write_marker(AVIOContext *s, int64_t time, enum AVIODataMarkerType type);

#define AVSEEK_SIZE 0x10000

#define AVSEEK_FORCE 0x20000

int64_t avio_seek(AVIOContext *s, int64_t offset, int whence);

int64_t avio_skip(AVIOContext *s, int64_t offset);

static av_always_inline int64_t avio_tell(AVIOContext *s)
{
    return avio_seek(s, 0, SEEK_CUR);
}

int64_t avio_size(AVIOContext *s);

int avio_feof(AVIOContext *s);

int avio_vprintf(AVIOContext *s, const char *fmt, va_list ap);

int avio_printf(AVIOContext *s, const char *fmt, ...) av_printf_format(2, 3);

void avio_print_string_array(AVIOContext *s, const char * const strings[]);

#define avio_print(s, ...) \
    avio_print_string_array(s, (const char*[]){__VA_ARGS__, NULL})

void avio_flush(AVIOContext *s);

int avio_read(AVIOContext *s, unsigned char *buf, int size);

int avio_read_partial(AVIOContext *s, unsigned char *buf, int size);

int          avio_r8  (AVIOContext *s);
unsigned int avio_rl16(AVIOContext *s);
unsigned int avio_rl24(AVIOContext *s);
unsigned int avio_rl32(AVIOContext *s);
uint64_t     avio_rl64(AVIOContext *s);
unsigned int avio_rb16(AVIOContext *s);
unsigned int avio_rb24(AVIOContext *s);
unsigned int avio_rb32(AVIOContext *s);
uint64_t     avio_rb64(AVIOContext *s);

int avio_get_str(AVIOContext *pb, int maxlen, char *buf, int buflen);

int avio_get_str16le(AVIOContext *pb, int maxlen, char *buf, int buflen);
int avio_get_str16be(AVIOContext *pb, int maxlen, char *buf, int buflen);

#define AVIO_FLAG_READ  1
#define AVIO_FLAG_WRITE 2
#define AVIO_FLAG_READ_WRITE (AVIO_FLAG_READ|AVIO_FLAG_WRITE)

#define AVIO_FLAG_NONBLOCK 8

#define AVIO_FLAG_DIRECT 0x8000

int avio_open(AVIOContext **s, const char *url, int flags);

int avio_open2(AVIOContext **s, const char *url, int flags,
               const AVIOInterruptCB *int_cb, AVDictionary **options);

int avio_close(AVIOContext *s);

int avio_closep(AVIOContext **s);

int avio_open_dyn_buf(AVIOContext **s);

int avio_get_dyn_buf(AVIOContext *s, uint8_t **pbuffer);

int avio_close_dyn_buf(AVIOContext *s, uint8_t **pbuffer);

const char *avio_enum_protocols(void **opaque, int output);

const AVClass *avio_protocol_get_class(const char *name);

int     avio_pause(AVIOContext *h, int pause);

int64_t avio_seek_time(AVIOContext *h, int stream_index,
                       int64_t timestamp, int flags);

struct AVBPrint;

int avio_read_to_bprint(AVIOContext *h, struct AVBPrint *pb, size_t max_size);

int avio_accept(AVIOContext *s, AVIOContext **c);

int avio_handshake(AVIOContext *c);
#endif
