/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.crash;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class TombstoneText {

    private static final int MAX_LOG_LINES = 200;
    private static final String PRIORITIES = "  VDIWEF";

    private TombstoneText() {
    }

    public static String format(byte[] data) {
        if (data == null || data.length == 0) {
            return null;
        }
        try {
            return new Tombstone(new Reader(data, 0, data.length)).toText();
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static final class Tombstone {
        String fingerprint = "";
        String timestamp = "";
        long pid;
        long tid;
        final List<String> commandLine = new ArrayList<>();
        long signalNumber = -1;
        String signalName = "";
        long signalCode;
        String signalCodeName = "";
        boolean hasFaultAddress;
        long faultAddress;
        String abortMessage = "";
        final List<String> causes = new ArrayList<>();
        final List<ThreadInfo> threads = new ArrayList<>();
        final List<String> log = new ArrayList<>();

        Tombstone(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                switch (tag >>> 3) {
                    case 2: fingerprint = in.string(); break;
                    case 4: timestamp = in.string(); break;
                    case 5: pid = in.varint(); break;
                    case 6: tid = in.varint(); break;
                    case 9: commandLine.add(in.string()); break;
                    case 10: readSignal(in.message()); break;
                    case 14: abortMessage = in.string(); break;
                    case 15: readCause(in.message()); break;
                    case 16: readThreadEntry(in.message()); break;
                    case 18: readLogBuffer(in.message()); break;
                    default: in.skip(tag & 7);
                }
            }
        }

        private void readSignal(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                switch (tag >>> 3) {
                    case 1: signalNumber = in.varint(); break;
                    case 2: signalName = in.string(); break;
                    case 3: signalCode = in.varint(); break;
                    case 4: signalCodeName = in.string(); break;
                    case 8: hasFaultAddress = in.varint() != 0; break;
                    case 9: faultAddress = in.varint(); break;
                    default: in.skip(tag & 7);
                }
            }
        }

        private void readCause(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                if ((tag >>> 3) == 1) {
                    causes.add(in.string());
                } else {
                    in.skip(tag & 7);
                }
            }
        }

        private void readThreadEntry(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                if ((tag >>> 3) == 2) {
                    threads.add(new ThreadInfo(in.message()));
                } else {
                    in.skip(tag & 7);
                }
            }
        }

        private void readLogBuffer(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                if ((tag >>> 3) == 2) {
                    log.add(readLogLine(in.message()));
                    if (log.size() > MAX_LOG_LINES * 2) {

                        log.subList(0, log.size() - MAX_LOG_LINES).clear();
                    }
                } else {
                    in.skip(tag & 7);
                }
            }
        }

        private static String readLogLine(Reader in) {
            String time = "", tag = "", message = "";
            long pid = 0, tid = 0, priority = 0;
            while (in.more()) {
                final int t = in.tag();
                switch (t >>> 3) {
                    case 1: time = in.string(); break;
                    case 2: pid = in.varint(); break;
                    case 3: tid = in.varint(); break;
                    case 4: priority = in.varint(); break;
                    case 5: tag = in.string(); break;
                    case 6: message = in.string(); break;
                    default: in.skip(t & 7);
                }
            }
            final char level = priority >= 0 && priority < PRIORITIES.length() ? PRIORITIES.charAt((int) priority) : '?';
            return String.format(Locale.US, "%s %5d %5d %c %s: %s", time, pid, tid, level, tag, message.trim());
        }

        String toText() {
            if (signalNumber < 0 && threads.isEmpty()) {
                return null;
            }
            final StringBuilder out = new StringBuilder(8192);
            out.append("signal ").append(signalNumber);
            if (!signalName.isEmpty()) out.append(" (").append(signalName).append(')');
            out.append(", code ").append(signalCode);
            if (!signalCodeName.isEmpty()) out.append(" (").append(signalCodeName).append(')');
            if (hasFaultAddress) out.append(String.format(Locale.US, ", fault addr 0x%016x", faultAddress));
            out.append('\n');
            if (!abortMessage.isEmpty()) out.append("Abort message: ").append(abortMessage).append('\n');
            for (String cause : causes) {
                out.append("Cause: ").append(cause).append('\n');
            }
            if (!commandLine.isEmpty()) out.append("Cmdline: ").append(String.join(" ", commandLine)).append('\n');
            out.append("pid: ").append(pid).append(", tid: ").append(tid);
            final ThreadInfo crashed = findThread(tid);
            if (crashed != null && !crashed.name.isEmpty()) out.append(", name: ").append(crashed.name);
            out.append('\n');
            if (!timestamp.isEmpty()) out.append("Timestamp: ").append(timestamp).append('\n');
            if (!fingerprint.isEmpty()) out.append("Build fingerprint: ").append(fingerprint).append('\n');

            if (crashed != null) {
                out.append('\n').append("backtrace:\n");
                crashed.appendBacktrace(out);
            }

            if (!log.isEmpty()) {
                out.append("\n--------- log ---------\n");
                for (int i = Math.max(0, log.size() - MAX_LOG_LINES); i < log.size(); i++) {
                    out.append(log.get(i)).append('\n');
                }
            }
            return out.toString();
        }

        private ThreadInfo findThread(long id) {
            for (ThreadInfo thread : threads) {
                if (thread.id == id) return thread;
            }
            return threads.isEmpty() ? null : threads.get(0);
        }
    }

    private static final class ThreadInfo {
        long id;
        String name = "";
        final List<String> notes = new ArrayList<>();
        final List<String> frames = new ArrayList<>();

        ThreadInfo(Reader in) {
            while (in.more()) {
                final int tag = in.tag();
                switch (tag >>> 3) {
                    case 1: id = in.varint(); break;
                    case 2: name = in.string(); break;
                    case 4: frames.add(readFrame(in.message(), frames.size())); break;
                    case 7: notes.add(in.string()); break;
                    default: in.skip(tag & 7);
                }
            }
        }

        private static String readFrame(Reader in, int index) {
            long relPc = 0, functionOffset = 0, mapOffset = 0;
            String function = "", file = "", buildId = "";
            while (in.more()) {
                final int tag = in.tag();
                switch (tag >>> 3) {
                    case 1: relPc = in.varint(); break;
                    case 4: function = in.string(); break;
                    case 5: functionOffset = in.varint(); break;
                    case 6: file = in.string(); break;
                    case 7: mapOffset = in.varint(); break;
                    case 8: buildId = in.string(); break;
                    default: in.skip(tag & 7);
                }
            }
            final StringBuilder line = new StringBuilder(160);
            line.append(String.format(Locale.US, "      #%02d pc %016x  %s", index, relPc, file));
            if (mapOffset != 0) line.append(String.format(Locale.US, " (offset 0x%x)", mapOffset));
            if (!function.isEmpty()) line.append(" (").append(function).append('+').append(functionOffset).append(')');
            if (!buildId.isEmpty()) line.append(" (BuildId: ").append(buildId).append(')');
            return line.toString();
        }

        void appendBacktrace(StringBuilder out) {
            for (String note : notes) {
                out.append("  NOTE: ").append(note).append('\n');
            }
            for (String frame : frames) {
                out.append(frame).append('\n');
            }
        }
    }

    private static final class Reader {
        private final byte[] data;
        private int pos;
        private final int end;

        Reader(byte[] data, int start, int end) {
            this.data = data;
            this.pos = start;
            this.end = end;
        }

        boolean more() {
            return pos < end;
        }

        int tag() {
            return (int) varint();
        }

        long varint() {
            long result = 0;
            for (int shift = 0; shift < 64; shift += 7) {
                if (pos >= end) throw new IllegalStateException("varint");
                final byte b = data[pos++];
                result |= (long) (b & 0x7f) << shift;
                if ((b & 0x80) == 0) return result;
            }
            throw new IllegalStateException("varint too long");
        }

        private int length() {
            final long length = varint();
            if (length < 0 || length > end - pos) throw new IllegalStateException("length");
            return (int) length;
        }

        String string() {
            final int length = length();
            final String value = new String(data, pos, length, StandardCharsets.UTF_8);
            pos += length;
            return value;
        }

        Reader message() {
            final int length = length();
            final Reader sub = new Reader(data, pos, pos + length);
            pos += length;
            return sub;
        }

        void skip(int wireType) {
            switch (wireType) {
                case 0: varint(); break;
                case 1: advance(8); break;
                case 2: advance(length()); break;
                case 5: advance(4); break;
                default: throw new IllegalStateException("wire type " + wireType);
            }
        }

        private void advance(int count) {
            if (count > end - pos) throw new IllegalStateException("skip");
            pos += count;
        }
    }
}
