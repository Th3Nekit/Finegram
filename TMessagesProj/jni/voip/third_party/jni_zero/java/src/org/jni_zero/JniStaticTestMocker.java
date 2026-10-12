// Copyright 2018 The Chromium Authors

package org.jni_zero;

public interface JniStaticTestMocker<T> {
    void setInstanceForTesting(T instance);
}
