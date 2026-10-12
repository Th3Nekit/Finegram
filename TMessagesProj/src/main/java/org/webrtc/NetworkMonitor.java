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

import android.content.Context;
import android.os.Build;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.List;
import org.webrtc.NetworkChangeDetector;

public class NetworkMonitor {

  public interface NetworkObserver {
    public void onConnectionTypeChanged(NetworkChangeDetector.ConnectionType connectionType);
  }

  private static final String TAG = "NetworkMonitor";

  private static class InstanceHolder {

    static final NetworkMonitor instance = new NetworkMonitor();
  }

  private NetworkChangeDetectorFactory networkChangeDetectorFactory =
      new NetworkChangeDetectorFactory() {
        @Override
        public NetworkChangeDetector create(
            NetworkChangeDetector.Observer observer, Context context) {
          return new NetworkMonitorAutoDetect(observer, context);
        }
      };

  private final ArrayList<Long> nativeNetworkObservers;

  private final ArrayList<NetworkObserver> networkObservers;

  private final Object networkChangeDetectorLock = new Object();

  @Nullable private NetworkChangeDetector networkChangeDetector;

  private int numObservers;

  private volatile NetworkChangeDetector.ConnectionType currentConnectionType;

  private NetworkMonitor() {
    nativeNetworkObservers = new ArrayList<Long>();
    networkObservers = new ArrayList<NetworkObserver>();
    numObservers = 0;
    currentConnectionType = NetworkChangeDetector.ConnectionType.CONNECTION_UNKNOWN;
  }

  public void setNetworkChangeDetectorFactory(NetworkChangeDetectorFactory factory) {
    assertIsTrue(numObservers == 0);
    this.networkChangeDetectorFactory = factory;
  }

  @Deprecated
  public static void init(Context context) {}

  @CalledByNative
  public static NetworkMonitor getInstance() {
    return InstanceHolder.instance;
  }

  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected to be true");
    }
  }

  public void startMonitoring(Context applicationContext, String fieldTrialsString) {
    synchronized (networkChangeDetectorLock) {
      ++numObservers;
      if (networkChangeDetector == null) {
        networkChangeDetector = createNetworkChangeDetector(applicationContext, fieldTrialsString);
      }
      currentConnectionType = networkChangeDetector.getCurrentConnectionType();
    }
  }

  @Deprecated
  public void startMonitoring(Context applicationContext) {
    startMonitoring(applicationContext, "");
  }

  @Deprecated
  public void startMonitoring() {
    startMonitoring(ContextUtils.getApplicationContext(), "");
  }

  @CalledByNative
  private void startMonitoring(
      @Nullable Context applicationContext, long nativeObserver, String fieldTrialsString) {
    Logging.d(TAG,
        "Start monitoring with native observer " + nativeObserver
            + " fieldTrialsString: " + fieldTrialsString);

    startMonitoring(
        applicationContext != null ? applicationContext : ContextUtils.getApplicationContext(),
        fieldTrialsString);

    synchronized (nativeNetworkObservers) {
      nativeNetworkObservers.add(nativeObserver);
    }

    updateObserverActiveNetworkList(nativeObserver);

    notifyObserversOfConnectionTypeChange(currentConnectionType);
  }

  public void stopMonitoring() {
    synchronized (networkChangeDetectorLock) {
      if (--numObservers == 0) {
        networkChangeDetector.destroy();
        networkChangeDetector = null;
      }
    }
  }

  @CalledByNative
  private void stopMonitoring(long nativeObserver) {
    Logging.d(TAG, "Stop monitoring with native observer " + nativeObserver);
    stopMonitoring();
    synchronized (nativeNetworkObservers) {
      nativeNetworkObservers.remove(nativeObserver);
    }
  }

  @CalledByNative
  private boolean networkBindingSupported() {
    synchronized (networkChangeDetectorLock) {
      return networkChangeDetector != null && networkChangeDetector.supportNetworkCallback();
    }
  }

  @CalledByNative
  private static int androidSdkInt() {
    return Build.VERSION.SDK_INT;
  }

  private NetworkChangeDetector.ConnectionType getCurrentConnectionType() {
    return currentConnectionType;
  }

  private NetworkChangeDetector createNetworkChangeDetector(
      Context appContext, String fieldTrialsString) {
    return networkChangeDetectorFactory.create(new NetworkChangeDetector.Observer() {
      @Override
      public void onConnectionTypeChanged(NetworkChangeDetector.ConnectionType newConnectionType) {
        updateCurrentConnectionType(newConnectionType);
      }

      @Override
      public void onNetworkConnect(NetworkChangeDetector.NetworkInformation networkInfo) {
        notifyObserversOfNetworkConnect(networkInfo);
      }

      @Override
      public void onNetworkDisconnect(long networkHandle) {
        notifyObserversOfNetworkDisconnect(networkHandle);
      }

      @Override
      public void onNetworkPreference(
          List<NetworkChangeDetector.ConnectionType> types, int preference) {
        notifyObserversOfNetworkPreference(types, preference);
      }
    }, appContext);
  }

  private void updateCurrentConnectionType(NetworkChangeDetector.ConnectionType newConnectionType) {
    currentConnectionType = newConnectionType;
    notifyObserversOfConnectionTypeChange(newConnectionType);
  }

  private void notifyObserversOfConnectionTypeChange(
      NetworkChangeDetector.ConnectionType newConnectionType) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyConnectionTypeChanged(nativeObserver);
    }

    List<NetworkObserver> javaObservers;
    synchronized (networkObservers) {
      javaObservers = new ArrayList<>(networkObservers);
    }
    for (NetworkObserver observer : javaObservers) {
      observer.onConnectionTypeChanged(newConnectionType);
    }
  }

  private void notifyObserversOfNetworkConnect(
      NetworkChangeDetector.NetworkInformation networkInfo) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyOfNetworkConnect(nativeObserver, networkInfo);
    }
  }

  private void notifyObserversOfNetworkDisconnect(long networkHandle) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (Long nativeObserver : nativeObservers) {
      nativeNotifyOfNetworkDisconnect(nativeObserver, networkHandle);
    }
  }

  private void notifyObserversOfNetworkPreference(
      List<NetworkChangeDetector.ConnectionType> types, int preference) {
    List<Long> nativeObservers = getNativeNetworkObserversSync();
    for (NetworkChangeDetector.ConnectionType type : types) {
      for (Long nativeObserver : nativeObservers) {
        nativeNotifyOfNetworkPreference(nativeObserver, type, preference);
      }
    }
  }

  private void updateObserverActiveNetworkList(long nativeObserver) {
    List<NetworkChangeDetector.NetworkInformation> networkInfoList;
    synchronized (networkChangeDetectorLock) {
      networkInfoList =
          (networkChangeDetector == null) ? null : networkChangeDetector.getActiveNetworkList();
    }
    if (networkInfoList == null) {
      return;
    }

    NetworkChangeDetector.NetworkInformation[] networkInfos =
        new NetworkChangeDetector.NetworkInformation[networkInfoList.size()];
    networkInfos = networkInfoList.toArray(networkInfos);
    nativeNotifyOfActiveNetworkList(nativeObserver, networkInfos);
  }

  private List<Long> getNativeNetworkObserversSync() {
    synchronized (nativeNetworkObservers) {
      return new ArrayList<>(nativeNetworkObservers);
    }
  }

  @Deprecated
  public static void addNetworkObserver(NetworkObserver observer) {
    getInstance().addObserver(observer);
  }

  public void addObserver(NetworkObserver observer) {
    synchronized (networkObservers) {
      networkObservers.add(observer);
    }
  }

  @Deprecated
  public static void removeNetworkObserver(NetworkObserver observer) {
    getInstance().removeObserver(observer);
  }

  public void removeObserver(NetworkObserver observer) {
    synchronized (networkObservers) {
      networkObservers.remove(observer);
    }
  }

  public static boolean isOnline() {
    NetworkChangeDetector.ConnectionType connectionType = getInstance().getCurrentConnectionType();
    return connectionType != NetworkChangeDetector.ConnectionType.CONNECTION_NONE;
  }

  private native void nativeNotifyConnectionTypeChanged(long nativeAndroidNetworkMonitor);

  private native void nativeNotifyOfNetworkConnect(
      long nativeAndroidNetworkMonitor, NetworkChangeDetector.NetworkInformation networkInfo);

  private native void nativeNotifyOfNetworkDisconnect(
      long nativeAndroidNetworkMonitor, long networkHandle);

  private native void nativeNotifyOfActiveNetworkList(
      long nativeAndroidNetworkMonitor, NetworkChangeDetector.NetworkInformation[] networkInfos);

  private native void nativeNotifyOfNetworkPreference(
      long nativeAndroidNetworkMonitor, NetworkChangeDetector.ConnectionType type, int preference);

  @Nullable
  NetworkChangeDetector getNetworkChangeDetector() {
    synchronized (networkChangeDetectorLock) {
      return networkChangeDetector;
    }
  }

  int getNumObservers() {
    synchronized (networkChangeDetectorLock) {
      return numObservers;
    }
  }

  static NetworkMonitorAutoDetect createAndSetAutoDetectForTest(
      Context context, String fieldTrialsString) {
    NetworkMonitor networkMonitor = getInstance();
    NetworkChangeDetector networkChangeDetector =
        networkMonitor.createNetworkChangeDetector(context, fieldTrialsString);
    networkMonitor.networkChangeDetector = networkChangeDetector;
    return (NetworkMonitorAutoDetect) networkChangeDetector;
  }
}
