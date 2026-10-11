// Copyright 2014 The Chromium Authors

#ifndef JNI_ZERO_JNI_INT_WRAPPER_H_
#define JNI_ZERO_JNI_INT_WRAPPER_H_

#ifdef NDEBUG

typedef jint JniIntWrapper;

inline jint as_jint(JniIntWrapper wrapper) {
  return wrapper;
}

#else

class JniIntWrapper {
 public:
  JniIntWrapper() : i_(0) {}
  JniIntWrapper(int i) : i_(i) {}
  JniIntWrapper(const JniIntWrapper& ji) : i_(ji.i_) {}
  template <class T>
  JniIntWrapper(const T& t) : i_(t) {}
  jint as_jint() const { return i_; }

 private:

  JniIntWrapper(long);
  jint i_;
};

inline jint as_jint(const JniIntWrapper& wrapper) {
  return wrapper.as_jint();
}

#endif

#endif
