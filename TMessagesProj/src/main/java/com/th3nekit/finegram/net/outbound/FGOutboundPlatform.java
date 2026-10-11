/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;

import org.telegram.messenger.ApplicationLoader;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import io.nekohasekai.libbox.ExchangeContext;
import io.nekohasekai.libbox.Libbox;
import io.nekohasekai.libbox.AutoRedirectHandler;
import io.nekohasekai.libbox.AutoRedirectSession;
import io.nekohasekai.libbox.BridgeOptions;
import io.nekohasekai.libbox.BridgeSession;
import io.nekohasekai.libbox.ConnectionOwner;
import io.nekohasekai.libbox.InterfaceUpdateListener;
import io.nekohasekai.libbox.LocalDNSTransport;
import io.nekohasekai.libbox.NeighborUpdateListener;
import io.nekohasekai.libbox.NetworkInterface;
import io.nekohasekai.libbox.NetworkInterfaceIterator;
import io.nekohasekai.libbox.Notification;
import io.nekohasekai.libbox.PlatformInterface;
import io.nekohasekai.libbox.PlatformUser;
import io.nekohasekai.libbox.ShellSession;
import io.nekohasekai.libbox.StringIterator;
import io.nekohasekai.libbox.TunOptions;
import io.nekohasekai.libbox.WIFIState;

final class FGOutboundPlatform implements PlatformInterface {

    private final ConnectivityManager connectivity = (ConnectivityManager)
            ApplicationLoader.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    private final Map<InterfaceUpdateListener, Monitor> monitors = new HashMap<>();

    @Override
    public int openTun(TunOptions options) throws Exception {

        throw new UnsupportedOperationException("tun is not used");
    }

    @Override
    public boolean usePlatformAutoDetectInterfaceControl() {
        return false;
    }

    @Override
    public void autoDetectInterfaceControl(int fd) {
    }

    @Override
    public NetworkInterfaceIterator getInterfaces() {
        final ArrayList<NetworkInterface> interfaces = new ArrayList<>();
        final Map<String, LinkProperties> links = new HashMap<>();
        final Map<String, NetworkCapabilities> capabilities = new HashMap<>();
        if (connectivity != null) {
            for (Network network : connectivity.getAllNetworks()) {
                LinkProperties properties = connectivity.getLinkProperties(network);
                if (properties == null || properties.getInterfaceName() == null) continue;
                links.put(properties.getInterfaceName(), properties);
                capabilities.put(properties.getInterfaceName(), connectivity.getNetworkCapabilities(network));
            }
        }
        try {
            Enumeration<java.net.NetworkInterface> enumeration = java.net.NetworkInterface.getNetworkInterfaces();
            while (enumeration != null && enumeration.hasMoreElements()) {
                java.net.NetworkInterface item = enumeration.nextElement();
                NetworkInterface target = new NetworkInterface();
                target.setName(item.getName());
                target.setIndex(item.getIndex());
                target.setMTU(item.getMTU());
                int flags = 0;
                if (item.isUp()) flags |= 1 | 64;
                if (item.isLoopback()) flags |= 8;
                if (item.isPointToPoint()) flags |= 16;
                if (item.supportsMulticast()) flags |= 4096;
                target.setFlags(flags);
                List<String> addresses = new ArrayList<>();
                for (InterfaceAddress address : item.getInterfaceAddresses()) {
                    addresses.add(addressText(address.getAddress()) + "/" + address.getNetworkPrefixLength());
                }
                target.setAddresses(new Strings(addresses));
                LinkProperties properties = links.get(item.getName());
                List<String> dns = new ArrayList<>();
                if (properties != null) {
                    for (InetAddress address : properties.getDnsServers()) dns.add(addressText(address));
                }
                target.setDNSServer(new Strings(dns));
                String domains = properties == null ? null : properties.getDomains();
                target.setDNSSearchDomain(new Strings(domains == null || domains.isEmpty()
                        ? Collections.emptyList() : java.util.Arrays.asList(domains.split("\\s+"))));
                target.setGateway(new Strings(Collections.emptyList()));
                NetworkCapabilities networkCapabilities = capabilities.get(item.getName());
                int type = Libbox.InterfaceTypeOther;
                if (networkCapabilities != null) {
                    if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) type = Libbox.InterfaceTypeWIFI;
                    else if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) type = Libbox.InterfaceTypeCellular;
                    else if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) type = Libbox.InterfaceTypeEthernet;
                    target.setMetered(!networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
                }
                target.setType(type);
                interfaces.add(target);
            }
        } catch (Exception ignored) {
        }
        return new NetworkInterfaceIterator() {
            private int index;

            @Override
            public boolean hasNext() {
                return index < interfaces.size();
            }

            @Override
            public NetworkInterface next() {
                return interfaces.get(index++);
            }
        };
    }

    @Override
    public synchronized void startDefaultInterfaceMonitor(InterfaceUpdateListener listener) {
        if (connectivity == null || monitors.containsKey(listener)) return;
        Monitor monitor = new Monitor(listener);
        monitors.put(listener, monitor);
        try {
            connectivity.registerDefaultNetworkCallback(monitor);
            monitor.registered = true;
            monitor.publish(connectivity.getActiveNetwork());
        } catch (RuntimeException error) {
            closeDefaultInterfaceMonitor(listener);
            throw error;
        }
    }

    @Override
    public synchronized void closeDefaultInterfaceMonitor(InterfaceUpdateListener listener) {
        Monitor monitor = monitors.remove(listener);
        if (monitor == null) return;
        monitor.closed = true;
        if (monitor.registered) {
            try {
                connectivity.unregisterNetworkCallback(monitor);
            } catch (RuntimeException ignored) {
            }
        }
    }

    @Override
    public void startNeighborMonitor(NeighborUpdateListener listener) {
    }

    @Override
    public void closeNeighborMonitor(NeighborUpdateListener listener) {
    }

    @Override
    public boolean useProcFS() {
        return false;
    }

    @Override
    public ConnectionOwner findConnectionOwner(int protocol, String sourceAddress, int sourcePort,
                                               String destinationAddress, int destinationPort) throws Exception {
        throw new UnsupportedOperationException("connection owner lookup is not used");
    }

    @Override
    public boolean includeAllNetworks() {
        return false;
    }

    @Override
    public boolean underNetworkExtension() {
        return false;
    }

    @Override
    public LocalDNSTransport localDNSTransport() {
        return new LocalDNSTransport() {
            @Override
            public boolean raw() {
                return false;
            }

            @Override
            public void lookup(ExchangeContext context, String family, String domain) {
                try {
                    Network network = connectivity == null ? null : connectivity.getActiveNetwork();
                    InetAddress[] addresses = network == null
                            ? InetAddress.getAllByName(domain) : network.getAllByName(domain);
                    StringBuilder result = new StringBuilder();
                    for (InetAddress address : addresses) {
                        if ("ip4".equals(family) && !(address instanceof Inet4Address)
                                || "ip6".equals(family) && !(address instanceof Inet6Address)) continue;
                        if (result.length() > 0) result.append('\n');
                        result.append(addressText(address));
                    }
                    context.success(result.toString());
                } catch (UnknownHostException error) {
                    context.errorCode(3);
                } catch (Exception error) {
                    context.errorCode(2);
                }
            }

            @Override
            public void exchange(ExchangeContext context, byte[] message) {
                context.errorCode(4);
            }
        };
    }

    @Override
    public WIFIState readWIFIState() {
        return null;
    }

    @Override
    public void clearDNSCache() {
    }

    @Override
    public void registerMyInterface(String name) {
    }

    @Override
    public void sendNotification(Notification notification) {
    }

    @Override
    public void cancelNotification(String identifier, int id) {
    }

    @Override
    public boolean usePlatformAutoRedirect() {
        return false;
    }

    @Override
    public AutoRedirectSession createAutoRedirect(byte[] options, AutoRedirectHandler handler) throws Exception {
        throw new UnsupportedOperationException("auto redirect is not used");
    }

    @Override
    public boolean usePlatformBridge() {
        return false;
    }

    @Override
    public BridgeSession createBridge(BridgeOptions options) throws Exception {
        throw new UnsupportedOperationException("bridge is not used");
    }

    @Override
    public boolean usePlatformShell() {
        return false;
    }

    @Override
    public void checkPlatformShell() throws Exception {
        throw new UnsupportedOperationException("shell is not used");
    }

    @Override
    public ShellSession openShellSession(PlatformUser user, String path, StringIterator args,
                                         String workingDirectory, int rows, int columns) throws Exception {
        throw new UnsupportedOperationException("shell is not used");
    }

    @Override
    public PlatformUser lookupUser(String name) throws Exception {
        throw new UnsupportedOperationException("user lookup is not used");
    }

    @Override
    public String lookupSFTPServer() throws Exception {
        throw new UnsupportedOperationException("sftp is not used");
    }

    @Override
    public String readSystemSSHHostKey() throws Exception {
        throw new UnsupportedOperationException("ssh is not used");
    }

    @Override
    public String tailscaleHostname() {
        return "";
    }

    private static String addressText(InetAddress address) {
        String text = address.getHostAddress();
        int scope = text.indexOf('%');
        return scope < 0 ? text : text.substring(0, scope);
    }

    private final class Monitor extends ConnectivityManager.NetworkCallback {
        private final InterfaceUpdateListener listener;
        private volatile boolean closed;
        private boolean registered;
        private Network network;

        Monitor(InterfaceUpdateListener listener) {
            this.listener = listener;
        }

        void publish(Network selected) {
            if (closed) return;
            network = selected;
            LinkProperties properties = selected == null ? null : connectivity.getLinkProperties(selected);
            String name = properties == null ? null : properties.getInterfaceName();
            try {
                java.net.NetworkInterface item = name == null ? null : java.net.NetworkInterface.getByName(name);
                NetworkCapabilities capabilities = selected == null ? null : connectivity.getNetworkCapabilities(selected);
                boolean metered = capabilities != null
                        && !capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);
                listener.updateDefaultInterface(item == null ? "" : name, item == null ? -1 : item.getIndex(), metered, false);
            } catch (Exception ignored) {
            }
        }

        @Override
        public void onAvailable(Network network) {
            publish(network);
        }

        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties properties) {
            publish(network);
        }

        @Override
        public void onCapabilitiesChanged(Network network, NetworkCapabilities capabilities) {
            publish(network);
        }

        @Override
        public void onLost(Network lost) {
            if (!lost.equals(network)) return;
            Network active = connectivity.getActiveNetwork();
            publish(lost.equals(active) ? null : active);
        }
    }

    private static final class Strings implements StringIterator {
        private final List<String> values;
        private int index;

        Strings(List<String> values) {
            this.values = values;
        }

        @Override
        public boolean hasNext() {
            return index < values.size();
        }

        @Override
        public String next() {
            return values.get(index++);
        }

        @Override
        public int len() {
            return values.size();
        }
    }
}
