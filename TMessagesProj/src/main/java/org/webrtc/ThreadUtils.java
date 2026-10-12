/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import androidx.annotation.Nullable;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ThreadUtils {

  public static class ThreadChecker {
    @Nullable private Thread thread = Thread.currentThread();

    public void checkIsOnValidThread() {
      if (thread == null) {
        thread = Thread.currentThread();
      }
      if (Thread.currentThread() != thread) {
        throw new IllegalStateException("Wrong thread");
      }
    }

    public void detachThread() {
      thread = null;
    }
  }

  public static void checkIsOnMainThread() {
    if (Thread.currentThread() != Looper.getMainLooper().getThread()) {
      throw new IllegalStateException("Not on main thread!");
    }
  }

  public interface BlockingOperation { void run() throws InterruptedException; }

  public static void executeUninterruptibly(BlockingOperation operation) {
    boolean wasInterrupted = false;
    while (true) {
      try {
        operation.run();
        break;
      } catch (InterruptedException e) {

        wasInterrupted = true;
      }
    }

    if (wasInterrupted) {
      Thread.currentThread().interrupt();
    }
  }

  public static boolean joinUninterruptibly(final Thread thread, long timeoutMs) {
    final long startTimeMs = SystemClock.elapsedRealtime();
    long timeRemainingMs = timeoutMs;
    boolean wasInterrupted = false;
    while (timeRemainingMs > 0) {
      try {
        thread.join(timeRemainingMs);
        break;
      } catch (InterruptedException e) {

        wasInterrupted = true;
        final long elapsedTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
        timeRemainingMs = timeoutMs - elapsedTimeMs;
      }
    }

    if (wasInterrupted) {
      Thread.currentThread().interrupt();
    }
    return !thread.isAlive();
  }

  public static void joinUninterruptibly(final Thread thread) {
    executeUninterruptibly(new BlockingOperation() {
      @Override
      public void run() throws InterruptedException {
        thread.join();
      }
    });
  }

  public static void awaitUninterruptibly(final CountDownLatch latch) {
    executeUninterruptibly(new BlockingOperation() {
      @Override
      public void run() throws InterruptedException {
        latch.await();
      }
    });
  }

  public static boolean awaitUninterruptibly(CountDownLatch barrier, long timeoutMs) {
    final long startTimeMs = SystemClock.elapsedRealtime();
    long timeRemainingMs = timeoutMs;
    boolean wasInterrupted = false;
    boolean result = false;
    do {
      try {
        result = barrier.await(timeRemainingMs, TimeUnit.MILLISECONDS);
        break;
      } catch (InterruptedException e) {

        wasInterrupted = true;
        final long elapsedTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
        timeRemainingMs = timeoutMs - elapsedTimeMs;
      }
    } while (timeRemainingMs > 0);

    if (wasInterrupted) {
      Thread.currentThread().interrupt();
    }
    return result;
  }

  public static <V> V invokeAtFrontUninterruptibly(
      final Handler handler, final Callable<V> callable) {
    if (handler.getLooper().getThread() == Thread.currentThread()) {
      try {
        return callable.call();
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    }

    class CaughtException {
      Exception e;
    }
    class Result {
      public V value;
    }
    final Result result = new Result();
    final CaughtException caughtException = new CaughtException();
    final CountDownLatch barrier = new CountDownLatch(1);
    handler.post(new Runnable() {
      @Override
      public void run() {
        try {
          result.value = callable.call();
        } catch (Exception e) {
          caughtException.e = e;
        }
        barrier.countDown();
      }
    });
    awaitUninterruptibly(barrier);

    if (caughtException.e != null) {
      final RuntimeException runtimeException = new RuntimeException(caughtException.e);
      runtimeException.setStackTrace(
          concatStackTraces(caughtException.e.getStackTrace(), runtimeException.getStackTrace()));
      throw runtimeException;
    }
    return result.value;
  }

  public static void invokeAtFrontUninterruptibly(final Handler handler, final Runnable runner) {
    invokeAtFrontUninterruptibly(handler, new Callable<Void>() {
      @Override
      public Void call() {
        runner.run();
        return null;
      }
    });
  }

  static StackTraceElement[] concatStackTraces(
      StackTraceElement[] inner, StackTraceElement[] outer) {
    final StackTraceElement[] combined = new StackTraceElement[inner.length + outer.length];
    System.arraycopy(inner, 0, combined, 0, inner.length);
    System.arraycopy(outer, 0, combined, inner.length, outer.length);
    return combined;
  }
}
