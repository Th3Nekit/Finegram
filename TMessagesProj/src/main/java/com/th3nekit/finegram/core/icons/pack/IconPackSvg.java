/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.icons.pack;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.Xml;

import androidx.core.graphics.PathParser;

import org.xmlpull.v1.XmlPullParser;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;

public final class IconPackSvg {

    private IconPackSvg() {
    }

    public static boolean looksLikeSvg(String name, byte[] source) {
        if (name != null && name.toLowerCase().endsWith(".svg")) {
            return true;
        }
        if (source == null) {
            return false;
        }
        for (int i = 0; i < Math.min(source.length, 64); i++) {
            final byte b = source[i];
            if (b == '<') {
                return true;
            }
            if (b != ' ' && b != '\n' && b != '\r' && b != '\t' && b != (byte) 0xEF
                    && b != (byte) 0xBB && b != (byte) 0xBF) {
                return false;
            }
        }
        return false;
    }

    public static Bitmap render(byte[] source, int sizePx) {
        if (source == null || source.length == 0 || sizePx <= 0) {
            return null;
        }
        try {
            final Drawing drawing = parse(source);
            if (drawing == null || drawing.paths.isEmpty()) {
                return null;
            }

            final Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
            final Canvas canvas = new Canvas(bitmap);
            final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.WHITE);

            final float scale = Math.min(sizePx / drawing.width, sizePx / drawing.height);
            canvas.translate((sizePx - drawing.width * scale) / 2f,
                    (sizePx - drawing.height * scale) / 2f);
            canvas.scale(scale, scale);
            canvas.translate(-drawing.left, -drawing.top);

            for (Path path : drawing.paths) {
                canvas.drawPath(path, paint);
            }
            return bitmap;
        } catch (Throwable ignore) {

            return null;
        }
    }

    private static final class Drawing {
        float left;
        float top;
        float width;
        float height;
        final List<Path> paths = new ArrayList<>();
    }

    private static Drawing parse(byte[] source) throws Exception {
        final XmlPullParser parser = Xml.newPullParser();
        parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false);
        parser.setInput(new ByteArrayInputStream(source), null);

        final Drawing drawing = new Drawing();
        final android.util.SparseArray<android.graphics.Matrix> transforms = new android.util.SparseArray<>();
        boolean rootSeen = false;
        int skipBelow = -1;

        int event = parser.getEventType();
        while (event != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.END_TAG && skipBelow >= 0 && parser.getDepth() <= skipBelow) {
                skipBelow = -1;
            }
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (skipBelow >= 0) {
                    event = parser.next();
                    continue;
                }
                switch (tag) {
                    case "svg":
                        readFrame(parser, drawing);
                        rootSeen = true;
                        break;
                    case "g":

                        transforms.put(parser.getDepth(), combine(current(transforms, parser.getDepth()),
                                parseTransform(parser.getAttributeValue(null, "transform"))));
                        break;
                    case "defs":
                    case "clipPath":
                    case "mask":

                        skipBelow = parser.getDepth();
                        break;
                    case "path":
                        addPath(parser, drawing, current(transforms, parser.getDepth()));
                        break;
                    case "circle":
                        addCircle(parser, drawing, current(transforms, parser.getDepth()));
                        break;
                    case "rect":
                        addRect(parser, drawing, current(transforms, parser.getDepth()));
                        break;
                    default:
                        break;
                }
            }
            event = parser.next();
        }
        return rootSeen && drawing.width > 0 && drawing.height > 0 ? drawing : null;
    }

    private static void readFrame(XmlPullParser parser, Drawing drawing) {
        final String box = parser.getAttributeValue(null, "viewBox");
        if (box != null) {
            final String[] parts = box.trim().split("[\\s,]+");
            if (parts.length == 4) {
                drawing.left = number(parts[0]);
                drawing.top = number(parts[1]);
                drawing.width = number(parts[2]);
                drawing.height = number(parts[3]);
            }
        }
        if (drawing.width <= 0 || drawing.height <= 0) {
            drawing.left = 0;
            drawing.top = 0;
            drawing.width = attr(parser, "width", 0);
            drawing.height = attr(parser, "height", 0);
        }
    }

    private static void addPath(XmlPullParser parser, Drawing drawing, android.graphics.Matrix inherited) {
        final String data = parser.getAttributeValue(null, "d");
        if (data == null || data.isEmpty()) {
            return;
        }
        final Path path = PathParser.createPathFromPathData(data);
        if (path == null) {
            return;
        }
        if ("evenodd".equals(parser.getAttributeValue(null, "fill-rule"))) {
            path.setFillType(Path.FillType.EVEN_ODD);
        }
        apply(path, inherited, parser);
        drawing.paths.add(path);
    }

    private static void addCircle(XmlPullParser parser, Drawing drawing, android.graphics.Matrix inherited) {
        final float r = attr(parser, "r", 0);
        if (r <= 0) {
            return;
        }
        final Path path = new Path();
        path.addCircle(attr(parser, "cx", 0), attr(parser, "cy", 0), r, Path.Direction.CW);
        apply(path, inherited, parser);
        drawing.paths.add(path);
    }

    private static void addRect(XmlPullParser parser, Drawing drawing, android.graphics.Matrix inherited) {
        final float w = attr(parser, "width", 0);
        final float h = attr(parser, "height", 0);
        if (w <= 0 || h <= 0) {
            return;
        }
        final float x = attr(parser, "x", 0);
        final float y = attr(parser, "y", 0);
        final float rx = attr(parser, "rx", 0);
        final Path path = new Path();
        path.addRoundRect(new RectF(x, y, x + w, y + h), rx, rx, Path.Direction.CW);
        apply(path, inherited, parser);
        drawing.paths.add(path);
    }

    private static void apply(Path path, android.graphics.Matrix inherited, XmlPullParser parser) {
        final android.graphics.Matrix own = parseTransform(parser.getAttributeValue(null, "transform"));
        final android.graphics.Matrix matrix = combine(inherited, own);
        if (matrix != null) {
            path.transform(matrix);
        }
    }

    private static android.graphics.Matrix current(
            android.util.SparseArray<android.graphics.Matrix> transforms, int depth) {

        for (int d = depth; d > 0; d--) {
            final android.graphics.Matrix matrix = transforms.get(d);
            if (matrix != null) {
                return matrix;
            }
        }
        return null;
    }

    private static android.graphics.Matrix combine(
            android.graphics.Matrix outer, android.graphics.Matrix inner) {
        if (outer == null) return inner;
        if (inner == null) return outer;
        final android.graphics.Matrix result = new android.graphics.Matrix(outer);
        result.preConcat(inner);
        return result;
    }

    private static android.graphics.Matrix parseTransform(String raw) {
        if (raw == null || raw.isEmpty()) {
            return null;
        }
        android.graphics.Matrix matrix = null;
        final java.util.regex.Matcher m = TRANSFORM.matcher(raw);
        while (m.find()) {
            final String name = m.group(1);
            final String[] parts = m.group(2).trim().split("[\\s,]+");
            final float[] v = new float[parts.length];
            for (int a = 0; a < parts.length; a++) {
                v[a] = number(parts[a]);
            }
            final android.graphics.Matrix step = new android.graphics.Matrix();
            switch (name) {
                case "matrix":
                    if (v.length >= 6) {

                        step.setValues(new float[]{v[0], v[2], v[4], v[1], v[3], v[5], 0, 0, 1});
                    }
                    break;
                case "translate":
                    step.setTranslate(v[0], v.length > 1 ? v[1] : 0);
                    break;
                case "scale":
                    step.setScale(v[0], v.length > 1 ? v[1] : v[0]);
                    break;
                case "rotate":
                    if (v.length >= 3) {
                        step.setRotate(v[0], v[1], v[2]);
                    } else {
                        step.setRotate(v[0]);
                    }
                    break;
                default:
                    continue;
            }
            matrix = combine(matrix, step);
        }
        return matrix;
    }

    private static final java.util.regex.Pattern TRANSFORM =
            java.util.regex.Pattern.compile("(matrix|translate|scale|rotate)\\s*\\(([^)]*)\\)");

    private static float attr(XmlPullParser parser, String name, float fallback) {
        final String raw = parser.getAttributeValue(null, name);
        if (raw == null) {
            return fallback;
        }
        final float value = number(raw);
        return value == 0 ? fallback : value;
    }

    private static float number(String raw) {
        int end = 0;
        while (end < raw.length()) {
            final char c = raw.charAt(end);
            if ((c < '0' || c > '9') && c != '.' && c != '-' && c != '+' && c != 'e' && c != 'E') {
                break;
            }
            end++;
        }
        if (end == 0) {
            return 0;
        }
        try {
            return Float.parseFloat(raw.substring(0, end));
        } catch (NumberFormatException e) {
            return 0;
        }
    }
}
