// Copyright 2023 The Chromium Authors

#ifndef JNI_ZERO_JNI_EXPORT_H_
#define JNI_ZERO_JNI_EXPORT_H_

#if defined(__i386__)

#define JNI_BOUNDARY_EXPORT \
  extern "C" __attribute__((visibility("default"), force_align_arg_pointer))
#else
#define JNI_BOUNDARY_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(COMPONENT_BUILD)
#define JNI_ZERO_COMPONENT_BUILD_EXPORT __attribute__((visibility("default")))
#else
#define JNI_ZERO_COMPONENT_BUILD_EXPORT
#endif

#endif
