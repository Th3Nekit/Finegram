package com.th3nekit.finegram.ink;

import java.io.ByteArrayOutputStream;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECFieldFp;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.EllipticCurve;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Arrays;
import java.util.zip.DataFormatException;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

import javax.crypto.Cipher;
import javax.crypto.KeyAgreement;
import javax.crypto.Mac;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public final class FGInkCodec {

    public static final int TYPE_HELLO = 1;
    public static final int TYPE_INK = 2;
    public static final int TYPE_CAPSULE = 3;

    public static final char MARK = '⁣';
    private static final char NIBBLE_BASE = '︀';
    private static final byte MAGIC_A = 0x46;
    private static final byte MAGIC_B = 0x47;
    private static final int VERSION = 1;
    private static final int PUBLIC_KEY_SIZE = 33;
    private static final int NONCE_SIZE = 12;
    private static final int TAG_BITS = 128;
    private static final int FLAG_DEFLATE = 1;
    private static final int MAX_PLAIN = 64 * 1024;
    private static final byte[] SALT = "fg-ink-v1".getBytes(StandardCharsets.US_ASCII);
    private static final SecureRandom RANDOM = new SecureRandom();

    public static final class Frame {
        public final int type;
        public final byte[] bytes;
        public final int tailStart;

        Frame(int type, byte[] bytes, int tailStart) {
            this.type = type;
            this.bytes = bytes;
            this.tailStart = tailStart;
        }

        public byte[] publicKey() {
            return type == TYPE_HELLO ? Arrays.copyOfRange(bytes, 3, 3 + PUBLIC_KEY_SIZE) : null;
        }

        public int unlockAt() {
            if (type != TYPE_CAPSULE) {
                return 0;
            }
            return ((bytes[4] & 0xFF) << 24) | ((bytes[5] & 0xFF) << 16) | ((bytes[6] & 0xFF) << 8) | (bytes[7] & 0xFF);
        }
    }

    private FGInkCodec() {
    }

    public static String tail(byte[] frame) {
        StringBuilder out = new StringBuilder(frame.length * 2 + 1);
        out.append(MARK);
        for (byte b : frame) {
            out.append((char) (NIBBLE_BASE + ((b >> 4) & 0x0F)));
            out.append((char) (NIBBLE_BASE + (b & 0x0F)));
        }
        return out.toString();
    }

    public static int tailLength(int frameSize) {
        return frameSize * 2 + 1;
    }

    public static Frame find(CharSequence text) {
        if (text == null) {
            return null;
        }
        int end = text.length();
        int start = end;
        while (start > 0 && isNibble(text.charAt(start - 1))) {
            start--;
        }
        int count = end - start;
        if (count < 6 || (count & 1) != 0 || start == 0 || text.charAt(start - 1) != MARK) {
            return null;
        }
        byte[] bytes = new byte[count / 2];
        for (int i = 0; i < bytes.length; i++) {
            int hi = text.charAt(start + i * 2) - NIBBLE_BASE;
            int lo = text.charAt(start + i * 2 + 1) - NIBBLE_BASE;
            bytes[i] = (byte) ((hi << 4) | lo);
        }
        if (bytes[0] != MAGIC_A || bytes[1] != MAGIC_B || ((bytes[2] >> 4) & 0x0F) != VERSION) {
            return null;
        }
        int type = bytes[2] & 0x0F;
        if (type == TYPE_HELLO && bytes.length != 3 + PUBLIC_KEY_SIZE) {
            return null;
        }
        if (type == TYPE_INK && bytes.length < 4 + NONCE_SIZE + TAG_BITS / 8) {
            return null;
        }
        if (type == TYPE_CAPSULE && bytes.length < 8 + NONCE_SIZE + TAG_BITS / 8) {
            return null;
        }
        if (type != TYPE_HELLO && type != TYPE_INK && type != TYPE_CAPSULE) {
            return null;
        }
        return new Frame(type, bytes, start - 1);
    }

    private static boolean isNibble(char c) {
        return c >= NIBBLE_BASE && c <= NIBBLE_BASE + 0x0F;
    }

    public static byte[] hello(byte[] publicKey) {
        byte[] out = new byte[3 + PUBLIC_KEY_SIZE];
        out[0] = MAGIC_A;
        out[1] = MAGIC_B;
        out[2] = (byte) ((VERSION << 4) | TYPE_HELLO);
        System.arraycopy(publicKey, 0, out, 3, PUBLIC_KEY_SIZE);
        return out;
    }

    public static byte[] seal(byte[] key, int type, int unlockAt, String plain) throws GeneralSecurityException {
        byte[] data = plain.getBytes(StandardCharsets.UTF_8);
        int flags = 0;
        byte[] packed = deflate(data);
        if (packed.length < data.length) {
            data = packed;
            flags |= FLAG_DEFLATE;
        }
        int headerSize = type == TYPE_CAPSULE ? 8 : 4;
        byte[] header = new byte[headerSize];
        header[0] = MAGIC_A;
        header[1] = MAGIC_B;
        header[2] = (byte) ((VERSION << 4) | type);
        header[3] = (byte) flags;
        if (type == TYPE_CAPSULE) {
            header[4] = (byte) (unlockAt >>> 24);
            header[5] = (byte) (unlockAt >>> 16);
            header[6] = (byte) (unlockAt >>> 8);
            header[7] = (byte) unlockAt;
        }
        byte[] nonce = new byte[NONCE_SIZE];
        RANDOM.nextBytes(nonce);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"), new GCMParameterSpec(TAG_BITS, nonce));
        cipher.updateAAD(header);
        byte[] sealed = cipher.doFinal(data);
        byte[] out = new byte[headerSize + NONCE_SIZE + sealed.length];
        System.arraycopy(header, 0, out, 0, headerSize);
        System.arraycopy(nonce, 0, out, headerSize, NONCE_SIZE);
        System.arraycopy(sealed, 0, out, headerSize + NONCE_SIZE, sealed.length);
        return out;
    }

    public static int sealedSize(int type, String plain) {
        byte[] data = plain.getBytes(StandardCharsets.UTF_8);
        byte[] packed = deflate(data);
        int size = Math.min(data.length, packed.length);
        return (type == TYPE_CAPSULE ? 8 : 4) + NONCE_SIZE + size + TAG_BITS / 8;
    }

    public static String open(byte[] key, Frame frame) {
        if (frame == null || (frame.type != TYPE_INK && frame.type != TYPE_CAPSULE)) {
            return null;
        }
        int headerSize = frame.type == TYPE_CAPSULE ? 8 : 4;
        byte[] bytes = frame.bytes;
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"),
                    new GCMParameterSpec(TAG_BITS, bytes, headerSize, NONCE_SIZE));
            cipher.updateAAD(bytes, 0, headerSize);
            byte[] data = cipher.doFinal(bytes, headerSize + NONCE_SIZE, bytes.length - headerSize - NONCE_SIZE);
            if ((bytes[3] & FLAG_DEFLATE) != 0) {
                data = inflate(data);
                if (data == null) {
                    return null;
                }
            }
            return new String(data, StandardCharsets.UTF_8);
        } catch (GeneralSecurityException | IllegalArgumentException e) {
            return null;
        }
    }

    private static byte[] deflate(byte[] data) {
        Deflater deflater = new Deflater(Deflater.BEST_COMPRESSION, true);
        try {
            deflater.setInput(data);
            deflater.finish();
            ByteArrayOutputStream out = new ByteArrayOutputStream(data.length + 16);
            byte[] buffer = new byte[512];
            while (!deflater.finished()) {
                int n = deflater.deflate(buffer);
                out.write(buffer, 0, n);
            }
            return out.toByteArray();
        } finally {
            deflater.end();
        }
    }

    private static byte[] inflate(byte[] data) {
        Inflater inflater = new Inflater(true);
        try {
            byte[] input = Arrays.copyOf(data, data.length + 1);
            inflater.setInput(input);
            ByteArrayOutputStream out = new ByteArrayOutputStream(data.length * 3 + 16);
            byte[] buffer = new byte[1024];
            while (!inflater.finished()) {
                int n = inflater.inflate(buffer);
                if (n == 0 && (inflater.needsInput() || inflater.needsDictionary())) {
                    break;
                }
                out.write(buffer, 0, n);
                if (out.size() > MAX_PLAIN) {
                    return null;
                }
            }
            return inflater.finished() ? out.toByteArray() : null;
        } catch (DataFormatException e) {
            return null;
        } finally {
            inflater.end();
        }
    }

    public static KeyPair generateKeyPair() throws GeneralSecurityException {
        KeyPairGenerator generator = KeyPairGenerator.getInstance("EC");
        generator.initialize(new ECGenParameterSpec("secp256r1"), RANDOM);
        return generator.generateKeyPair();
    }

    public static PrivateKey privateKey(byte[] pkcs8) throws GeneralSecurityException {
        return KeyFactory.getInstance("EC").generatePrivate(new PKCS8EncodedKeySpec(pkcs8));
    }

    public static byte[] compress(ECPublicKey key) {
        ECPoint point = key.getW();
        byte[] out = new byte[PUBLIC_KEY_SIZE];
        out[0] = (byte) (point.getAffineY().testBit(0) ? 3 : 2);
        byte[] x = fixed(point.getAffineX(), 32);
        System.arraycopy(x, 0, out, 1, 32);
        return out;
    }

    public static PublicKey decompress(byte[] data, ECParameterSpec params) throws GeneralSecurityException {
        if (data == null || data.length != PUBLIC_KEY_SIZE || (data[0] != 2 && data[0] != 3)) {
            throw new GeneralSecurityException("bad point");
        }
        EllipticCurve curve = params.getCurve();
        BigInteger p = ((ECFieldFp) curve.getField()).getP();
        BigInteger x = new BigInteger(1, Arrays.copyOfRange(data, 1, PUBLIC_KEY_SIZE));
        if (x.compareTo(p) >= 0) {
            throw new GeneralSecurityException("bad point");
        }
        BigInteger rhs = x.pow(3).add(curve.getA().multiply(x)).add(curve.getB()).mod(p);
        BigInteger y = rhs.modPow(p.add(BigInteger.ONE).shiftRight(2), p);
        if (!y.modPow(BigInteger.valueOf(2), p).equals(rhs)) {
            throw new GeneralSecurityException("bad point");
        }
        if (y.testBit(0) != (data[0] == 3)) {
            y = p.subtract(y);
        }
        return KeyFactory.getInstance("EC").generatePublic(new ECPublicKeySpec(new ECPoint(x, y), params));
    }

    public static byte[] pairKey(PrivateKey own, PublicKey peer, long selfId, long peerId) throws GeneralSecurityException {
        KeyAgreement agreement = KeyAgreement.getInstance("ECDH");
        agreement.init(own);
        agreement.doPhase(peer, true);
        byte[] secret = agreement.generateSecret();
        long low = Math.min(selfId, peerId);
        long high = Math.max(selfId, peerId);
        byte[] info = ("fg-ink-pair:" + low + ":" + high).getBytes(StandardCharsets.US_ASCII);
        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(SALT, "HmacSHA256"));
        byte[] prk = mac.doFinal(secret);
        mac.init(new SecretKeySpec(prk, "HmacSHA256"));
        mac.update(info);
        mac.update((byte) 1);
        byte[] key = mac.doFinal();
        Arrays.fill(secret, (byte) 0);
        Arrays.fill(prk, (byte) 0);
        return key;
    }

    public static byte[] fingerprint(byte[] first, byte[] second) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            boolean ordered = compare(first, second) <= 0;
            digest.update(ordered ? first : second);
            digest.update(ordered ? second : first);
            return digest.digest();
        } catch (GeneralSecurityException e) {
            return new byte[32];
        }
    }

    private static int compare(byte[] a, byte[] b) {
        for (int i = 0; i < Math.min(a.length, b.length); i++) {
            int d = (a[i] & 0xFF) - (b[i] & 0xFF);
            if (d != 0) {
                return d;
            }
        }
        return a.length - b.length;
    }

    private static byte[] fixed(BigInteger value, int size) {
        byte[] raw = value.toByteArray();
        if (raw.length == size) {
            return raw;
        }
        byte[] out = new byte[size];
        if (raw.length > size) {
            System.arraycopy(raw, raw.length - size, out, 0, size);
        } else {
            System.arraycopy(raw, 0, out, size - raw.length, raw.length);
        }
        return out;
    }
}
