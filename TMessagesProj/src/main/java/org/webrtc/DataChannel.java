/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;

public class DataChannel {

  public static class Init {
    public boolean ordered = true;

    public int maxRetransmitTimeMs = -1;

    public int maxRetransmits = -1;
    public String protocol = "";
    public boolean negotiated;

    public int id = -1;

    @CalledByNative("Init")
    boolean getOrdered() {
      return ordered;
    }

    @CalledByNative("Init")
    int getMaxRetransmitTimeMs() {
      return maxRetransmitTimeMs;
    }

    @CalledByNative("Init")
    int getMaxRetransmits() {
      return maxRetransmits;
    }

    @CalledByNative("Init")
    String getProtocol() {
      return protocol;
    }

    @CalledByNative("Init")
    boolean getNegotiated() {
      return negotiated;
    }

    @CalledByNative("Init")
    int getId() {
      return id;
    }
  }

  public static class Buffer {

    public final ByteBuffer data;

    public final boolean binary;

    @CalledByNative("Buffer")
    public Buffer(ByteBuffer data, boolean binary) {
      this.data = data;
      this.binary = binary;
    }
  }

  public interface Observer {

    @CalledByNative("Observer") public void onBufferedAmountChange(long previousAmount);

    @CalledByNative("Observer") public void onStateChange();

    @CalledByNative("Observer") public void onMessage(Buffer buffer);
  }

  public enum State {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED;

    @CalledByNative("State")
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  private long nativeDataChannel;
  private long nativeObserver;

  @CalledByNative
  public DataChannel(long nativeDataChannel) {
    this.nativeDataChannel = nativeDataChannel;
  }

  public void registerObserver(Observer observer) {
    checkDataChannelExists();
    if (nativeObserver != 0) {
      nativeUnregisterObserver(nativeObserver);
    }
    nativeObserver = nativeRegisterObserver(observer);
  }

  public void unregisterObserver() {
    checkDataChannelExists();
    nativeUnregisterObserver(nativeObserver);
    nativeObserver = 0;
  }

  public String label() {
    checkDataChannelExists();
    return nativeLabel();
  }

  public int id() {
    checkDataChannelExists();
    return nativeId();
  }

  public State state() {
    checkDataChannelExists();
    return nativeState();
  }

  public long bufferedAmount() {
    checkDataChannelExists();
    return nativeBufferedAmount();
  }

  public void close() {
    checkDataChannelExists();
    nativeClose();
  }

  public boolean send(Buffer buffer) {
    checkDataChannelExists();

    byte[] data = new byte[buffer.data.remaining()];
    buffer.data.get(data);
    return nativeSend(data, buffer.binary);
  }

  public void dispose() {
    checkDataChannelExists();
    JniCommon.nativeReleaseRef(nativeDataChannel);
    nativeDataChannel = 0;
  }

  @CalledByNative
  long getNativeDataChannel() {
    return nativeDataChannel;
  }

  private void checkDataChannelExists() {
    if (nativeDataChannel == 0) {
      throw new IllegalStateException("DataChannel has been disposed.");
    }
  }

  private native long nativeRegisterObserver(Observer observer);
  private native void nativeUnregisterObserver(long observer);
  private native String nativeLabel();
  private native int nativeId();
  private native State nativeState();
  private native long nativeBufferedAmount();
  private native void nativeClose();
  private native boolean nativeSend(byte[] data, boolean binary);
};
