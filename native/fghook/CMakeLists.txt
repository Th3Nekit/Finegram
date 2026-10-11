cmake_minimum_required(VERSION 3.22)
project(fghook C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PREBUILT ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/${ANDROID_ABI})

add_library(lsplant SHARED IMPORTED)
set_target_properties(lsplant PROPERTIES IMPORTED_LOCATION ${PREBUILT}/liblsplant.so)

add_library(shadowhook SHARED IMPORTED)
set_target_properties(shadowhook PROPERTIES IMPORTED_LOCATION ${PREBUILT}/libshadowhook.so)

add_library(xz STATIC
        third_party/xz/xz_crc32.c
        third_party/xz/xz_crc64.c
        third_party/xz/xz_dec_lzma2.c
        third_party/xz/xz_dec_stream.c)
target_include_directories(xz PUBLIC third_party/xz)
target_compile_definitions(xz PUBLIC XZ_USE_CRC64 XZ_DEC_ANY_CHECK)
set_target_properties(xz PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11)

add_library(fghook SHARED src/fghook.cpp src/art_symbols.cpp)
target_include_directories(fghook PRIVATE third_party/include)
target_link_libraries(fghook PRIVATE xz lsplant shadowhook log)
target_compile_options(fghook PRIVATE -O2 -Wall -fvisibility=hidden)
target_link_options(fghook PRIVATE -Wl,--exclude-libs,ALL -Wl,--gc-sections -Wl,-z,max-page-size=16384 -s)
