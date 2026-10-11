/* Copyright (c) 2006, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* minidump_format.h: A cross-platform reimplementation of minidump-related
 * portions of DbgHelp.h from the Windows Platform SDK.
 *
 * (This is C99 source, please don't corrupt it with C++.)
 *
 * Structures that are defined by Microsoft to contain a zero-length array
 * are instead defined here to contain an array with one element, as
 * zero-length arrays are forbidden by standard C and C++.  In these cases,
 * *_minsize constants are provided to be used in place of sizeof.  For a
 * cleaner interface to these sizes when using C++, see minidump_size.h.
 *
 * These structures are also sufficient to populate minidump files.
 *
 * These definitions may be extended to support handling minidump files
 * for other CPUs and other operating systems.
 *
 * Because precise data type sizes are crucial for this implementation to
 * function properly and portably in terms of interoperability with minidumps
 * produced by DbgHelp on Windows, a set of primitive types with known sizes
 * are used as the basis of each structure defined by this file.  DbgHelp
 * on Windows is assumed to be the reference implementation; this file
 * seeks to provide a cross-platform compatible implementation.  To avoid
 * collisions with the types and values defined and used by DbgHelp in the
 * event that this implementation is used on Windows, each type and value
 * defined here is given a new name, beginning with "MD".  Names of the
 * equivalent types and values in the Windows Platform SDK are given in
 * comments.
 *
 * Author: Mark Mentovai */

#ifndef GOOGLE_BREAKPAD_COMMON_MINIDUMP_FORMAT_H__
#define GOOGLE_BREAKPAD_COMMON_MINIDUMP_FORMAT_H__

#include <stddef.h>

#include "google_breakpad/common/breakpad_types.h"

#if defined(_MSC_VER)

#pragma warning(push)
#pragma warning(disable:4200)
#endif

typedef struct {
  uint32_t data1;
  uint16_t data2;
  uint16_t data3;
  uint8_t  data4[8];
} MDGUID;

#define MD_CONTEXT_IA64  0x00080000

#define MD_CONTEXT_SHX   0x000000c0
#define MD_CONTEXT_ALPHA 0x00020000

#define MD_CONTEXT_CPU_MASK 0xffffff00

typedef struct {
  uint32_t context_flags;
} MDRawContextBase;

#include "minidump_cpu_amd64.h"
#include "minidump_cpu_arm.h"
#include "minidump_cpu_arm64.h"
#include "minidump_cpu_mips.h"
#include "minidump_cpu_ppc.h"
#include "minidump_cpu_ppc64.h"
#include "minidump_cpu_sparc.h"
#include "minidump_cpu_x86.h"

typedef struct {
  uint32_t signature;
  uint32_t struct_version;
  uint32_t file_version_hi;
  uint32_t file_version_lo;
  uint32_t product_version_hi;
  uint32_t product_version_lo;
  uint32_t file_flags_mask;
  uint32_t file_flags;
  uint32_t file_os;
  uint32_t file_type;
  uint32_t file_subtype;
  uint32_t file_date_hi;
  uint32_t file_date_lo;
} MDVSFixedFileInfo;

#define MD_VSFIXEDFILEINFO_SIGNATURE 0xfeef04bd

#define MD_VSFIXEDFILEINFO_VERSION 0x00010000

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_DEBUG        0x00000001

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRERELEASE   0x00000002

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PATCHED      0x00000004

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRIVATEBUILD 0x00000008

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_INFOINFERRED 0x00000010

#define MD_VSFIXEDFILEINFO_FILE_FLAGS_SPECIALBUILD 0x00000020

#define MD_VSFIXEDFILEINFO_FILE_OS_UNKNOWN    0
#define MD_VSFIXEDFILEINFO_FILE_OS_DOS        (1 << 16)
#define MD_VSFIXEDFILEINFO_FILE_OS_OS216      (2 << 16)
#define MD_VSFIXEDFILEINFO_FILE_OS_OS232      (3 << 16)
#define MD_VSFIXEDFILEINFO_FILE_OS_NT         (4 << 16)
#define MD_VSFIXEDFILEINFO_FILE_OS_WINCE      (5 << 16)

#define MD_VSFIXEDFILEINFO_FILE_OS__BASE      0
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS16 1
#define MD_VSFIXEDFILEINFO_FILE_OS__PM16      2
#define MD_VSFIXEDFILEINFO_FILE_OS__PM32      3
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS32 4

#define MD_VSFIXEDFILEINFO_FILE_TYPE_UNKNOWN    0
#define MD_VSFIXEDFILEINFO_FILE_TYPE_APP        1
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DLL        2
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DRV        3
#define MD_VSFIXEDFILEINFO_FILE_TYPE_FONT       4
#define MD_VSFIXEDFILEINFO_FILE_TYPE_VXD        5
#define MD_VSFIXEDFILEINFO_FILE_TYPE_STATIC_LIB 7

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_UNKNOWN                0

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_PRINTER            1

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_KEYBOARD           2

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_LANGUAGE           3

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_DISPLAY            4

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_MOUSE              5

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_NETWORK            6

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SYSTEM             7

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INSTALLABLE        8

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SOUND              9

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_COMM              10

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INPUTMETHOD       11

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_VERSIONED_PRINTER 12

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_RASTER            1

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_VECTOR            2

#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_TRUETYPE          3

typedef uint32_t MDRVA;

typedef struct {
  uint32_t  data_size;
  MDRVA     rva;
} MDLocationDescriptor;

typedef struct {

  uint64_t             start_of_memory_range;

  MDLocationDescriptor memory;
} MDMemoryDescriptor;

typedef struct {
  uint32_t  signature;
  uint32_t  version;
  uint32_t  stream_count;
  MDRVA     stream_directory_rva;

  uint32_t  checksum;

  uint32_t  time_date_stamp;
  uint64_t  flags;
} MDRawHeader;

#define MD_HEADER_SIGNATURE 0x504d444d

#define MD_HEADER_VERSION   0x0000a793

typedef enum {

  MD_NORMAL                            = 0x00000000,
  MD_WITH_DATA_SEGS                    = 0x00000001,
  MD_WITH_FULL_MEMORY                  = 0x00000002,
  MD_WITH_HANDLE_DATA                  = 0x00000004,
  MD_FILTER_MEMORY                     = 0x00000008,
  MD_SCAN_MEMORY                       = 0x00000010,
  MD_WITH_UNLOADED_MODULES             = 0x00000020,
  MD_WITH_INDIRECTLY_REFERENCED_MEMORY = 0x00000040,
  MD_FILTER_MODULE_PATHS               = 0x00000080,
  MD_WITH_PROCESS_THREAD_DATA          = 0x00000100,
  MD_WITH_PRIVATE_READ_WRITE_MEMORY    = 0x00000200,
  MD_WITHOUT_OPTIONAL_DATA             = 0x00000400,
  MD_WITH_FULL_MEMORY_INFO             = 0x00000800,
  MD_WITH_THREAD_INFO                  = 0x00001000,
  MD_WITH_CODE_SEGS                    = 0x00002000,
  MD_WITHOUT_AUXILLIARY_SEGS           = 0x00004000,
  MD_WITH_FULL_AUXILLIARY_STATE        = 0x00008000,
  MD_WITH_PRIVATE_WRITE_COPY_MEMORY    = 0x00010000,
  MD_IGNORE_INACCESSIBLE_MEMORY        = 0x00020000,
  MD_WITH_TOKEN_INFORMATION            = 0x00040000
} MDType;

typedef struct {
  uint32_t             stream_type;
  MDLocationDescriptor location;
} MDRawDirectory;

typedef enum {
  MD_UNUSED_STREAM               =  0,
  MD_RESERVED_STREAM_0           =  1,
  MD_RESERVED_STREAM_1           =  2,
  MD_THREAD_LIST_STREAM          =  3,
  MD_MODULE_LIST_STREAM          =  4,
  MD_MEMORY_LIST_STREAM          =  5,
  MD_EXCEPTION_STREAM            =  6,
  MD_SYSTEM_INFO_STREAM          =  7,
  MD_THREAD_EX_LIST_STREAM       =  8,
  MD_MEMORY_64_LIST_STREAM       =  9,
  MD_COMMENT_STREAM_A            = 10,
  MD_COMMENT_STREAM_W            = 11,
  MD_HANDLE_DATA_STREAM          = 12,
  MD_FUNCTION_TABLE_STREAM       = 13,
  MD_UNLOADED_MODULE_LIST_STREAM = 14,
  MD_MISC_INFO_STREAM            = 15,
  MD_MEMORY_INFO_LIST_STREAM     = 16,
  MD_THREAD_INFO_LIST_STREAM     = 17,
  MD_HANDLE_OPERATION_LIST_STREAM = 18,
  MD_LAST_RESERVED_STREAM        = 0x0000ffff,

  MD_BREAKPAD_INFO_STREAM        = 0x47670001,
  MD_ASSERTION_INFO_STREAM       = 0x47670002,

  MD_LINUX_CPU_INFO              = 0x47670003,
  MD_LINUX_PROC_STATUS           = 0x47670004,
  MD_LINUX_LSB_RELEASE           = 0x47670005,
  MD_LINUX_CMD_LINE              = 0x47670006,
  MD_LINUX_ENVIRON               = 0x47670007,
  MD_LINUX_AUXV                  = 0x47670008,
  MD_LINUX_MAPS                  = 0x47670009,
  MD_LINUX_DSO_DEBUG             = 0x4767000A
} MDStreamType;

typedef struct {
  uint32_t length;

  uint16_t buffer[1];
} MDString;

static const size_t MDString_minsize = offsetof(MDString, buffer[0]);

typedef struct {
  uint32_t             thread_id;
  uint32_t             suspend_count;
  uint32_t             priority_class;
  uint32_t             priority;
  uint64_t             teb;
  MDMemoryDescriptor   stack;
  MDLocationDescriptor thread_context;
} MDRawThread;

typedef struct {
  uint32_t    number_of_threads;
  MDRawThread threads[1];
} MDRawThreadList;

static const size_t MDRawThreadList_minsize = offsetof(MDRawThreadList,
                                                       threads[0]);

typedef struct {
  uint64_t             base_of_image;
  uint32_t             size_of_image;
  uint32_t             checksum;
  uint32_t             time_date_stamp;
  MDRVA                module_name_rva;
  MDVSFixedFileInfo    version_info;

  MDLocationDescriptor cv_record;

  MDLocationDescriptor misc_record;

  uint32_t             reserved0[2];
  uint32_t             reserved1[2];
} MDRawModule;

#define MD_MODULE_SIZE 108

typedef struct {
  uint32_t signature;
  uint32_t offset;
} MDCVHeader;

typedef struct {
  MDCVHeader cv_header;
  uint32_t   signature;
  uint32_t   age;
  uint8_t    pdb_file_name[1];
} MDCVInfoPDB20;

static const size_t MDCVInfoPDB20_minsize = offsetof(MDCVInfoPDB20,
                                                     pdb_file_name[0]);

#define MD_CVINFOPDB20_SIGNATURE 0x3031424e

typedef struct {
  uint32_t  cv_signature;
  MDGUID    signature;
  uint32_t  age;
  uint8_t   pdb_file_name[1];

} MDCVInfoPDB70;

static const size_t MDCVInfoPDB70_minsize = offsetof(MDCVInfoPDB70,
                                                     pdb_file_name[0]);

#define MD_CVINFOPDB70_SIGNATURE 0x53445352

typedef struct {
  uint32_t data1[2];
  uint32_t data2;
  uint32_t data3;
  uint32_t data4;
  uint32_t data5[3];
  uint8_t  extra[2];
} MDCVInfoELF;

#define MD_CVINFOCV41_SIGNATURE 0x3930424e
#define MD_CVINFOCV50_SIGNATURE 0x3131424e

#define MD_CVINFOUNKNOWN_SIGNATURE 0xffffffff

typedef struct {
  uint32_t  data_type;

  uint32_t  length;
  uint8_t   unicode;
  uint8_t   reserved[3];
  uint8_t   data[1];
} MDImageDebugMisc;

static const size_t MDImageDebugMisc_minsize = offsetof(MDImageDebugMisc,
                                                        data[0]);

typedef struct {
  uint32_t    number_of_modules;
  MDRawModule modules[1];
} MDRawModuleList;

static const size_t MDRawModuleList_minsize = offsetof(MDRawModuleList,
                                                       modules[0]);

typedef struct {
  uint32_t           number_of_memory_ranges;
  MDMemoryDescriptor memory_ranges[1];
} MDRawMemoryList;

static const size_t MDRawMemoryList_minsize = offsetof(MDRawMemoryList,
                                                       memory_ranges[0]);

#define MD_EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct {
  uint32_t  exception_code;

  uint32_t  exception_flags;

  uint64_t  exception_record;

  uint64_t  exception_address;

  uint32_t  number_parameters;

  uint32_t  __align;
  uint64_t  exception_information[MD_EXCEPTION_MAXIMUM_PARAMETERS];
} MDException;

#include "minidump_exception_linux.h"
#include "minidump_exception_mac.h"
#include "minidump_exception_ps3.h"
#include "minidump_exception_solaris.h"
#include "minidump_exception_win32.h"

typedef struct {
  uint32_t             thread_id;

  uint32_t             __align;
  MDException          exception_record;
  MDLocationDescriptor thread_context;
} MDRawExceptionStream;

typedef union {
  struct {
    uint32_t vendor_id[3];
    uint32_t version_information;
    uint32_t feature_information;
    uint32_t amd_extended_cpu_features;
  } x86_cpu_info;
  struct {
    uint32_t cpuid;
    uint32_t elf_hwcaps;
  } arm_cpu_info;
  struct {
    uint64_t processor_features[2];
  } other_cpu_info;
} MDCPUInformation;

typedef enum {
  MD_CPU_ARM_ELF_HWCAP_SWP       = (1 << 0),
  MD_CPU_ARM_ELF_HWCAP_HALF      = (1 << 1),
  MD_CPU_ARM_ELF_HWCAP_THUMB     = (1 << 2),
  MD_CPU_ARM_ELF_HWCAP_26BIT     = (1 << 3),
  MD_CPU_ARM_ELF_HWCAP_FAST_MULT = (1 << 4),
  MD_CPU_ARM_ELF_HWCAP_FPA       = (1 << 5),
  MD_CPU_ARM_ELF_HWCAP_VFP       = (1 << 6),
  MD_CPU_ARM_ELF_HWCAP_EDSP      = (1 << 7),
  MD_CPU_ARM_ELF_HWCAP_JAVA      = (1 << 8),
  MD_CPU_ARM_ELF_HWCAP_IWMMXT    = (1 << 9),
  MD_CPU_ARM_ELF_HWCAP_CRUNCH    = (1 << 10),
  MD_CPU_ARM_ELF_HWCAP_THUMBEE   = (1 << 11),
  MD_CPU_ARM_ELF_HWCAP_NEON      = (1 << 12),
  MD_CPU_ARM_ELF_HWCAP_VFPv3     = (1 << 13),
  MD_CPU_ARM_ELF_HWCAP_VFPv3D16  = (1 << 14),
  MD_CPU_ARM_ELF_HWCAP_TLS       = (1 << 15),
  MD_CPU_ARM_ELF_HWCAP_VFPv4     = (1 << 16),
  MD_CPU_ARM_ELF_HWCAP_IDIVA     = (1 << 17),
  MD_CPU_ARM_ELF_HWCAP_IDIVT     = (1 << 18),
} MDCPUInformationARMElfHwCaps;

typedef struct {

  uint16_t         processor_architecture;
  uint16_t         processor_level;

  uint16_t         processor_revision;

  uint8_t          number_of_processors;
  uint8_t          product_type;

  uint32_t         major_version;
  uint32_t         minor_version;
  uint32_t         build_number;
  uint32_t         platform_id;
  MDRVA            csd_version_rva;

  uint16_t         suite_mask;
  uint16_t         reserved2;

  MDCPUInformation cpu;
} MDRawSystemInfo;

typedef enum {
  MD_CPU_ARCHITECTURE_X86       =  0,
  MD_CPU_ARCHITECTURE_MIPS      =  1,
  MD_CPU_ARCHITECTURE_ALPHA     =  2,
  MD_CPU_ARCHITECTURE_PPC       =  3,
  MD_CPU_ARCHITECTURE_SHX       =  4,

  MD_CPU_ARCHITECTURE_ARM       =  5,
  MD_CPU_ARCHITECTURE_IA64      =  6,
  MD_CPU_ARCHITECTURE_ALPHA64   =  7,
  MD_CPU_ARCHITECTURE_MSIL      =  8,

  MD_CPU_ARCHITECTURE_AMD64     =  9,
  MD_CPU_ARCHITECTURE_X86_WIN64 = 10,

  MD_CPU_ARCHITECTURE_SPARC     = 0x8001,
  MD_CPU_ARCHITECTURE_PPC64     = 0x8002,
  MD_CPU_ARCHITECTURE_ARM64     = 0x8003,
  MD_CPU_ARCHITECTURE_UNKNOWN   = 0xffff
} MDCPUArchitecture;

typedef enum {
  MD_OS_WIN32S        = 0,
  MD_OS_WIN32_WINDOWS = 1,
  MD_OS_WIN32_NT      = 2,
  MD_OS_WIN32_CE      = 3,

  MD_OS_UNIX          = 0x8000,
  MD_OS_MAC_OS_X      = 0x8101,
  MD_OS_IOS           = 0x8102,
  MD_OS_LINUX         = 0x8201,
  MD_OS_SOLARIS       = 0x8202,
  MD_OS_ANDROID       = 0x8203,
  MD_OS_PS3           = 0x8204,
  MD_OS_NACL          = 0x8205
} MDOSPlatform;

typedef struct {
  uint16_t year;
  uint16_t month;
  uint16_t day_of_week;
  uint16_t day;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint16_t milliseconds;
} MDSystemTime;

typedef struct {

  int32_t bias;

  uint16_t standard_name[32];

  MDSystemTime standard_date;

  int32_t standard_bias;

  uint16_t daylight_name[32];

  MDSystemTime daylight_date;

  int32_t daylight_bias;
} MDTimeZoneInformation;

#define MD_MAX_PATH 260

typedef struct {
  uint32_t size_of_info;
  uint32_t flags1;

  uint32_t process_id;

  uint32_t process_create_time;
  uint32_t process_user_time;
  uint32_t process_kernel_time;

  uint32_t processor_max_mhz;
  uint32_t processor_current_mhz;
  uint32_t processor_mhz_limit;
  uint32_t processor_max_idle_state;
  uint32_t processor_current_idle_state;

  uint32_t process_integrity_level;

  uint32_t process_execute_flags;

  uint32_t protected_process;

  uint32_t time_zone_id;
  MDTimeZoneInformation time_zone;

  uint16_t build_string[MD_MAX_PATH];
  uint16_t dbg_bld_str[40];
} MDRawMiscInfo;

static const size_t MD_MISCINFO_SIZE =
    offsetof(MDRawMiscInfo, processor_max_mhz);
static const size_t MD_MISCINFO2_SIZE =
    offsetof(MDRawMiscInfo, process_integrity_level);
static const size_t MD_MISCINFO3_SIZE =
    offsetof(MDRawMiscInfo, build_string[0]);
static const size_t MD_MISCINFO4_SIZE = sizeof(MDRawMiscInfo);

typedef enum {
  MD_MISCINFO_FLAGS1_PROCESS_ID            = 0x00000001,

  MD_MISCINFO_FLAGS1_PROCESS_TIMES         = 0x00000002,

  MD_MISCINFO_FLAGS1_PROCESSOR_POWER_INFO  = 0x00000004,

  MD_MISCINFO_FLAGS1_PROCESS_INTEGRITY     = 0x00000010,

  MD_MISCINFO_FLAGS1_PROCESS_EXECUTE_FLAGS = 0x00000020,

  MD_MISCINFO_FLAGS1_TIMEZONE              = 0x00000040,

  MD_MISCINFO_FLAGS1_PROTECTED_PROCESS     = 0x00000080,

  MD_MISCINFO_FLAGS1_BUILDSTRING           = 0x00000100,

} MDMiscInfoFlags1;

typedef struct {
  uint32_t size_of_header;
  uint32_t size_of_entry;
  uint64_t number_of_entries;
} MDRawMemoryInfoList;

typedef struct {
  uint64_t  base_address;
  uint64_t  allocation_base;

  uint32_t  allocation_protection;

  uint32_t  __alignment1;
  uint64_t  region_size;
  uint32_t  state;
  uint32_t  protection;
  uint32_t  type;
  uint32_t  __alignment2;
} MDRawMemoryInfo;

typedef enum {
  MD_MEMORY_STATE_COMMIT   = 0x1000,
  MD_MEMORY_STATE_RESERVE  = 0x2000,
  MD_MEMORY_STATE_FREE     = 0x10000
} MDMemoryState;

typedef enum {
  MD_MEMORY_PROTECT_NOACCESS          = 0x01,
  MD_MEMORY_PROTECT_READONLY          = 0x02,
  MD_MEMORY_PROTECT_READWRITE         = 0x04,
  MD_MEMORY_PROTECT_WRITECOPY         = 0x08,
  MD_MEMORY_PROTECT_EXECUTE           = 0x10,
  MD_MEMORY_PROTECT_EXECUTE_READ      = 0x20,
  MD_MEMORY_PROTECT_EXECUTE_READWRITE = 0x40,
  MD_MEMORY_PROTECT_EXECUTE_WRITECOPY = 0x80,

  MD_MEMORY_PROTECT_GUARD             = 0x100,
  MD_MEMORY_PROTECT_NOCACHE           = 0x200,
  MD_MEMORY_PROTECT_WRITECOMBINE      = 0x400,
} MDMemoryProtection;

const uint32_t MD_MEMORY_PROTECTION_ACCESS_MASK = 0xFF;

typedef enum {
  MD_MEMORY_TYPE_PRIVATE = 0x20000,
  MD_MEMORY_TYPE_MAPPED  = 0x40000,
  MD_MEMORY_TYPE_IMAGE   = 0x1000000
} MDMemoryType;

typedef struct {

  uint32_t validity;

  uint32_t dump_thread_id;

  uint32_t requesting_thread_id;
} MDRawBreakpadInfo;

typedef enum {

  MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID       = 1 << 0,

  MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID = 1 << 1
} MDBreakpadInfoValidity;

typedef struct {

  uint16_t expression[128];
  uint16_t function[128];
  uint16_t file[128];
  uint32_t line;
  uint32_t type;
} MDRawAssertionInfo;

typedef enum {
  MD_ASSERTION_INFO_TYPE_UNKNOWN = 0,

  MD_ASSERTION_INFO_TYPE_INVALID_PARAMETER,

  MD_ASSERTION_INFO_TYPE_PURE_VIRTUAL_CALL
} MDAssertionInfoData;

typedef struct {
  uint32_t  addr;
  MDRVA     name;
  uint32_t  ld;
} MDRawLinkMap32;

typedef struct {
  uint32_t  version;
  MDRVA     map;
  uint32_t  dso_count;
  uint32_t  brk;
  uint32_t  ldbase;
  uint32_t  dynamic;
} MDRawDebug32;

typedef struct {
  uint64_t  addr;
  MDRVA     name;
  uint64_t  ld;
} MDRawLinkMap64;

typedef struct {
  uint32_t  version;
  MDRVA     map;
  uint32_t  dso_count;
  uint64_t  brk;
  uint64_t  ldbase;
  uint64_t  dynamic;
} MDRawDebug64;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
