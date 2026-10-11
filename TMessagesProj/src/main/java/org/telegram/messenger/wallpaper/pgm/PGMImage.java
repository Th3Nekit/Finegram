package org.telegram.messenger.wallpaper.pgm;

import android.graphics.Bitmap;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.List;

public final class PGMImage {

    private PGMImage() {}

    public static void write(Bitmap bmp, OutputStream out) throws IOException {
        write(bmp, out, null);
    }

    public static void write(Bitmap bmp, OutputStream out, List<String> comments) throws IOException {
        if (bmp.getConfig() != Bitmap.Config.ALPHA_8) {
            throw new IllegalArgumentException("Only Bitmap.Config.ALPHA_8 is supported");
        }

        final int w = bmp.getWidth();
        final int h = bmp.getHeight();

        out.write("P5\n".getBytes(StandardCharsets.US_ASCII));
        if (comments != null && !comments.isEmpty()) {
            for (String c : comments) {

                String line = c == null ? "" : c.replace('\r', ' ').replace('\n', ' ');
                out.write(("#" + line + "\n").getBytes(StandardCharsets.US_ASCII));
            }
        }
        out.write((w + " " + h + "\n").getBytes(StandardCharsets.US_ASCII));
        out.write("255\n".getBytes(StandardCharsets.US_ASCII));

        int rowBytes = bmp.getRowBytes();
        byte[] src = new byte[rowBytes * h];
        bmp.copyPixelsToBuffer(ByteBuffer.wrap(src));

        int srcPos = 0;
        for (int y = 0; y < h; y++, srcPos += rowBytes) {
            out.write(src, srcPos, w);
        }
    }

    public static Bitmap read(InputStream in) throws IOException {
        return read(in, null);
    }

    public static Bitmap read(InputStream in, List<String> comments) throws IOException {
        BufferedInputStream bis = new BufferedInputStream(in);

        String magic = nextToken(bis, comments);
        if (!"P5".equals(magic)) {
            throw new IOException("Not a binary PGM (P5), got: " + magic);
        }

        int width  = parsePositiveInt(nextNonCommentToken(bis, comments), "width");
        int height = parsePositiveInt(nextNonCommentToken(bis, comments), "height");
        int maxval = parsePositiveInt(nextNonCommentToken(bis, comments), "maxval");
        if (maxval != 255) {
            throw new IOException("Only 8-bit PGM supported (maxval=255), got: " + maxval);
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8);
        int rowBytes = bitmap.getRowBytes();
        byte[] dst = new byte[rowBytes * height];
        ByteBuffer bb = ByteBuffer.wrap(dst);

        byte[] row = new byte[width];
        int dstPos = 0;
        for (int y = 0; y < height; y++) {
            readFully(bis, row, 0, width);
            System.arraycopy(row, 0, dst, dstPos, width);
            dstPos += rowBytes;
        }
        bitmap.copyPixelsFromBuffer(bb);

        return bitmap;
    }

    private static int parsePositiveInt(String s, String what) throws IOException {
        try {
            int v = Integer.parseInt(s);
            if (v <= 0) throw new IOException("Invalid " + what + ": " + v);
            return v;
        } catch (NumberFormatException e) {
            throw new IOException("Invalid " + what + ": " + s, e);
        }
    }

    private static String nextToken(BufferedInputStream in, List<String> comments) throws IOException {
        StringBuilder sb = new StringBuilder();
        while (true) {
            in.mark(1);
            int c = in.read();
            if (c == -1) return (sb.length() == 0) ? null : sb.toString();

            if (Character.isWhitespace(c)) {
                if (sb.length() > 0) return sb.toString();
                continue;
            }

            if (c == '#') {
                String line = readLineAscii(in);
                if (comments != null) {
                    comments.add(line);
                }
                if (sb.length() > 0) return sb.toString();
                continue;
            }

            sb.append((char) c);
            while (true) {
                in.mark(1);
                c = in.read();
                if (c == -1 || Character.isWhitespace(c)) return sb.toString();
                if (c == '#') {
                    in.reset();
                    return sb.toString();
                }
                sb.append((char) c);
            }
        }
    }

    private static String nextNonCommentToken(BufferedInputStream in, List<String> comments) throws IOException {
        String tok;
        while (true) {
            tok = nextToken(in, comments);
            if (tok == null) throw new IOException("Unexpected EOF in header");

            if (!tok.startsWith("#")) return tok;
        }
    }

    private static String readLineAscii(InputStream in) throws IOException {
        StringBuilder sb = new StringBuilder();
        int c;
        while ((c = in.read()) != -1) {
            if (c == '\n') break;
            if (c != '\r') sb.append((char) c);
        }
        return sb.toString();
    }

    private static void readFully(InputStream in, byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) {
            int r = in.read(b, off + n, len - n);
            if (r < 0) throw new IOException("Unexpected EOF");
            n += r;
        }
    }
}
