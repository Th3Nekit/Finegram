package com.th3nekit.finegram.core.crash;

import java.util.regex.Pattern;

final class DiagnosticText {
    private static final Pattern PRIVATE = Pattern.compile("(?i)(?:\\b(?:[a-z0-9_-]*(?:token|password|passwd|secret|subscription|authorization|api[_-]?key|auth[_-]?key)|bearer|phone|email|subscribe|proxy|account(?:[_-]?id)?|dialog(?:[_-]?id)?|chat(?:[_-]?id)?|user[_-]?id|message[_-]?(?:text|body)|payload)\\b[\\\"']?\\s*[:=]|\\bbearer\\s+\\S+|\\b(?:vless|vmess|trojan|ss|ssr)://)");
    private static final Pattern PRIVATE_FIELDS = Pattern.compile("(?i)\\b(?:authorization|bearer|cookie|password|passwd|secret|android[_-]?id|session[_-]?id)\\b|[\\\"']?(?:text|caption|message[_-]?(?:text|body|payload))[\\\"']?\\s*[:=]");
    private static final Pattern URL = Pattern.compile("(?i)\\b[a-z][a-z0-9+.-]*://\\S+|\\b(?:www\\.|t\\.me/|telegram\\.me/)\\S+|\\b(?:[a-z0-9-]+\\.)+[a-z]{2,}(?::[0-9]+)?/[^\\s\\\"']*");
    private static final Pattern EMAIL = Pattern.compile("[\\w.+-]+@[\\w.-]+\\.[A-Za-z]{2,}|(?<![\\w])@[A-Za-z0-9_]{3,}");
    private static final Pattern PATH = Pattern.compile("[\\\"'](?:/(?:data|storage|sdcard|mnt|cache|home|private|Users)/|[A-Za-z]:[\\\\/])[^\\\"']*[\\\"']|(?:/(?:data|storage|sdcard|mnt|cache|home|private|Users)/|[A-Za-z]:[\\\\/])[^\\s\"']+");
    private static final Pattern ADDRESS = Pattern.compile("\\b(?:\\d{1,3}\\.){3}\\d{1,3}(?::\\d+)?\\b|(?i)(?<![\\w:])(?:[0-9a-f]{0,4}:){2,}[0-9a-f:]{0,39}(?:%\\w+)?");
    private static final Pattern IDENTIFIER = Pattern.compile("(?i)\\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\b|(?<![\\w])\\+?\\d[\\d ()-]{8,}\\d(?![\\w])|\\b[0-9a-f]{16,}\\b|[A-Za-z0-9_+/=-]{48,}");

    static String clean(String input) {
        if (input == null || input.isEmpty()) return "";
        StringBuilder out = new StringBuilder();
        int end = Math.min(input.length(), 32768);
        if (end < input.length() && end > 0 && Character.isHighSurrogate(input.charAt(end - 1))) end--;
        String bounded = input.substring(0, end);
        for (String line : bounded.split("\\r?\\n", -1)) {
            if (out.length() > 0) out.append('\n');
            if (line.length() > 2048) {
                out.append("[oversized line omitted]");
                continue;
            }
            line = line.replaceAll("[\\p{Cc}\\p{Cf}&&[^\\t]]", " ");
            if (PRIVATE.matcher(line).find() || PRIVATE_FIELDS.matcher(line).find()) {
                out.append("[private data omitted]");
                continue;
            }
            line = URL.matcher(line).replaceAll("[link]");
            line = EMAIL.matcher(line).replaceAll("[identity]");
            line = PATH.matcher(line).replaceAll("[path]");
            line = ADDRESS.matcher(line).replaceAll("[address]");
            out.append(IDENTIFIER.matcher(line).replaceAll("[identifier]"));
        }
        return out.toString();
    }

    private DiagnosticText() {}
}
