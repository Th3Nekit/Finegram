// Copyright 2012 The Chromium Authors

#ifndef JNI_ZERO_SCOPED_JAVA_REF_H_
#define JNI_ZERO_SCOPED_JAVA_REF_H_

#include <jni.h>
#include <stddef.h>

#include <type_traits>
#include <utility>

#include "third_party/jni_zero/jni_export.h"
#include "third_party/jni_zero/logging.h"

namespace jni_zero {

class JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalFrame {
 public:
  explicit ScopedJavaLocalFrame(JNIEnv* env);
  ScopedJavaLocalFrame(JNIEnv* env, int capacity);

  ScopedJavaLocalFrame(const ScopedJavaLocalFrame&) = delete;
  ScopedJavaLocalFrame& operator=(const ScopedJavaLocalFrame&) = delete;

  ~ScopedJavaLocalFrame();

 private:

  JNIEnv* env_;
};

template <typename T>
class JavaRef;

template <>
class JNI_ZERO_COMPONENT_BUILD_EXPORT JavaRef<jobject> {
 public:

  constexpr JavaRef() {}

  constexpr JavaRef(std::nullptr_t) {}

  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  ~JavaRef() {}

  jobject obj() const { return obj_; }

  explicit operator bool() const { return obj_ != nullptr; }

  bool is_null() const { return obj_ == nullptr; }

 protected:

#if JNI_ZERO_DCHECK_IS_ON()

  JavaRef(JNIEnv* env, jobject obj);
#else
  JavaRef(JNIEnv* env, jobject obj) : obj_(obj) {}
#endif

  void steal(JavaRef&& other) {
    obj_ = other.obj_;
    other.obj_ = nullptr;
  }

  JNIEnv* SetNewLocalRef(JNIEnv* env, jobject obj);
  void SetNewGlobalRef(JNIEnv* env, jobject obj);
  void ResetLocalRef(JNIEnv* env);
  void ResetGlobalRef();
  jobject ReleaseInternal();

 private:
  jobject obj_ = nullptr;
};

template <typename T>
class JavaObjectArrayReader;

template <typename T>
class JavaRef : public JavaRef<jobject> {
 public:
  constexpr JavaRef() {}
  constexpr JavaRef(std::nullptr_t) {}

  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  ~JavaRef() {}

  T obj() const { return static_cast<T>(JavaRef<jobject>::obj()); }

  template <typename ElementType,
            typename T_ = T,
            typename = std::enable_if_t<std::is_same_v<T_, jobjectArray>>>
  JavaObjectArrayReader<ElementType> ReadElements() const {
    return JavaObjectArrayReader<ElementType>(*this);
  }

 protected:
  JavaRef(JNIEnv* env, T obj) : JavaRef<jobject>(env, obj) {}
};

template <typename T>
class JavaParamRef : public JavaRef<T> {
 public:

  JavaParamRef(JNIEnv* env, T obj) : JavaRef<T>(env, obj) {}

  JavaParamRef(std::nullptr_t) {}

  JavaParamRef(const JavaParamRef&) = delete;
  JavaParamRef& operator=(const JavaParamRef&) = delete;

  ~JavaParamRef() {}

  operator T() const { return JavaRef<T>::obj(); }
};

template <typename T>
class ScopedJavaLocalRef : public JavaRef<T> {
 public:

  static ScopedJavaLocalRef Adopt(JNIEnv* env, T obj) {
    return ScopedJavaLocalRef(env, obj);
  }

  constexpr ScopedJavaLocalRef() {}
  constexpr ScopedJavaLocalRef(std::nullptr_t) {}

  ScopedJavaLocalRef(const ScopedJavaLocalRef& other) : env_(other.env_) {
    JavaRef<T>::SetNewLocalRef(env_, other.obj());
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaLocalRef(const ScopedJavaLocalRef<U>& other) : env_(other.env_) {
    JavaRef<T>::SetNewLocalRef(env_, other.obj());
  }

  ScopedJavaLocalRef(ScopedJavaLocalRef&& other) : env_(other.env_) {
    JavaRef<T>::steal(std::move(other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaLocalRef(ScopedJavaLocalRef<U>&& other) : env_(other.env_) {
    JavaRef<T>::steal(std::move(other));
  }

  explicit ScopedJavaLocalRef(const JavaRef<T>& other) { Reset(other); }

  ScopedJavaLocalRef(JNIEnv* env, T obj) : JavaRef<T>(env, obj), env_(env) {}

  ~ScopedJavaLocalRef() { Reset(); }

  ScopedJavaLocalRef& operator=(std::nullptr_t) {
    Reset();
    return *this;
  }

  ScopedJavaLocalRef& operator=(const ScopedJavaLocalRef& other) {
    Reset(other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaLocalRef& operator=(const ScopedJavaLocalRef<U>& other) {
    Reset(other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaLocalRef& operator=(ScopedJavaLocalRef<U>&& other) {
    env_ = other.env_;
    Reset();
    JavaRef<T>::steal(std::move(other));
    return *this;
  }

  ScopedJavaLocalRef& operator=(const JavaRef<T>& other) {
    Reset(other);
    return *this;
  }

  void Reset() { JavaRef<T>::ResetLocalRef(env_); }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  void Reset(const ScopedJavaLocalRef<U>& other) {

    env_ = JavaRef<T>::SetNewLocalRef(other.env_, other.obj());
  }

  void Reset(const JavaRef<T>& other) {

    env_ = JavaRef<T>::SetNewLocalRef(env_, other.obj());
  }

  T Release() { return static_cast<T>(JavaRef<T>::ReleaseInternal()); }

 private:

  JNIEnv* env_ = nullptr;

  ScopedJavaLocalRef(JNIEnv* env, const JavaParamRef<T>& other);

  template <typename U>
  friend class ScopedJavaLocalRef;

  template <typename U>
  friend class JavaObjectArrayReader;
};

template <typename T>
class ScopedJavaGlobalRef : public JavaRef<T> {
 public:
  constexpr ScopedJavaGlobalRef() {}
  constexpr ScopedJavaGlobalRef(std::nullptr_t) {}

  ScopedJavaGlobalRef(const ScopedJavaGlobalRef& other) { Reset(other); }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaGlobalRef(const ScopedJavaGlobalRef<U>& other) {
    Reset(other);
  }

  ScopedJavaGlobalRef(ScopedJavaGlobalRef&& other) {
    JavaRef<T>::steal(std::move(other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaGlobalRef(ScopedJavaGlobalRef<U>&& other) {
    JavaRef<T>::steal(std::move(other));
  }

  explicit ScopedJavaGlobalRef(const JavaRef<T>& other) { Reset(other); }

  ScopedJavaGlobalRef(JNIEnv* env, T obj) { Reset(env, obj); }

  ~ScopedJavaGlobalRef() { Reset(); }

  ScopedJavaGlobalRef& operator=(std::nullptr_t) {
    Reset();
    return *this;
  }

  ScopedJavaGlobalRef& operator=(const ScopedJavaGlobalRef& other) {
    Reset(other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaGlobalRef& operator=(const ScopedJavaGlobalRef<U>& other) {
    Reset(other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  ScopedJavaGlobalRef& operator=(ScopedJavaGlobalRef<U>&& other) {
    Reset();
    JavaRef<T>::steal(std::move(other));
    return *this;
  }

  ScopedJavaGlobalRef& operator=(const JavaRef<T>& other) {
    Reset(other);
    return *this;
  }

  void Reset() { JavaRef<T>::ResetGlobalRef(); }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  void Reset(const ScopedJavaGlobalRef<U>& other) {
    Reset(nullptr, other.obj());
  }

  void Reset(const JavaRef<T>& other) { Reset(nullptr, other.obj()); }

  void Reset(JNIEnv* env, const JavaParamRef<T>& other) {
    Reset(env, other.obj());
  }

  void Reset(JNIEnv* env, T obj) { JavaRef<T>::SetNewGlobalRef(env, obj); }

  T Release() { return static_cast<T>(JavaRef<T>::ReleaseInternal()); }
};

template <typename T>
class JavaObjectArrayReader {
 public:
  class iterator {
   public:

    using iterator_category = std::input_iterator_tag;

    using difference_type = ptrdiff_t;
    using value_type = ScopedJavaLocalRef<T>;

    using reference = value_type;

    class pointer {
     public:
      explicit pointer(const reference& ref) : ref_(ref) {}
      pointer(const pointer& ptr) = default;
      pointer& operator=(const pointer& ptr) = delete;
      reference* operator->() { return &ref_; }

     private:
      reference ref_;
    };

    iterator(const iterator&) = default;
    ~iterator() = default;

    iterator& operator=(const iterator&) = default;

    bool operator==(const iterator& other) const {
      JNI_ZERO_DCHECK(reader_ == other.reader_);
      return i_ == other.i_;
    }

    bool operator!=(const iterator& other) const {
      JNI_ZERO_DCHECK(reader_ == other.reader_);
      return i_ != other.i_;
    }

    reference operator*() const {
      JNI_ZERO_DCHECK(i_ < reader_->size_);

      return value_type::Adopt(
          reader_->array_.env_,
          static_cast<T>(reader_->array_.env_->GetObjectArrayElement(
              reader_->array_.obj(), i_)));
    }

    pointer operator->() const { return pointer(operator*()); }

    iterator& operator++() {
      JNI_ZERO_DCHECK(i_ < reader_->size_);
      ++i_;
      return *this;
    }

    iterator operator++(int) {
      iterator old = *this;
      ++*this;
      return old;
    }

   private:
    iterator(const JavaObjectArrayReader* reader, jsize i)
        : reader_(reader), i_(i) {}
    const JavaObjectArrayReader<T>* reader_;
    jsize i_;

    friend JavaObjectArrayReader;
  };

  JavaObjectArrayReader(const JavaRef<jobjectArray>& array) : array_(array) {
    size_ = array_.env_->GetArrayLength(array_.obj());
  }

  JavaObjectArrayReader(const JavaObjectArrayReader& other) = default;

  JavaObjectArrayReader& operator=(const JavaObjectArrayReader& other) =
      default;

  JavaObjectArrayReader(JavaObjectArrayReader&& other) = default;
  JavaObjectArrayReader& operator=(JavaObjectArrayReader&& other) = default;

  bool empty() const { return size_ == 0; }

  jsize size() const { return size_; }

  iterator begin() const { return iterator(this, 0); }

  iterator end() const { return iterator(this, size_); }

 private:
  ScopedJavaLocalRef<jobjectArray> array_;
  jsize size_;

  friend iterator;
};

}

#endif
