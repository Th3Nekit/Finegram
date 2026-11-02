package com.th3nekit.finegram.core.crash;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.util.ArrayDeque;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class DiagnosticLogcat {
    private static final Pattern LINE = Pattern.compile("^((0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01]) "
            + "(?:[01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]\\.[0-9]{3} +"
            + "([1-9][0-9]{0,9}) +([1-9][0-9]{0,9}) +[VDIWEF] +)([^\\r\\n]*)$");
    private static final Pattern MESSAGE = Pattern.compile("^([^:]+): ?(.*)$");
    private static final Pattern CAMERA_TAG = Pattern.compile("(?i)^(?:Camera.*|.*Camera[23X].*|cameraserver|BufferQueue.*|Surface.*|OpenGL.*|EGL.*|MediaCodec.*|CCodec.*|ACodec.*|MPEG4Writer|MediaRecorder.*|VideoEncoder.*|AudioRecord.*|DeferrableSurface|UseCase.*|CaptureSession.*|SynchronizedCaptureSession.*|ProcessingCaptureSession.*|VideoCapture.*|Recorder|DynamicRangeResolver|SupportedSurfaceCombination|ImageCapture.*|StreamSharing.*)$");
    private static final Pattern CAMERA_MESSAGE = Pattern.compile("(?i)^(?:Camera[X23]?|RoundCamera|RoundVideo|torch|CameraDiagnostics)(?:[ .:#_\\[\\]-]|$).*");

    static String cameraLine(String line, int pid) {
        if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);
        Matcher match = threadtime(line);
        if (match == null || !match.group(4).equals(Integer.toString(pid))) return null;
        Matcher message = MESSAGE.matcher(match.group(6));
        if (!message.matches()) return null;
        String tag = message.group(1).trim();
        if (!CAMERA_TAG.matcher(tag).matches()
                && !(tag.equals("tmessages") && CAMERA_MESSAGE.matcher(message.group(2)).matches())) return null;
        return cleanLine(line);
    }

    private static Matcher threadtime(String line) {
        if (line.length() > 2048) return null;
        Matcher match = LINE.matcher(line);
        if (!match.matches()) return null;
        int month = Integer.parseInt(match.group(2));
        int maxDay = month == 2 ? 29 : (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);
        if (Integer.parseInt(match.group(3)) > maxDay
                || Long.parseLong(match.group(4)) > Integer.MAX_VALUE
                || Long.parseLong(match.group(5)) > Integer.MAX_VALUE) return null;
        return match;
    }

    static String cleanLine(String line) {
        Matcher match = threadtime(line);
        return match == null ? DiagnosticText.clean(line)
                : match.group(1) + DiagnosticText.clean(match.group(6));
    }

    static String read(List<String> command, long timeoutMs, int maxRawBytes,
                       BooleanSupplier cancelled, Consumer<String> consumer) {
        if (cancelled.getAsBoolean()) return "cancelled";
        final Process process;
        try {
            process = new ProcessBuilder(command).redirectErrorStream(true).start();
        } catch (IOException | SecurityException e) {
            return "unavailable (" + e.getClass().getSimpleName() + ")";
        }
        final AtomicBoolean limit = new AtomicBoolean();
        final AtomicBoolean finished = new AtomicBoolean();
        final AtomicBoolean active = new AtomicBoolean(true);
        final AtomicBoolean failed = new AtomicBoolean();
        final LinkedBlockingQueue<String> lines = new LinkedBlockingQueue<>(128);
        Thread reader = new Thread(() -> {
            try (InputStream input = process.getInputStream()) {
                ByteArrayOutputStream line = new ByteArrayOutputStream();
                int total = 0;
                boolean oversized = false;
                int next;
                while (active.get() && !cancelled.getAsBoolean() && (next = input.read()) != -1) {
                    if (++total > maxRawBytes) {
                        limit.set(true);
                        process.destroy();
                        break;
                    }
                    if (next == '\n') {
                        if (!oversized) enqueue(lines, line, active, cancelled);
                        line.reset();
                        oversized = false;
                    } else if (line.size() < 4096) {
                        line.write(next);
                    } else {
                        oversized = true;
                    }
                }
                if (!oversized && line.size() > 0 && !limit.get() && active.get() && !cancelled.getAsBoolean()) enqueue(lines, line, active, cancelled);
            } catch (IOException ignored) {
                if (active.get() && !cancelled.getAsBoolean()) failed.set(true);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } finally {
                finished.set(true);
            }
        }, "diagnostics-logcat-reader");
        reader.setDaemon(true);
        reader.start();
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs);
        String status = "available (camera/media lines from own PID only; may be empty)";
        Integer exit = null;
        try {
            while (true) {
                if (cancelled.getAsBoolean()) { status = "cancelled"; break; }
                if (System.nanoTime() >= deadline) { status = "time limit reached"; break; }
                String line = lines.poll(50, TimeUnit.MILLISECONDS);
                if (line != null && !cancelled.getAsBoolean()) {
                    try { consumer.accept(line); }
                    catch (RuntimeException e) { status = "unavailable (consumer failure)"; break; }
                }
                try {
                    exit = process.exitValue();
                    if (finished.get() && lines.isEmpty()) break;
                } catch (IllegalThreadStateException running) {
                }
            }
            if (cancelled.getAsBoolean()) status = "cancelled";
            else if (exit != null && exit != 0) status = "unavailable (logcat exit " + exit + ")";
            else if (failed.get()) status = "unavailable (read failure)";
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            status = "interrupted";
        } finally {
            active.set(false);
            if (exit == null) process.destroy();
            try { process.getInputStream().close(); } catch (IOException ignored) {}
            reader.interrupt();
            try { reader.join(500); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            lines.clear();
        }
        return limit.get() ? "raw byte limit reached" : status;
    }

    private static void enqueue(LinkedBlockingQueue<String> lines, ByteArrayOutputStream bytes,
                                AtomicBoolean active, BooleanSupplier cancelled) throws InterruptedException {
        final String line;
        try {
            line = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes.toByteArray())).toString();
        } catch (CharacterCodingException e) {
            return;
        }
        while (active.get() && !cancelled.getAsBoolean()) {
            if (lines.offer(line, 50, TimeUnit.MILLISECONDS)) return;
        }
    }

    static final class Tail {
        private final int maxChars;
        private int chars;
        private final ArrayDeque<String> lines = new ArrayDeque<>();

        Tail(int maxChars) { this.maxChars = maxChars; }

        synchronized void add(String line) {
            if (line == null || line.isEmpty()) return;
            line = cleanLine(line);
            if (line.length() + 1 > maxChars) return;
            while (chars + line.length() + 1 > maxChars) chars -= lines.removeFirst().length() + 1;
            lines.addLast(line);
            chars += line.length() + 1;
        }

        synchronized String snapshot() {
            if (lines.isEmpty()) return "No accessible camera/media logcat lines for this PID.\n";
            StringBuilder out = new StringBuilder();
            for (String line : lines) out.append(line).append('\n');
            return out.toString();
        }
    }

    private DiagnosticLogcat() {}
}
