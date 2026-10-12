/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.telegram.messenger.time;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.Serializable;
import java.text.DateFormatSymbols;
import java.text.ParseException;
import java.text.ParsePosition;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.SortedMap;
import java.util.TimeZone;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class FastDateParser implements DateParser, Serializable {

    private static final long serialVersionUID = 2L;

    static final Locale JAPANESE_IMPERIAL = new Locale("ja", "JP", "JP");

    private final String pattern;
    private final TimeZone timeZone;
    private final Locale locale;
    private final int century;
    private final int startYear;

    private transient Pattern parsePattern;
    private transient Strategy[] strategies;

    private transient String currentFormatField;
    private transient Strategy nextStrategy;

    protected FastDateParser(final String pattern, final TimeZone timeZone, final Locale locale) {
        this(pattern, timeZone, locale, null);
    }

    protected FastDateParser(final String pattern, final TimeZone timeZone, final Locale locale, final Date centuryStart) {
        this.pattern = pattern;
        this.timeZone = timeZone;
        this.locale = locale;

        final Calendar definingCalendar = Calendar.getInstance(timeZone, locale);
        int centuryStartYear;
        if (centuryStart != null) {
            definingCalendar.setTime(centuryStart);
            centuryStartYear = definingCalendar.get(Calendar.YEAR);
        } else if (locale.equals(JAPANESE_IMPERIAL)) {
            centuryStartYear = 0;
        } else {

            definingCalendar.setTime(new Date());
            centuryStartYear = definingCalendar.get(Calendar.YEAR) - 80;
        }
        century = centuryStartYear / 100 * 100;
        startYear = centuryStartYear - century;

        init(definingCalendar);
    }

    private void init(Calendar definingCalendar) {

        final StringBuilder regex = new StringBuilder();
        final List<Strategy> collector = new ArrayList<Strategy>();

        final Matcher patternMatcher = formatPattern.matcher(pattern);
        if (!patternMatcher.lookingAt()) {
            throw new IllegalArgumentException(
                    "Illegal pattern character '" + pattern.charAt(patternMatcher.regionStart()) + "'");
        }

        currentFormatField = patternMatcher.group();
        Strategy currentStrategy = getStrategy(currentFormatField, definingCalendar);
        for (; ; ) {
            patternMatcher.region(patternMatcher.end(), patternMatcher.regionEnd());
            if (!patternMatcher.lookingAt()) {
                nextStrategy = null;
                break;
            }
            final String nextFormatField = patternMatcher.group();
            nextStrategy = getStrategy(nextFormatField, definingCalendar);
            if (currentStrategy.addRegex(this, regex)) {
                collector.add(currentStrategy);
            }
            currentFormatField = nextFormatField;
            currentStrategy = nextStrategy;
        }
        if (patternMatcher.regionStart() != patternMatcher.regionEnd()) {
            throw new IllegalArgumentException("Failed to parse \"" + pattern + "\" ; gave up at index " + patternMatcher.regionStart());
        }
        if (currentStrategy.addRegex(this, regex)) {
            collector.add(currentStrategy);
        }
        currentFormatField = null;
        strategies = collector.toArray(new Strategy[collector.size()]);
        parsePattern = Pattern.compile(regex.toString());
    }

    @Override
    public String getPattern() {
        return pattern;
    }

    @Override
    public TimeZone getTimeZone() {
        return timeZone;
    }

    @Override
    public Locale getLocale() {
        return locale;
    }

    Pattern getParsePattern() {
        return parsePattern;
    }

    @Override
    public boolean equals(final Object obj) {
        if (!(obj instanceof FastDateParser)) {
            return false;
        }
        final FastDateParser other = (FastDateParser) obj;
        return pattern.equals(other.pattern)
                && timeZone.equals(other.timeZone)
                && locale.equals(other.locale);
    }

    @Override
    public int hashCode() {
        return pattern.hashCode() + 13 * (timeZone.hashCode() + 13 * locale.hashCode());
    }

    @Override
    public String toString() {
        return "FastDateParser[" + pattern + "," + locale + "," + timeZone.getID() + "]";
    }

    private void readObject(final ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();

        final Calendar definingCalendar = Calendar.getInstance(timeZone, locale);
        init(definingCalendar);
    }

    @Override
    public Object parseObject(final String source) throws ParseException {
        return parse(source);
    }

    @Override
    public Date parse(final String source) throws ParseException {
        final Date date = parse(source, new ParsePosition(0));
        if (date == null) {

            if (locale.equals(JAPANESE_IMPERIAL)) {
                throw new ParseException(
                        "(The " + locale + " locale does not support dates before 1868 AD)\n" +
                                "Unparseable date: \"" + source + "\" does not match " + parsePattern.pattern(), 0);
            }
            throw new ParseException("Unparseable date: \"" + source + "\" does not match " + parsePattern.pattern(), 0);
        }
        return date;
    }

    @Override
    public Object parseObject(final String source, final ParsePosition pos) {
        return parse(source, pos);
    }

    @Override
    public Date parse(final String source, final ParsePosition pos) {
        final int offset = pos.getIndex();
        final Matcher matcher = parsePattern.matcher(source.substring(offset));
        if (!matcher.lookingAt()) {
            return null;
        }

        final Calendar cal = Calendar.getInstance(timeZone, locale);
        cal.clear();

        for (int i = 0; i < strategies.length; ) {
            final Strategy strategy = strategies[i++];
            strategy.setCalendar(this, cal, matcher.group(i));
        }
        pos.setIndex(offset + matcher.end());
        return cal.getTime();
    }

    private static StringBuilder escapeRegex(final StringBuilder regex, final String value, final boolean unquote) {
        regex.append("\\Q");
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            switch (c) {
                case '\'':
                    if (unquote) {
                        if (++i == value.length()) {
                            return regex;
                        }
                        c = value.charAt(i);
                    }
                    break;
                case '\\':
                    if (++i == value.length()) {
                        break;
                    }

                    regex.append(c);
                    c = value.charAt(i);
                    if (c == 'E') {
                        regex.append("E\\\\E\\");
                        c = 'Q';
                    }
                    break;
                default:
                    break;
            }
            regex.append(c);
        }
        regex.append("\\E");
        return regex;
    }

    private static String[] getDisplayNameArray(int field, boolean isLong, Locale locale) {
        DateFormatSymbols dfs = new DateFormatSymbols(locale);
        switch (field) {
            case Calendar.AM_PM:
                return dfs.getAmPmStrings();
            case Calendar.DAY_OF_WEEK:
                return isLong ? dfs.getWeekdays() : dfs.getShortWeekdays();
            case Calendar.ERA:
                return dfs.getEras();
            case Calendar.MONTH:
                return isLong ? dfs.getMonths() : dfs.getShortMonths();
        }
        return null;
    }

    private static void insertValuesInMap(Map<String, Integer> map, String[] values) {
        if (values == null) {
            return;
        }
        for (int i = 0; i < values.length; ++i) {
            if (values[i] != null && values[i].length() > 0) {
                map.put(values[i], i);
            }
        }
    }

    private static Map<String, Integer> getDisplayNames(int field, Locale locale) {
        Map<String, Integer> result = new HashMap<String, Integer>();
        insertValuesInMap(result, getDisplayNameArray(field, false, locale));
        insertValuesInMap(result, getDisplayNameArray(field, true, locale));
        return result.isEmpty() ? null : result;
    }

    private static Map<String, Integer> getDisplayNames(final int field, final Calendar definingCalendar, final Locale locale) {
        return getDisplayNames(field, locale);
    }

    private int adjustYear(final int twoDigitYear) {
        int trial = century + twoDigitYear;
        return twoDigitYear >= startYear ? trial : trial + 100;
    }

    boolean isNextNumber() {
        return nextStrategy != null && nextStrategy.isNumber();
    }

    int getFieldWidth() {
        return currentFormatField.length();
    }

    private static abstract class Strategy {

        boolean isNumber() {
            return false;
        }

        void setCalendar(final FastDateParser parser, final Calendar cal, final String value) {

        }

        abstract boolean addRegex(FastDateParser parser, StringBuilder regex);
    }

    private static final Pattern formatPattern = Pattern.compile(
            "D+|E+|F+|G+|H+|K+|M+|L+|S+|W+|Z+|a+|d+|h+|k+|m+|s+|w+|y+|z+|''|'[^']++(''[^']*+)*+'|[^'A-Za-z]++");

    private Strategy getStrategy(final String formatField, final Calendar definingCalendar) {
        switch (formatField.charAt(0)) {
            case '\'':
                if (formatField.length() > 2) {
                    return new CopyQuotedStrategy(formatField.substring(1, formatField.length() - 1));
                }

            default:
                return new CopyQuotedStrategy(formatField);
            case 'D':
                return DAY_OF_YEAR_STRATEGY;
            case 'E':
                return getLocaleSpecificStrategy(Calendar.DAY_OF_WEEK, definingCalendar);
            case 'F':
                return DAY_OF_WEEK_IN_MONTH_STRATEGY;
            case 'G':
                return getLocaleSpecificStrategy(Calendar.ERA, definingCalendar);
            case 'H':
                return MODULO_HOUR_OF_DAY_STRATEGY;
            case 'K':
                return HOUR_STRATEGY;
            case 'M':
            case 'L':
                return formatField.length() >= 3 ? getLocaleSpecificStrategy(Calendar.MONTH, definingCalendar) : NUMBER_MONTH_STRATEGY;
            case 'S':
                return MILLISECOND_STRATEGY;
            case 'W':
                return WEEK_OF_MONTH_STRATEGY;
            case 'a':
                return getLocaleSpecificStrategy(Calendar.AM_PM, definingCalendar);
            case 'd':
                return DAY_OF_MONTH_STRATEGY;
            case 'h':
                return MODULO_HOUR_STRATEGY;
            case 'k':
                return HOUR_OF_DAY_STRATEGY;
            case 'm':
                return MINUTE_STRATEGY;
            case 's':
                return SECOND_STRATEGY;
            case 'w':
                return WEEK_OF_YEAR_STRATEGY;
            case 'y':
                return formatField.length() > 2 ? LITERAL_YEAR_STRATEGY : ABBREVIATED_YEAR_STRATEGY;
            case 'Z':
            case 'z':
                return getLocaleSpecificStrategy(Calendar.ZONE_OFFSET, definingCalendar);
        }
    }

    @SuppressWarnings("unchecked")
    private static final ConcurrentMap<Locale, Strategy>[] caches = new ConcurrentMap[Calendar.FIELD_COUNT];

    private static ConcurrentMap<Locale, Strategy> getCache(final int field) {
        synchronized (caches) {
            if (caches[field] == null) {
                caches[field] = new ConcurrentHashMap<Locale, Strategy>(3);
            }
            return caches[field];
        }
    }

    private Strategy getLocaleSpecificStrategy(final int field, final Calendar definingCalendar) {
        final ConcurrentMap<Locale, Strategy> cache = getCache(field);
        Strategy strategy = cache.get(locale);
        if (strategy == null) {
            strategy = field == Calendar.ZONE_OFFSET
                    ? new TimeZoneStrategy(locale)
                    : new TextStrategy(field, definingCalendar, locale);
            final Strategy inCache = cache.putIfAbsent(locale, strategy);
            if (inCache != null) {
                return inCache;
            }
        }
        return strategy;
    }

    private static class CopyQuotedStrategy extends Strategy {
        private final String formatField;

        CopyQuotedStrategy(final String formatField) {
            this.formatField = formatField;
        }

        @Override
        boolean isNumber() {
            char c = formatField.charAt(0);
            if (c == '\'') {
                c = formatField.charAt(1);
            }
            return Character.isDigit(c);
        }

        @Override
        boolean addRegex(final FastDateParser parser, final StringBuilder regex) {
            escapeRegex(regex, formatField, true);
            return false;
        }
    }

    private static class TextStrategy extends Strategy {
        private final int field;
        private final Map<String, Integer> keyValues;

        TextStrategy(final int field, final Calendar definingCalendar, final Locale locale) {
            this.field = field;
            this.keyValues = getDisplayNames(field, definingCalendar, locale);
        }

        @Override
        boolean addRegex(final FastDateParser parser, final StringBuilder regex) {
            regex.append('(');
            for (final String textKeyValue : keyValues.keySet()) {
                escapeRegex(regex, textKeyValue, false).append('|');
            }
            regex.setCharAt(regex.length() - 1, ')');
            return true;
        }

        @Override
        void setCalendar(final FastDateParser parser, final Calendar cal, final String value) {
            final Integer iVal = keyValues.get(value);
            if (iVal == null) {
                final StringBuilder sb = new StringBuilder(value);
                sb.append(" not in (");
                for (final String textKeyValue : keyValues.keySet()) {
                    sb.append(textKeyValue).append(' ');
                }
                sb.setCharAt(sb.length() - 1, ')');
                throw new IllegalArgumentException(sb.toString());
            }
            cal.set(field, iVal.intValue());
        }
    }

    private static class NumberStrategy extends Strategy {
        private final int field;

        NumberStrategy(final int field) {
            this.field = field;
        }

        @Override
        boolean isNumber() {
            return true;
        }

        @Override
        boolean addRegex(final FastDateParser parser, final StringBuilder regex) {

            if (parser.isNextNumber()) {
                regex.append("(\\p{Nd}{").append(parser.getFieldWidth()).append("}+)");
            } else {
                regex.append("(\\p{Nd}++)");
            }
            return true;
        }

        @Override
        void setCalendar(final FastDateParser parser, final Calendar cal, final String value) {
            cal.set(field, modify(Integer.parseInt(value)));
        }

        int modify(final int iValue) {
            return iValue;
        }
    }

    private static final Strategy ABBREVIATED_YEAR_STRATEGY = new NumberStrategy(Calendar.YEAR) {

        @Override
        void setCalendar(final FastDateParser parser, final Calendar cal, final String value) {
            int iValue = Integer.parseInt(value);
            if (iValue < 100) {
                iValue = parser.adjustYear(iValue);
            }
            cal.set(Calendar.YEAR, iValue);
        }
    };

    private static class TimeZoneStrategy extends Strategy {

        private final String validTimeZoneChars;
        private final SortedMap<String, TimeZone> tzNames = new TreeMap<String, TimeZone>(String.CASE_INSENSITIVE_ORDER);

        private static final int ID = 0;

        private static final int LONG_STD = 1;

        private static final int SHORT_STD = 2;

        private static final int LONG_DST = 3;

        private static final int SHORT_DST = 4;

        TimeZoneStrategy(final Locale locale) {
            final String[][] zones = DateFormatSymbols.getInstance(locale).getZoneStrings();
            for (String[] zone : zones) {
                if (zone[ID].startsWith("GMT")) {
                    continue;
                }
                final TimeZone tz = TimeZone.getTimeZone(zone[ID]);
                if (!tzNames.containsKey(zone[LONG_STD])) {
                    tzNames.put(zone[LONG_STD], tz);
                }
                if (!tzNames.containsKey(zone[SHORT_STD])) {
                    tzNames.put(zone[SHORT_STD], tz);
                }
                if (tz.useDaylightTime()) {
                    if (!tzNames.containsKey(zone[LONG_DST])) {
                        tzNames.put(zone[LONG_DST], tz);
                    }
                    if (!tzNames.containsKey(zone[SHORT_DST])) {
                        tzNames.put(zone[SHORT_DST], tz);
                    }
                }
            }

            final StringBuilder sb = new StringBuilder();
            sb.append("(GMT[+\\-]\\d{0,1}\\d{2}|[+\\-]\\d{2}:?\\d{2}|");
            for (final String id : tzNames.keySet()) {
                escapeRegex(sb, id, false).append('|');
            }
            sb.setCharAt(sb.length() - 1, ')');
            validTimeZoneChars = sb.toString();
        }

        @Override
        boolean addRegex(final FastDateParser parser, final StringBuilder regex) {
            regex.append(validTimeZoneChars);
            return true;
        }

        @Override
        void setCalendar(final FastDateParser parser, final Calendar cal, final String value) {
            TimeZone tz;
            if (value.charAt(0) == '+' || value.charAt(0) == '-') {
                tz = TimeZone.getTimeZone("GMT" + value);
            } else if (value.startsWith("GMT")) {
                tz = TimeZone.getTimeZone(value);
            } else {
                tz = tzNames.get(value);
                if (tz == null) {
                    throw new IllegalArgumentException(value + " is not a supported timezone name");
                }
            }
            cal.setTimeZone(tz);
        }
    }

    private static final Strategy NUMBER_MONTH_STRATEGY = new NumberStrategy(Calendar.MONTH) {
        @Override
        int modify(final int iValue) {
            return iValue - 1;
        }
    };
    private static final Strategy LITERAL_YEAR_STRATEGY = new NumberStrategy(Calendar.YEAR);
    private static final Strategy WEEK_OF_YEAR_STRATEGY = new NumberStrategy(Calendar.WEEK_OF_YEAR);
    private static final Strategy WEEK_OF_MONTH_STRATEGY = new NumberStrategy(Calendar.WEEK_OF_MONTH);
    private static final Strategy DAY_OF_YEAR_STRATEGY = new NumberStrategy(Calendar.DAY_OF_YEAR);
    private static final Strategy DAY_OF_MONTH_STRATEGY = new NumberStrategy(Calendar.DAY_OF_MONTH);
    private static final Strategy DAY_OF_WEEK_IN_MONTH_STRATEGY = new NumberStrategy(Calendar.DAY_OF_WEEK_IN_MONTH);
    private static final Strategy HOUR_OF_DAY_STRATEGY = new NumberStrategy(Calendar.HOUR_OF_DAY);
    private static final Strategy MODULO_HOUR_OF_DAY_STRATEGY = new NumberStrategy(Calendar.HOUR_OF_DAY) {
        @Override
        int modify(final int iValue) {
            return iValue % 24;
        }
    };
    private static final Strategy MODULO_HOUR_STRATEGY = new NumberStrategy(Calendar.HOUR) {
        @Override
        int modify(final int iValue) {
            return iValue % 12;
        }
    };
    private static final Strategy HOUR_STRATEGY = new NumberStrategy(Calendar.HOUR);
    private static final Strategy MINUTE_STRATEGY = new NumberStrategy(Calendar.MINUTE);
    private static final Strategy SECOND_STRATEGY = new NumberStrategy(Calendar.SECOND);
    private static final Strategy MILLISECOND_STRATEGY = new NumberStrategy(Calendar.MILLISECOND);
}
