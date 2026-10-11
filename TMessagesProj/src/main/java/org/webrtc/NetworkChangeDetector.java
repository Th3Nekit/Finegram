/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import java.util.List;

public interface NetworkChangeDetector {

  public static enum ConnectionType {
    CONNECTION_UNKNOWN,
    CONNECTION_ETHERNET,
    CONNECTION_WIFI,
    CONNECTION_5G,
    CONNECTION_4G,
    CONNECTION_3G,
    CONNECTION_2G,
    CONNECTION_UNKNOWN_CELLULAR,
    CONNECTION_BLUETOOTH,
    CONNECTION_VPN,
    CONNECTION_NONE
  }

  public static class IPAddress {
    public final byte[] address;

    public IPAddress(byte[] address) {
      this.address = address;
    }

    @CalledByNative("IPAddress")
    private byte[] getAddress() {
      return address;
    }
  }

  public static class NetworkInformation {
    public final String name;
    public final ConnectionType type;

    public final ConnectionType underlyingTypeForVpn;
    public final long handle;
    public final IPAddress[] ipAddresses;

    public NetworkInformation(String name, ConnectionType type, ConnectionType underlyingTypeForVpn,
        long handle, IPAddress[] addresses) {
      this.name = name;
      this.type = type;
      this.underlyingTypeForVpn = underlyingTypeForVpn;
      this.handle = handle;
      this.ipAddresses = addresses;
    }

    @CalledByNative("NetworkInformation")
    private IPAddress[] getIpAddresses() {
      return ipAddresses;
    }

    @CalledByNative("NetworkInformation")
    private ConnectionType getConnectionType() {
      return type;
    }

    @CalledByNative("NetworkInformation")
    private ConnectionType getUnderlyingConnectionTypeForVpn() {
      return underlyingTypeForVpn;
    }

    @CalledByNative("NetworkInformation")
    private long getHandle() {
      return handle;
    }

    @CalledByNative("NetworkInformation")
    private String getName() {
      return name;
    }
  };

  public static interface Observer {

    public void onConnectionTypeChanged(ConnectionType newConnectionType);

    public void onNetworkConnect(NetworkInformation networkInfo);

    public void onNetworkDisconnect(long networkHandle);

    public void onNetworkPreference(List<ConnectionType> types, int preference);
  }

  public ConnectionType getCurrentConnectionType();

  public boolean supportNetworkCallback();

  @Nullable public List<NetworkInformation> getActiveNetworkList();

  public void destroy();
}
