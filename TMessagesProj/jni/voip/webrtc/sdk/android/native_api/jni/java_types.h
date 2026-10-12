/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Android's FindClass() is tricky because the app-specific ClassLoader is not
// consulted when there is no app-specific frame on the stack (i.e. when called
// from a thread created from native C++ code). These helper functions provide a
// workaround for this.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass

#ifndef SDK_ANDROID_NATIVE_API_JNI_JAVA_TYPES_H_
#define SDK_ANDROID_NATIVE_API_JNI_JAVA_TYPES_H_

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {

class Iterable {
 public:
  Iterable(JNIEnv* jni, const JavaRef<jobject>& iterable);
  Iterable(Iterable&& other);

  ~Iterable();

  Iterable(const Iterable&) = delete;
  Iterable& operator=(const Iterable&) = delete;

  class Iterator {
   public:

    Iterator();

    Iterator(JNIEnv* jni, const JavaRef<jobject>& iterable);

    Iterator(Iterator&& other);

    ~Iterator();

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    Iterator& operator=(Iterator&&) = delete;

    Iterator& operator++();

    void Remove();

    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other) { return !(*this == other); }
    ScopedJavaLocalRef<jobject>& operator*();

   private:
    bool AtEnd() const;

    JNIEnv* jni_ = nullptr;
    ScopedJavaLocalRef<jobject> iterator_;
    ScopedJavaLocalRef<jobject> value_;
    SequenceChecker thread_checker_;
  };

  Iterable::Iterator begin() { return Iterable::Iterator(jni_, iterable_); }
  Iterable::Iterator end() { return Iterable::Iterator(); }

 private:
  JNIEnv* jni_;
  ScopedJavaLocalRef<jobject> iterable_;
};

bool IsNull(JNIEnv* jni, const JavaRef<jobject>& obj);

std::string GetJavaEnumName(JNIEnv* jni, const JavaRef<jobject>& j_enum);

Iterable GetJavaMapEntrySet(JNIEnv* jni, const JavaRef<jobject>& j_map);
ScopedJavaLocalRef<jobject> GetJavaMapEntryKey(JNIEnv* jni,
                                               const JavaRef<jobject>& j_entry);
ScopedJavaLocalRef<jobject> GetJavaMapEntryValue(
    JNIEnv* jni,
    const JavaRef<jobject>& j_entry);

int64_t JavaToNativeLong(JNIEnv* env, const JavaRef<jobject>& j_long);

absl::optional<bool> JavaToNativeOptionalBool(JNIEnv* jni,
                                              const JavaRef<jobject>& boolean);
absl::optional<double> JavaToNativeOptionalDouble(
    JNIEnv* jni,
    const JavaRef<jobject>& j_double);
absl::optional<int32_t> JavaToNativeOptionalInt(
    JNIEnv* jni,
    const JavaRef<jobject>& integer);

std::string JavaToNativeString(JNIEnv* jni, const JavaRef<jstring>& j_string);

template <typename T, typename Convert>
std::vector<T> JavaToNativeVector(JNIEnv* env,
                                  const JavaRef<jobjectArray>& j_container,
                                  Convert convert) {
  std::vector<T> container;
  const size_t size = env->GetArrayLength(j_container.obj());
  container.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    container.emplace_back(convert(
        env, ScopedJavaLocalRef<jobject>(
                 env, env->GetObjectArrayElement(j_container.obj(), i))));
  }
  CHECK_EXCEPTION(env) << "Error during JavaToNativeVector";
  return container;
}

template <typename T, typename Java_T = jobject, typename Convert>
std::vector<T> JavaListToNativeVector(JNIEnv* env,
                                      const JavaRef<jobject>& j_list,
                                      Convert convert) {
  std::vector<T> native_list;
  if (!j_list.is_null()) {
    for (ScopedJavaLocalRef<jobject>& j_item : Iterable(env, j_list)) {
      native_list.emplace_back(
          convert(env, static_java_ref_cast<Java_T>(env, j_item)));
    }
    CHECK_EXCEPTION(env) << "Error during JavaListToNativeVector";
  }
  return native_list;
}

template <typename Key, typename T, typename Convert>
std::map<Key, T> JavaToNativeMap(JNIEnv* env,
                                 const JavaRef<jobject>& j_map,
                                 Convert convert) {
  std::map<Key, T> container;
  for (auto const& j_entry : GetJavaMapEntrySet(env, j_map)) {
    container.emplace(convert(env, GetJavaMapEntryKey(env, j_entry),
                              GetJavaMapEntryValue(env, j_entry)));
  }
  return container;
}

std::map<std::string, std::string> JavaToNativeStringMap(
    JNIEnv* env,
    const JavaRef<jobject>& j_map);

ScopedJavaLocalRef<jobject> NativeToJavaBoolean(JNIEnv* env, bool b);
ScopedJavaLocalRef<jobject> NativeToJavaDouble(JNIEnv* env, double d);
ScopedJavaLocalRef<jobject> NativeToJavaInteger(JNIEnv* jni, int32_t i);
ScopedJavaLocalRef<jobject> NativeToJavaLong(JNIEnv* env, int64_t u);
ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* jni, const char* str);
ScopedJavaLocalRef<jstring> NativeToJavaString(JNIEnv* jni,
                                               const std::string& str);

ScopedJavaLocalRef<jobject> NativeToJavaDouble(
    JNIEnv* jni,
    const absl::optional<double>& optional_double);
ScopedJavaLocalRef<jobject> NativeToJavaInteger(
    JNIEnv* jni,
    const absl::optional<int32_t>& optional_int);
ScopedJavaLocalRef<jstring> NativeToJavaString(
    JNIEnv* jni,
    const absl::optional<std::string>& str);

template <typename T, typename Convert>
ScopedJavaLocalRef<jobjectArray> NativeToJavaObjectArray(
    JNIEnv* env,
    const std::vector<T>& container,
    jclass clazz,
    Convert convert) {
  ScopedJavaLocalRef<jobjectArray> j_container(
      env, env->NewObjectArray(container.size(), clazz, nullptr));
  int i = 0;
  for (const T& element : container) {
    env->SetObjectArrayElement(j_container.obj(), i,
                               convert(env, element).obj());
    ++i;
  }
  return j_container;
}

ScopedJavaLocalRef<jbyteArray> NativeToJavaByteArray(
    JNIEnv* env,
    rtc::ArrayView<int8_t> container);
ScopedJavaLocalRef<jintArray> NativeToJavaIntArray(
    JNIEnv* env,
    rtc::ArrayView<int32_t> container);

std::vector<int8_t> JavaToNativeByteArray(JNIEnv* env,
                                          const JavaRef<jbyteArray>& jarray);
std::vector<int32_t> JavaToNativeIntArray(JNIEnv* env,
                                          const JavaRef<jintArray>& jarray);
std::vector<float> JavaToNativeFloatArray(JNIEnv* env,
                                          const JavaRef<jfloatArray>& jarray);

ScopedJavaLocalRef<jobjectArray> NativeToJavaBooleanArray(
    JNIEnv* env,
    const std::vector<bool>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaDoubleArray(
    JNIEnv* env,
    const std::vector<double>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaIntegerArray(
    JNIEnv* env,
    const std::vector<int32_t>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaLongArray(
    JNIEnv* env,
    const std::vector<int64_t>& container);
ScopedJavaLocalRef<jobjectArray> NativeToJavaStringArray(
    JNIEnv* env,
    const std::vector<std::string>& container);

class JavaListBuilder {
 public:
  explicit JavaListBuilder(JNIEnv* env);
  ~JavaListBuilder();
  void add(const JavaRef<jobject>& element);
  ScopedJavaLocalRef<jobject> java_list() { return j_list_; }

 private:
  JNIEnv* env_;
  ScopedJavaLocalRef<jobject> j_list_;
};

template <typename C, typename Convert>
ScopedJavaLocalRef<jobject> NativeToJavaList(JNIEnv* env,
                                             const C& container,
                                             Convert convert) {
  JavaListBuilder builder(env);
  for (const auto& e : container)
    builder.add(convert(env, e));
  return builder.java_list();
}

class JavaMapBuilder {
 public:
  explicit JavaMapBuilder(JNIEnv* env);
  ~JavaMapBuilder();
  void put(const JavaRef<jobject>& key, const JavaRef<jobject>& value);
  ScopedJavaLocalRef<jobject> GetJavaMap() { return j_map_; }

 private:
  JNIEnv* env_;
  ScopedJavaLocalRef<jobject> j_map_;
};

template <typename C, typename Convert>
ScopedJavaLocalRef<jobject> NativeToJavaMap(JNIEnv* env,
                                            const C& container,
                                            Convert convert) {
  JavaMapBuilder builder(env);
  for (const auto& e : container) {
    const auto key_value_pair = convert(env, e);
    builder.put(key_value_pair.first, key_value_pair.second);
  }
  return builder.GetJavaMap();
}

template <typename C>
ScopedJavaLocalRef<jobject> NativeToJavaStringMap(JNIEnv* env,
                                                  const C& container) {
  JavaMapBuilder builder(env);
  for (const auto& e : container) {
    const auto key_value_pair = std::make_pair(
        NativeToJavaString(env, e.first), NativeToJavaString(env, e.second));
    builder.put(key_value_pair.first, key_value_pair.second);
  }
  return builder.GetJavaMap();
}

jlong NativeToJavaPointer(const void* ptr);

inline std::string JavaToStdString(JNIEnv* jni,
                                   const JavaRef<jstring>& j_string) {
  return JavaToNativeString(jni, j_string);
}

inline std::string JavaToStdString(JNIEnv* jni, jstring j_string) {
  return JavaToStdString(jni, JavaParamRef<jstring>(j_string));
}

std::vector<std::string> JavaToStdVectorStrings(JNIEnv* jni,
                                                const JavaRef<jobject>& list);

inline std::map<std::string, std::string> JavaToStdMapStrings(
    JNIEnv* jni,
    const JavaRef<jobject>& j_map) {
  return JavaToNativeStringMap(jni, j_map);
}

inline std::map<std::string, std::string> JavaToStdMapStrings(JNIEnv* jni,
                                                              jobject j_map) {
  return JavaToStdMapStrings(jni, JavaParamRef<jobject>(j_map));
}

}

#endif
