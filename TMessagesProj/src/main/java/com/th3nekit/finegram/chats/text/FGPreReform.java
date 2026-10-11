/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.chats.text;

import org.telegram.tgnet.TLRPC;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

public final class FGPreReform {

    private FGPreReform() {}

    public static final class Result {
        public String text;
        public ArrayList<TLRPC.MessageEntity> entities;
    }

    private static final Set<String> YAT_ROOTS = new HashSet<>(Arrays.asList(
            "бег", "бед", "бел", "бес", "вед", "век", "вер", "вес", "вет",
            "гнев", "гнезд", "дев", "дед", "дел", "дет", "ед", "езд",
            "звезд", "звер", "зре", "калек", "колен", "креп",
            "лев", "лез", "лек", "лен", "леп", "лес",
            "мед", "мел", "мен", "мер", "мест", "месяц", "мет", "мех",
            "нег", "нем", "нет", "обед", "обет", "орех",
            "пех", "плен", "плесен", "плеш", "полен",
            "прес", "рез", "рек", "реч", "реш",
            "свеж", "свет", "свиреп", "сев", "сед", "сем", "сен", "сер",
            "сеч", "след", "слеп", "смех", "снег", "спех",
            "стен", "стрел", "стрех", "сусед", "тел", "тен", "тес",
            "увеч", "удел", "хлеб", "хлев", "хмел", "хрен",
            "цвет", "цед", "цел", "цен", "цеп", "человек"
    ));

    private static final String[][] WHOLE_WORDS = {
            {"где", "гдѣ"}, {"здесь", "здѣсь"},
            {"мне", "мнѣ"}, {"тебе", "тебѣ"}, {"себе", "себѣ"},
            {"все", "всѣ"}, {"всех", "всѣхъ"}, {"всем", "всѣмъ"}, {"всеми", "всѣми"},
            {"те", "тѣ"}, {"тех", "тѣхъ"}, {"тем", "тѣмъ"}, {"теми", "тѣми"},
            {"ныне", "нынѣ"}, {"после", "послѣ"}, {"разве", "развѣ"},
            {"везде", "вездѣ"}, {"кроме", "кромѣ"}, {"подле", "подлѣ"},
            {"возле", "возлѣ"}, {"вне", "внѣ"}, {"еще", "еще"},
            {"лето", "лѣто"}, {"лета", "лѣта"}, {"летом", "лѣтомъ"}, {"лет", "лѣтъ"},
            {"тесто", "тѣсто"}, {"тесный", "тѣсный"}, {"сеть", "сѣть"}, {"сети", "сѣти"},
            {"место", "мѣсто"}, {"места", "мѣста"}, {"месяц", "мѣсяцъ"}, {"пена", "пѣна"},
            {"песня", "пѣсня"}, {"песни", "пѣсни"}, {"петь", "пѣть"}, {"поет", "поётъ"}
    };

    private static final String[][] FITA_WORDS = {
            {"феодор", "ѳеодор"}, {"федор", "ѳедор"}, {"фома", "ѳома"},
            {"афин", "аѳин"}, {"афон", "аѳон"}, {"мифолог", "миѳолог"},
            {"миф", "миѳ"}, {"орфограф", "орѳограф"}, {"арифметик", "ариѳметик"},
            {"кафедр", "каѳедр"}, {"марфа", "марѳа"}
    };

    private static final String VOWELS = "аеёиоуыэюяѣ";
    private static final String HARD_CONSONANTS = "бвгджзклмнпрстфхцчш";

    private static final String[] PREFIXES = {
            "", "пере", "пред", "пре", "при", "под", "над", "раз", "рас", "воз", "вос",
            "не", "по", "за", "на", "об", "от", "до", "из", "со", "про", "вы", "у", "в", "с", "о"
    };

    public static Result convert(String text, ArrayList<TLRPC.MessageEntity> entities) {
        Result result = new Result();
        result.entities = entities;
        result.text = text;
        if (text == null || text.isEmpty()) return result;

        final int length = text.length();
        final boolean[] locked = lockedPositions(text, entities);

        StringBuilder out = new StringBuilder(length + (length >> 3));
        int[] map = new int[length + 1];

        int i = 0;
        while (i < length) {
            char c = text.charAt(i);
            if (!isCyrillic(c) || locked[i]) {
                map[i] = out.length();
                out.append(c);
                i++;
                continue;
            }
            int end = i;
            while (end < length && isCyrillic(text.charAt(end)) && !locked[end]) end++;

            String word = text.substring(i, end);
            appendMapped(out, map, i, end, convertWord(word));
            i = end;
        }
        map[length] = out.length();

        result.text = out.toString();
        result.entities = remap(entities, map, length);
        return result;
    }

    public static String convertWord(String word) {
        if (word == null || word.isEmpty()) return word;
        String lower = word.toLowerCase(Locale.ROOT);

        for (String[] pair : WHOLE_WORDS) {
            if (lower.equals(pair[0])) return matchCase(word, pair[1]);
        }

        String converted = lower;
        for (String[] pair : FITA_WORDS) {
            if (converted.startsWith(pair[0])) {
                converted = pair[1] + converted.substring(pair[0].length());
                break;
            }
        }
        converted = applyYat(converted);
        converted = applyDecimalI(converted);
        converted = applyAdjectiveEndings(converted);
        converted = applyHardSign(converted);
        return matchCase(word, converted);
    }

    private static String applyYat(String lower) {
        for (String prefix : PREFIXES) {
            if (!lower.startsWith(prefix)) continue;
            String rest = lower.substring(prefix.length());
            for (int len = Math.min(rest.length(), 8); len >= 2; len--) {
                String root = rest.substring(0, len);
                if (!YAT_ROOTS.contains(root)) continue;
                if (!endsAtBoundary(rest, len)) continue;
                int e = root.lastIndexOf('е');
                if (e < 0) continue;
                int at = prefix.length() + e;
                return lower.substring(0, at) + 'ѣ' + lower.substring(at + 1);
            }
        }
        return lower;
    }

    private static boolean endsAtBoundary(String rest, int rootLength) {
        if (rest.length() == rootLength) return true;
        char next = rest.charAt(rootLength);
        return VOWELS.indexOf(next) >= 0 || next == 'ь' || next == 'й';
    }

    private static String applyDecimalI(String lower) {
        StringBuilder sb = new StringBuilder(lower);
        for (int i = 0; i < sb.length() - 1; i++) {
            if (sb.charAt(i) != 'и') continue;
            char next = sb.charAt(i + 1);
            if (VOWELS.indexOf(next) >= 0 || next == 'й') {
                sb.setCharAt(i, 'і');
            }
        }
        return sb.toString();
    }

    private static String applyAdjectiveEndings(String lower) {
        if (lower.length() < 5) return lower;
        if (lower.endsWith("ого")) return lower.substring(0, lower.length() - 3) + "аго";
        if (lower.endsWith("его")) return lower.substring(0, lower.length() - 3) + "яго";
        return lower;
    }

    private static String applyHardSign(String lower) {
        if (lower.isEmpty()) return lower;
        char last = lower.charAt(lower.length() - 1);
        if (HARD_CONSONANTS.indexOf(last) >= 0) return lower + 'ъ';
        return lower;
    }

    private static String matchCase(String original, String converted) {
        if (original.isEmpty() || converted.isEmpty()) return converted;
        boolean allUpper = original.length() > 1 && original.equals(original.toUpperCase(Locale.ROOT));
        if (allUpper) return converted.toUpperCase(Locale.ROOT);
        if (Character.isUpperCase(original.charAt(0))) {
            return Character.toUpperCase(converted.charAt(0)) + converted.substring(1);
        }
        return converted;
    }

    private static boolean[] lockedPositions(String text, ArrayList<TLRPC.MessageEntity> entities) {
        boolean[] locked = new boolean[text.length()];
        if (entities == null) return locked;
        for (TLRPC.MessageEntity entity : entities) {
            if (entity == null || !isProtected(entity)) continue;
            int from = Math.max(0, entity.offset);
            int to = Math.min(text.length(), entity.offset + entity.length);
            for (int i = from; i < to; i++) locked[i] = true;
        }
        return locked;
    }

    private static boolean isProtected(TLRPC.MessageEntity entity) {
        return entity instanceof TLRPC.TL_messageEntityUrl
                || entity instanceof TLRPC.TL_messageEntityTextUrl
                || entity instanceof TLRPC.TL_messageEntityMention
                || entity instanceof TLRPC.TL_messageEntityMentionName
                || entity instanceof TLRPC.TL_messageEntityHashtag
                || entity instanceof TLRPC.TL_messageEntityCashtag
                || entity instanceof TLRPC.TL_messageEntityBotCommand
                || entity instanceof TLRPC.TL_messageEntityEmail
                || entity instanceof TLRPC.TL_messageEntityPhone
                || entity instanceof TLRPC.TL_messageEntityCode
                || entity instanceof TLRPC.TL_messageEntityPre;
    }

    private static void appendMapped(StringBuilder out, int[] map, int from, int to, String converted) {
        int base = out.length();
        int originalLength = to - from;
        int convertedLength = converted.length();
        for (int i = 0; i < originalLength; i++) {
            int shifted = originalLength == 0 ? 0 : (int) ((long) i * convertedLength / originalLength);
            map[from + i] = base + Math.min(shifted, convertedLength);
        }
        out.append(converted);
    }

    private static ArrayList<TLRPC.MessageEntity> remap(ArrayList<TLRPC.MessageEntity> entities,
                                                        int[] map, int originalLength) {
        if (entities == null || entities.isEmpty()) return entities;
        for (TLRPC.MessageEntity entity : entities) {
            if (entity == null) continue;
            int from = clamp(entity.offset, originalLength);
            int to = clamp(entity.offset + entity.length, originalLength);
            int newFrom = map[from];
            int newTo = map[to];
            entity.offset = newFrom;
            entity.length = Math.max(0, newTo - newFrom);
        }
        return entities;
    }

    private static int clamp(int value, int max) {
        if (value < 0) return 0;
        return Math.min(value, max);
    }

    private static boolean isCyrillic(char c) {
        return (c >= 'а' && c <= 'я') || (c >= 'А' && c <= 'Я') || c == 'ё' || c == 'Ё';
    }
}
