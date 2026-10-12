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

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Build;
import androidx.annotation.Nullable;
import android.telephony.TelephonyManager;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class NetworkMonitorAutoDetect extends BroadcastReceiver implements NetworkChangeDetector {
  static class NetworkState {
    private final boolean connected;

    private final int type;

    private final int subtype;

    private final int underlyingNetworkTypeForVpn;
    private final int underlyingNetworkSubtypeForVpn;

    public NetworkState(boolean connected, int type, int subtype, int underlyingNetworkTypeForVpn,
        int underlyingNetworkSubtypeForVpn) {
      this.connected = connected;
      this.type = type;
      this.subtype = subtype;
      this.underlyingNetworkTypeForVpn = underlyingNetworkTypeForVpn;
      this.underlyingNetworkSubtypeForVpn = underlyingNetworkSubtypeForVpn;
    }

    public boolean isConnected() {
      return connected;
    }

    public int getNetworkType() {
      return type;
    }

    public int getNetworkSubType() {
      return subtype;
    }

    public int getUnderlyingNetworkTypeForVpn() {
      return underlyingNetworkTypeForVpn;
    }

    public int getUnderlyingNetworkSubtypeForVpn() {
      return underlyingNetworkSubtypeForVpn;
    }
  }

  @SuppressLint("NewApi")
  private class SimpleNetworkCallback extends NetworkCallback {
    @Override
    public void onAvailable(Network network) {
      Logging.d(TAG, "Network becomes available: " + network.toString());
      onNetworkChanged(network);
    }

    @Override
    public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {

      Logging.d(TAG, "capabilities changed: " + networkCapabilities.toString());
      onNetworkChanged(network);
    }

    @Override
    public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {

      Logging.d(TAG, "link properties changed");
      onNetworkChanged(network);
    }

    @Override
    public void onLosing(Network network, int maxMsToLive) {

      Logging.d(
          TAG, "Network " + network.toString() + " is about to lose in " + maxMsToLive + "ms");
    }

    @Override
    public void onLost(Network network) {
      Logging.d(TAG, "Network " + network.toString() + " is disconnected");
      observer.onNetworkDisconnect(networkToNetId(network));
    }

    private void onNetworkChanged(Network network) {
      NetworkInformation networkInformation = connectivityManagerDelegate.networkToInfo(network);
      if (networkInformation != null) {
        observer.onNetworkConnect(networkInformation);
      }
    }
  }

  static class ConnectivityManagerDelegate {

    @Nullable private final ConnectivityManager connectivityManager;

    ConnectivityManagerDelegate(Context context) {
      connectivityManager =
          (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    ConnectivityManagerDelegate() {

      connectivityManager = null;
    }

    NetworkState getNetworkState() {
      if (connectivityManager == null) {
        return new NetworkState(false, -1, -1, -1, -1);
      }
      return getNetworkState(connectivityManager.getActiveNetworkInfo());
    }

    @SuppressLint("NewApi")
    NetworkState getNetworkState(@Nullable Network network) {
      if (network == null || connectivityManager == null) {
        return new NetworkState(false, -1, -1, -1, -1);
      }
      NetworkInfo networkInfo = connectivityManager.getNetworkInfo(network);
      if (networkInfo == null) {
        Logging.w(TAG, "Couldn't retrieve information from network " + network.toString());
        return new NetworkState(false, -1, -1, -1, -1);
      }

      if (networkInfo.getType() != ConnectivityManager.TYPE_VPN) {

        NetworkCapabilities networkCapabilities =
            connectivityManager.getNetworkCapabilities(network);
        if (networkCapabilities == null
            || !networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
          return getNetworkState(networkInfo);
        }

        return new NetworkState(networkInfo.isConnected(), ConnectivityManager.TYPE_VPN, -1,
            networkInfo.getType(), networkInfo.getSubtype());
      }

      if (networkInfo.getType() == ConnectivityManager.TYPE_VPN) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
            && network.equals(connectivityManager.getActiveNetwork())) {

          NetworkInfo underlyingActiveNetworkInfo = connectivityManager.getActiveNetworkInfo();

          if (underlyingActiveNetworkInfo != null
              && underlyingActiveNetworkInfo.getType() != ConnectivityManager.TYPE_VPN) {
            return new NetworkState(networkInfo.isConnected(), ConnectivityManager.TYPE_VPN, -1,
                underlyingActiveNetworkInfo.getType(), underlyingActiveNetworkInfo.getSubtype());
          }
        }
        return new NetworkState(
            networkInfo.isConnected(), ConnectivityManager.TYPE_VPN, -1, -1, -1);
      }

      return getNetworkState(networkInfo);
    }

    private NetworkState getNetworkState(@Nullable NetworkInfo networkInfo) {
      if (networkInfo == null || !networkInfo.isConnected()) {
        return new NetworkState(false, -1, -1, -1, -1);
      }
      return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype(), -1, -1);
    }

    @SuppressLint("NewApi")
    Network[] getAllNetworks() {
      if (connectivityManager == null) {
        return new Network[0];
      }
      return connectivityManager.getAllNetworks();
    }

    @Nullable
    List<NetworkInformation> getActiveNetworkList() {
      if (!supportNetworkCallback()) {
        return null;
      }
      ArrayList<NetworkInformation> netInfoList = new ArrayList<NetworkInformation>();
      for (Network network : getAllNetworks()) {
        NetworkInformation info = networkToInfo(network);
        if (info != null) {
          netInfoList.add(info);
        }
      }
      return netInfoList;
    }

    @SuppressLint("NewApi")
    long getDefaultNetId() {
      if (!supportNetworkCallback()) {
        return INVALID_NET_ID;
      }

      final NetworkInfo defaultNetworkInfo = connectivityManager.getActiveNetworkInfo();
      if (defaultNetworkInfo == null) {
        return INVALID_NET_ID;
      }
      final Network[] networks = getAllNetworks();
      long defaultNetId = INVALID_NET_ID;
      for (Network network : networks) {
        if (!hasInternetCapability(network)) {
          continue;
        }
        final NetworkInfo networkInfo = connectivityManager.getNetworkInfo(network);
        if (networkInfo != null && networkInfo.getType() == defaultNetworkInfo.getType()) {

          if (defaultNetId != INVALID_NET_ID) {
            throw new RuntimeException(
                "Multiple connected networks of same type are not supported.");
          }
          defaultNetId = networkToNetId(network);
        }
      }
      return defaultNetId;
    }

    @SuppressLint("NewApi")
    private @Nullable NetworkInformation networkToInfo(@Nullable Network network) {
      if (network == null || connectivityManager == null) {
        return null;
      }
      LinkProperties linkProperties = connectivityManager.getLinkProperties(network);

      if (linkProperties == null) {
        Logging.w(TAG, "Detected unknown network: " + network.toString());
        return null;
      }
      if (linkProperties.getInterfaceName() == null) {
        Logging.w(TAG, "Null interface name for network " + network.toString());
        return null;
      }

      NetworkState networkState = getNetworkState(network);
      NetworkChangeDetector.ConnectionType connectionType = getConnectionType(networkState);
      if (connectionType == NetworkChangeDetector.ConnectionType.CONNECTION_NONE) {

        Logging.d(TAG, "Network " + network.toString() + " is disconnected");
        return null;
      }

      if (connectionType == NetworkChangeDetector.ConnectionType.CONNECTION_UNKNOWN
          || connectionType == NetworkChangeDetector.ConnectionType.CONNECTION_UNKNOWN_CELLULAR) {
        Logging.d(TAG, "Network " + network.toString() + " connection type is " + connectionType
                + " because it has type " + networkState.getNetworkType() + " and subtype "
                + networkState.getNetworkSubType());
      }

      ConnectionType underlyingConnectionTypeForVpn =
          getUnderlyingConnectionTypeForVpn(networkState);

      NetworkInformation networkInformation = new NetworkInformation(
          linkProperties.getInterfaceName(), connectionType, underlyingConnectionTypeForVpn,
          networkToNetId(network), getIPAddresses(linkProperties));
      return networkInformation;
    }

    @SuppressLint("NewApi")
    boolean hasInternetCapability(Network network) {
      if (connectivityManager == null) {
        return false;
      }
      final NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
      return capabilities != null
          && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
    }

    @SuppressLint("NewApi")
    public void registerNetworkCallback(NetworkCallback networkCallback) {
      connectivityManager.registerNetworkCallback(
          new NetworkRequest.Builder()
              .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
              .build(),
          networkCallback);
    }

    @SuppressLint("NewApi")
    public void requestMobileNetwork(NetworkCallback networkCallback) {
      NetworkRequest.Builder builder = new NetworkRequest.Builder();
      builder.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
          .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
      connectivityManager.requestNetwork(builder.build(), networkCallback);
    }

    @SuppressLint("NewApi")
    IPAddress[] getIPAddresses(LinkProperties linkProperties) {
      IPAddress[] ipAddresses = new IPAddress[linkProperties.getLinkAddresses().size()];
      int i = 0;
      for (LinkAddress linkAddress : linkProperties.getLinkAddresses()) {
        ipAddresses[i] = new IPAddress(linkAddress.getAddress().getAddress());
        ++i;
      }
      return ipAddresses;
    }

    @SuppressLint("NewApi")
    public void releaseCallback(NetworkCallback networkCallback) {
      if (supportNetworkCallback()) {
        Logging.d(TAG, "Unregister network callback");
        connectivityManager.unregisterNetworkCallback(networkCallback);
      }
    }

    public boolean supportNetworkCallback() {
      return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && connectivityManager != null;
    }
  }

  static class WifiManagerDelegate {
    @Nullable private final Context context;
    WifiManagerDelegate(Context context) {
      this.context = context;
    }

    WifiManagerDelegate() {

      context = null;
    }

    String getWifiSSID() {
      final Intent intent;
      if (Build.VERSION.SDK_INT >= 33) {
        intent = context.registerReceiver(
                null, new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION), Context.RECEIVER_NOT_EXPORTED);
      } else {
        intent = context.registerReceiver(
                null, new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
      }
      if (intent != null) {
        final WifiInfo wifiInfo = intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
        if (wifiInfo != null) {
          final String ssid = wifiInfo.getSSID();
          if (ssid != null) {
            return ssid;
          }
        }
      }
      return "";
    }
  }

  static class WifiDirectManagerDelegate extends BroadcastReceiver {

    private static final int WIFI_P2P_NETWORK_HANDLE = 0;
    private final Context context;
    private final NetworkChangeDetector.Observer observer;

    @Nullable private NetworkInformation wifiP2pNetworkInfo;

    WifiDirectManagerDelegate(NetworkChangeDetector.Observer observer, Context context) {
      this.context = context;
      this.observer = observer;
      IntentFilter intentFilter = new IntentFilter();
      intentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
      intentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
      if (Build.VERSION.SDK_INT >= 33) {
        context.registerReceiver(this, intentFilter, Context.RECEIVER_NOT_EXPORTED);
      } else {
        context.registerReceiver(this, intentFilter);
      }
      if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {

        WifiP2pManager manager =
            (WifiP2pManager) context.getSystemService(Context.WIFI_P2P_SERVICE);
        WifiP2pManager.Channel channel =
            manager.initialize(context, context.getMainLooper(), null               );
        manager.requestGroupInfo(channel, wifiP2pGroup -> { onWifiP2pGroupChange(wifiP2pGroup); });
      }
    }

    @Override
    @SuppressLint("InlinedApi")
    public void onReceive(Context context, Intent intent) {
      if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(intent.getAction())) {
        WifiP2pGroup wifiP2pGroup = intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_GROUP);
        onWifiP2pGroupChange(wifiP2pGroup);
      } else if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(intent.getAction())) {
        int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, 0                         );
        onWifiP2pStateChange(state);
      }
    }

    public void release() {
      context.unregisterReceiver(this);
    }

    public List<NetworkInformation> getActiveNetworkList() {
      if (wifiP2pNetworkInfo != null) {
        return Collections.singletonList(wifiP2pNetworkInfo);
      }

      return Collections.emptyList();
    }

    private void onWifiP2pGroupChange(@Nullable WifiP2pGroup wifiP2pGroup) {
      if (wifiP2pGroup == null || wifiP2pGroup.getInterface() == null) {
        return;
      }

      NetworkInterface wifiP2pInterface;
      try {
        wifiP2pInterface = NetworkInterface.getByName(wifiP2pGroup.getInterface());
      } catch (SocketException e) {
        Logging.e(TAG, "Unable to get WifiP2p network interface", e);
        return;
      }

      List<InetAddress> interfaceAddresses = Collections.list(wifiP2pInterface.getInetAddresses());
      IPAddress[] ipAddresses = new IPAddress[interfaceAddresses.size()];
      for (int i = 0; i < interfaceAddresses.size(); ++i) {
        ipAddresses[i] = new IPAddress(interfaceAddresses.get(i).getAddress());
      }

      wifiP2pNetworkInfo = new NetworkInformation(wifiP2pGroup.getInterface(),
          NetworkChangeDetector.ConnectionType.CONNECTION_WIFI,
          NetworkChangeDetector.ConnectionType.CONNECTION_NONE, WIFI_P2P_NETWORK_HANDLE,
          ipAddresses);
      observer.onNetworkConnect(wifiP2pNetworkInfo);
    }

    private void onWifiP2pStateChange(int state) {
      if (state == WifiP2pManager.WIFI_P2P_STATE_DISABLED) {
        wifiP2pNetworkInfo = null;
        observer.onNetworkDisconnect(WIFI_P2P_NETWORK_HANDLE);
      }
    }
  }

  private static final long INVALID_NET_ID = -1;
  private static final String TAG = "NetworkMonitorAutoDetect";

  private final NetworkChangeDetector.Observer observer;
  private final IntentFilter intentFilter;
  private final Context context;

  @Nullable private final NetworkCallback mobileNetworkCallback;

  @Nullable private final NetworkCallback allNetworkCallback;

  private ConnectivityManagerDelegate connectivityManagerDelegate;
  private WifiManagerDelegate wifiManagerDelegate;
  private WifiDirectManagerDelegate wifiDirectManagerDelegate;

  private boolean isRegistered;
  private NetworkChangeDetector.ConnectionType connectionType;
  private String wifiSSID;

  @SuppressLint("NewApi")
  public NetworkMonitorAutoDetect(NetworkChangeDetector.Observer observer, Context context) {
    this.observer = observer;
    this.context = context;
    connectivityManagerDelegate = new ConnectivityManagerDelegate(context);
    wifiManagerDelegate = new WifiManagerDelegate(context);

    final NetworkState networkState = connectivityManagerDelegate.getNetworkState();
    connectionType = getConnectionType(networkState);
    wifiSSID = getWifiSSID(networkState);
    intentFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);

    if (PeerConnectionFactory.fieldTrialsFindFullName("IncludeWifiDirect").equals("Enabled")) {
      wifiDirectManagerDelegate = new WifiDirectManagerDelegate(observer, context);
    }

    registerReceiver();
    if (connectivityManagerDelegate.supportNetworkCallback()) {

      NetworkCallback tempNetworkCallback = new NetworkCallback();
      try {
        connectivityManagerDelegate.requestMobileNetwork(tempNetworkCallback);
      } catch (java.lang.SecurityException e) {
        Logging.w(TAG, "Unable to obtain permission to request a cellular network.");
        tempNetworkCallback = null;
      }
      mobileNetworkCallback = tempNetworkCallback;
      allNetworkCallback = new SimpleNetworkCallback();
      connectivityManagerDelegate.registerNetworkCallback(allNetworkCallback);
    } else {
      mobileNetworkCallback = null;
      allNetworkCallback = null;
    }
  }

  @Override
  public boolean supportNetworkCallback() {
    return connectivityManagerDelegate.supportNetworkCallback();
  }

  void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
    connectivityManagerDelegate = delegate;
  }

  void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
    wifiManagerDelegate = delegate;
  }

  boolean isReceiverRegisteredForTesting() {
    return isRegistered;
  }

  @Override
  @Nullable
  public List<NetworkInformation> getActiveNetworkList() {
    List<NetworkInformation> connectivityManagerList =
        connectivityManagerDelegate.getActiveNetworkList();
    if (connectivityManagerList == null) {
      return null;
    }
    ArrayList<NetworkInformation> result =
        new ArrayList<NetworkInformation>(connectivityManagerList);
    if (wifiDirectManagerDelegate != null) {
      result.addAll(wifiDirectManagerDelegate.getActiveNetworkList());
    }
    return result;
  }

  @Override
  public void destroy() {
    if (allNetworkCallback != null) {
      connectivityManagerDelegate.releaseCallback(allNetworkCallback);
    }
    if (mobileNetworkCallback != null) {
      connectivityManagerDelegate.releaseCallback(mobileNetworkCallback);
    }
    if (wifiDirectManagerDelegate != null) {
      wifiDirectManagerDelegate.release();
    }
    unregisterReceiver();
  }

  private void registerReceiver() {
    if (isRegistered)
      return;

    isRegistered = true;
    if (Build.VERSION.SDK_INT >= 33) {
      context.registerReceiver(this, intentFilter, Context.RECEIVER_NOT_EXPORTED);
    } else {
      context.registerReceiver(this, intentFilter);
    }
  }

  private void unregisterReceiver() {
    if (!isRegistered)
      return;

    isRegistered = false;
    context.unregisterReceiver(this);
  }

  public NetworkState getCurrentNetworkState() {
    return connectivityManagerDelegate.getNetworkState();
  }

  public long getDefaultNetId() {
    return connectivityManagerDelegate.getDefaultNetId();
  }

  private static NetworkChangeDetector.ConnectionType getConnectionType(
      boolean isConnected, int networkType, int networkSubtype) {
    if (!isConnected) {
      return NetworkChangeDetector.ConnectionType.CONNECTION_NONE;
    }

    switch (networkType) {
      case ConnectivityManager.TYPE_ETHERNET:
        return NetworkChangeDetector.ConnectionType.CONNECTION_ETHERNET;
      case ConnectivityManager.TYPE_WIFI:
        return NetworkChangeDetector.ConnectionType.CONNECTION_WIFI;
      case ConnectivityManager.TYPE_WIMAX:
        return NetworkChangeDetector.ConnectionType.CONNECTION_4G;
      case ConnectivityManager.TYPE_BLUETOOTH:
        return NetworkChangeDetector.ConnectionType.CONNECTION_BLUETOOTH;
      case ConnectivityManager.TYPE_MOBILE:

        switch (networkSubtype) {
          case TelephonyManager.NETWORK_TYPE_GPRS:
          case TelephonyManager.NETWORK_TYPE_EDGE:
          case TelephonyManager.NETWORK_TYPE_CDMA:
          case TelephonyManager.NETWORK_TYPE_1xRTT:
          case TelephonyManager.NETWORK_TYPE_IDEN:
          case TelephonyManager.NETWORK_TYPE_GSM:
            return NetworkChangeDetector.ConnectionType.CONNECTION_2G;
          case TelephonyManager.NETWORK_TYPE_UMTS:
          case TelephonyManager.NETWORK_TYPE_EVDO_0:
          case TelephonyManager.NETWORK_TYPE_EVDO_A:
          case TelephonyManager.NETWORK_TYPE_HSDPA:
          case TelephonyManager.NETWORK_TYPE_HSUPA:
          case TelephonyManager.NETWORK_TYPE_HSPA:
          case TelephonyManager.NETWORK_TYPE_EVDO_B:
          case TelephonyManager.NETWORK_TYPE_EHRPD:
          case TelephonyManager.NETWORK_TYPE_HSPAP:
          case TelephonyManager.NETWORK_TYPE_TD_SCDMA:
            return NetworkChangeDetector.ConnectionType.CONNECTION_3G;
          case TelephonyManager.NETWORK_TYPE_LTE:
          case TelephonyManager.NETWORK_TYPE_IWLAN:
            return NetworkChangeDetector.ConnectionType.CONNECTION_4G;
          case TelephonyManager.NETWORK_TYPE_NR:
            return NetworkChangeDetector.ConnectionType.CONNECTION_5G;
          default:
            return NetworkChangeDetector.ConnectionType.CONNECTION_UNKNOWN_CELLULAR;
        }
      case ConnectivityManager.TYPE_VPN:
        return NetworkChangeDetector.ConnectionType.CONNECTION_VPN;
      default:
        return NetworkChangeDetector.ConnectionType.CONNECTION_UNKNOWN;
    }
  }

  public static NetworkChangeDetector.ConnectionType getConnectionType(NetworkState networkState) {
    return getConnectionType(networkState.isConnected(), networkState.getNetworkType(),
        networkState.getNetworkSubType());
  }

  @Override
  public NetworkChangeDetector.ConnectionType getCurrentConnectionType() {
    return getConnectionType(getCurrentNetworkState());
  }

  private static NetworkChangeDetector.ConnectionType getUnderlyingConnectionTypeForVpn(
      NetworkState networkState) {
    if (networkState.getNetworkType() != ConnectivityManager.TYPE_VPN) {
      return NetworkChangeDetector.ConnectionType.CONNECTION_NONE;
    }
    return getConnectionType(networkState.isConnected(),
        networkState.getUnderlyingNetworkTypeForVpn(),
        networkState.getUnderlyingNetworkSubtypeForVpn());
  }

  private String getWifiSSID(NetworkState networkState) {
    if (getConnectionType(networkState) != NetworkChangeDetector.ConnectionType.CONNECTION_WIFI)
      return "";
    return wifiManagerDelegate.getWifiSSID();
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    final NetworkState networkState = getCurrentNetworkState();
    if (ConnectivityManager.CONNECTIVITY_ACTION.equals(intent.getAction())) {
      connectionTypeChanged(networkState);
    }
  }

  private void connectionTypeChanged(NetworkState networkState) {
    NetworkChangeDetector.ConnectionType newConnectionType = getConnectionType(networkState);
    String newWifiSSID = getWifiSSID(networkState);
    if (newConnectionType == connectionType && newWifiSSID.equals(wifiSSID))
      return;

    connectionType = newConnectionType;
    wifiSSID = newWifiSSID;
    Logging.d(TAG, "Network connectivity changed, type is: " + connectionType);
    observer.onConnectionTypeChanged(newConnectionType);
  }

  @SuppressLint("NewApi")
  private static long networkToNetId(Network network) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      return network.getNetworkHandle();
    }

    return Integer.parseInt(network.toString());
  }
}
