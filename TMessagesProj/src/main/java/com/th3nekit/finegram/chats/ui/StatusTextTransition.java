package com.th3nekit.finegram.chats.ui;

public final class StatusTextTransition {
    private StatusTextTransition() {}

    public static boolean isCounterChange(CharSequence previous, CharSequence next) {
        if (previous == null || next == null) return false;
        int before = firstDigit(previous), after = firstDigit(next);
        if (before < 0 || before != after) return false;
        for (int i = 0; i < before; i++) {
            if (previous.charAt(i) != next.charAt(i)) return false;
        }
        return true;
    }

    private static int firstDigit(CharSequence text) {
        for (int i = 0; i < text.length(); i++) {
            if (Character.isDigit(text.charAt(i))) return i;
        }
        return -1;
    }

    public static int[] commonEnds(CharSequence before, CharSequence after) {
        int limit = Math.min(before.length(), after.length());
        int prefix = 0, suffix = 0;
        while (prefix < limit && before.charAt(prefix) == after.charAt(prefix)) prefix++;
        if (prefix > 0 && prefix < limit && Character.isHighSurrogate(before.charAt(prefix - 1))) prefix--;
        while (suffix < limit - prefix && before.charAt(before.length() - suffix - 1) == after.charAt(after.length() - suffix - 1)) suffix++;
        if (suffix > 0 && Character.isLowSurrogate(before.charAt(before.length() - suffix))) suffix--;
        return new int[]{prefix, suffix};
    }
}
