/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Ядро — sing-box (GPL-3.0, Copyright the sing-box authors,
 * https://github.com/SagerNet/sing-box).
 */

package com.th3nekit.finegram.net.outbound;

import java.security.SecureRandom;

public final class FGOutboundCredentials {
    private static final SecureRandom random = new SecureRandom();
    private static final FGOutboundCredentials blocked = new FGOutboundCredentials(0);

    public final int port;
    public final String username;
    public final String password;

    private FGOutboundCredentials(int port) {
        this.port = port;
        username = token();
        password = token();
    }

    public static FGOutboundCredentials create(int port) {
        if (port <= 0 || port > 65535) throw new IllegalArgumentException("invalid SOCKS port");
        return new FGOutboundCredentials(port);
    }

    public static FGOutboundCredentials blocked() {
        return blocked;
    }

    private static String token() {
        byte[] bytes = new byte[32];
        random.nextBytes(bytes);
        char[] encoded = new char[bytes.length * 2];
        final String hex = "0123456789abcdef";
        for (int i = 0; i < bytes.length; i++) {
            encoded[i * 2] = hex.charAt((bytes[i] & 0xff) >>> 4);
            encoded[i * 2 + 1] = hex.charAt(bytes[i] & 15);
        }
        return new String(encoded);
    }
}
