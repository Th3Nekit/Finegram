package com.th3nekit.finegram.chats.ui;

import static org.telegram.messenger.AndroidUtilities.dp;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.SparseIntArray;
import android.view.MotionEvent;
import android.view.View;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.configs.FinegramChatsConfig;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.AnimatedFloat;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.blur3.BlurredBackgroundDrawableViewFactory;
import org.telegram.ui.Components.blur3.drawable.BlurredBackgroundDrawable;
import org.telegram.ui.Components.blur3.drawable.color.BlurredBackgroundColorProvider;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Locale;

@SuppressLint("ViewConstructor")
public class FGTimelineScrubber extends View {

    public static final int MARK_MENTION = 0;
    public static final int MARK_REACTION = 1;
    public static final int MARK_PINNED = 2;

    private static final float TAU = 86400f;
    private static final int MIN_SPAN = 2 * 86400;
    private static final int WEEK = 7 * 86400;
    private static final int MONTH = 30 * 86400;
    private static final long FAST_WINDOW_MS = 350;
    private static final long HIDE_DELAY_MS = 2000;
    private static final long RETRY_DELAY_MS = 30000;
    private static final float FAST_SCREENS = 0.75f;
    private static final float LONG_SCREENS = 1.5f;
    private static final long LATE_SHOW_MS = 1000;
    private static final int UNREAD_LIMIT = 100;
    private static final int TOUCH_WIDTH_DP = 48;
    private static final int EDGE_DP = 6;
    private static final int NOW_ZONE_DP = 14;
    private static final int SNAP_DP = 16;
    private static final int LABEL_GAP_DP = 22;
    private static final int MIN_RAIL_DP = 120;
    private static final int CAPSULE_MARGIN_DP = 16;
    private static final String[] MARK_PREFIX = {"@ ", "❤ ", "📌 "};

    public static class Mark {
        public final int type;
        public final int messageId;
        public final int date;
        public final String preview;

        public Mark(int type, int messageId, int date, CharSequence preview) {
            this.type = type;
            this.messageId = messageId;
            this.date = date;
            this.preview = preview == null ? "" : preview.toString().replace('\n', ' ').trim();
        }
    }

    public interface Delegate {
        int getRailTop();

        int getRailBottom();

        int getAnchorDate();

        boolean isAtBottom();

        int getThreadId();

        void collectPinnedMarks(ArrayList<Mark> out);

        int getUnreadMarksVersion();

        int getKnownOldestDate();

        boolean isThread();

        int getTopicId();

        void jumpToDate(int date);

        void jumpToMessage(int messageId);

        void jumpToLatest();
    }

    private static class Label {
        final float fraction;
        final String text;
        final int priority;
        float y;
        boolean accepted;

        Label(float fraction, String text, int priority) {
            this.fraction = fraction;
            this.text = text;
            this.priority = priority;
        }
    }

    private final int currentAccount;
    private final long dialogId;
    private final int classGuid;
    private final Theme.ResourcesProvider resourcesProvider;
    private final Delegate delegate;

    private final Paint railPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint labelPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint handlePaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint titlePaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final TextPaint subtitlePaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final RectF handleRect = new RectF();
    private final RectF bubbleRect = new RectF();
    private final Rect glassBounds = new Rect();
    private final BlurredBackgroundDrawable handleGlass;
    private final BlurredBackgroundDrawable columnGlass;

    private final AnimatedFloat visibility;
    private final AnimatedFloat expansion;
    private final AnimatedFloat thumb;

    private final ArrayList<Mark> marks = new ArrayList<>();
    private final ArrayList<Mark> unreadMarks = new ArrayList<>();
    private final ArrayList<Label> labels = new ArrayList<>();

    private int oldestDate;
    private boolean oldestLoading;
    private boolean unavailable;
    private long retryAt;
    private int gestureDistance;
    private int unreadVersion = Integer.MIN_VALUE;
    private int now;
    private boolean shown;
    private boolean scrubbing;
    private long windowStart;
    private int windowDistance;
    private long lastScrollTime;
    private float fingerY;
    private float railTop, railBottom;
    private Mark snapped;

    private boolean listTouchActive;
    private float contentShift;
    private final Runnable hideRunnable = () -> {
        if (!listTouchActive && !scrubbing) {
            setShown(false);
        }
    };
    private final boolean lowPerformance = SharedConfig.getDevicePerformanceClass() == SharedConfig.PERFORMANCE_CLASS_LOW;
    private int bubbleDate;
    private String bubbleDateText;
    private Mark bubbleMark;
    private CharSequence bubbleMarkText;
    private float bubbleMaxWidth;
    private boolean nowZone;
    private float columnWidth;
    private int handleDay = -1;
    private String handleText = "";
    private SimpleDateFormat monthYearFormat;
    private final SparseIntArray resolvedDays = new SparseIntArray();
    private int pendingDay;
    private int resolvingDay;
    private final Runnable resolveRunnable = this::resolvePendingDay;

    public FGTimelineScrubber(Context context, int currentAccount, long dialogId, int classGuid, Theme.ResourcesProvider resourcesProvider,
                              BlurredBackgroundDrawableViewFactory glassFactory, BlurredBackgroundColorProvider glassColors, Delegate delegate) {
        super(context);
        this.currentAccount = currentAccount;
        this.dialogId = dialogId;
        this.classGuid = classGuid;
        this.resourcesProvider = resourcesProvider;
        this.delegate = delegate;

        visibility = new AnimatedFloat(this, lowPerformance ? 150 : 220, CubicBezierInterpolator.EASE_OUT_QUINT);
        expansion = new AnimatedFloat(this, lowPerformance ? 160 : 260, CubicBezierInterpolator.EASE_OUT_QUINT);
        thumb = new AnimatedFloat(this, lowPerformance ? 0 : 180, CubicBezierInterpolator.EASE_OUT_QUINT);

        handleGlass = glassFactory.create(this, glassColors).setRadius(dp(15));
        columnGlass = glassFactory.create(this, glassColors).setRadius(dp(18));

        railPaint.setStyle(Paint.Style.STROKE);
        railPaint.setStrokeCap(Paint.Cap.ROUND);
        railPaint.setStrokeWidth(dp(3));
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(dp(1.5f));
        labelPaint.setTextSize(dp(11));
        handlePaint.setTextSize(dp(13));
        handlePaint.setTypeface(AndroidUtilities.bold());
        titlePaint.setTextSize(dp(14));
        titlePaint.setTypeface(AndroidUtilities.bold());
        subtitlePaint.setTextSize(dp(12));

        setVisibility(INVISIBLE);
    }

    public void onListScrolled(int dy, boolean byUser) {
        if (!FinegramChatsConfig.INSTANCE.getTimelineScrubber()) {
            if (shown) {
                setShown(false);
            }
            return;
        }
        if (dy == 0 || scrubbing) {
            return;
        }
        if (shown) {
            removeCallbacks(hideRunnable);
            invalidate();
        }
        if (!byUser) {
            return;
        }
        final long time = SystemClock.uptimeMillis();
        lastScrollTime = time;
        if (time - windowStart > FAST_WINDOW_MS) {
            windowStart = time;
            windowDistance = 0;
        }
        windowDistance += Math.abs(dy);
        gestureDistance += Math.abs(dy);
        if (!shown && (windowDistance > getHeight() * FAST_SCREENS || gestureDistance > getHeight() * LONG_SCREENS)) {
            requestShow();
        }
    }

    public void setListTouchActive(boolean active) {
        if (listTouchActive == active) {
            return;
        }
        listTouchActive = active;
        if (active) {
            removeCallbacks(hideRunnable);
        } else if (shown && !scrubbing) {
            removeCallbacks(hideRunnable);
            postDelayed(hideRunnable, HIDE_DELAY_MS);
        }
    }

    public void onContentShift(float shift) {
        if (shift != contentShift) {
            contentShift = shift;
            if (shown) {
                invalidate();
            }
        }
    }

    public void onListIdle() {
        gestureDistance = 0;
        if (shown && !scrubbing && !listTouchActive) {
            removeCallbacks(hideRunnable);
            postDelayed(hideRunnable, HIDE_DELAY_MS);
        }
    }

    private void requestShow() {
        if (unavailable || SystemClock.uptimeMillis() < retryAt) {
            return;
        }
        if (oldestDate == 0) {
            final int known = delegate.getKnownOldestDate();
            if (known > 0) {
                oldestDate = known;
            } else if (delegate.isThread()) {
                retryAt = SystemClock.uptimeMillis() + RETRY_DELAY_MS / 6;
                return;
            } else {
                loadOldest();
                return;
            }
        }
        now = ConnectionsManager.getInstance(currentAccount).getCurrentTime();
        if (now - oldestDate < MIN_SPAN) {
            return;
        }
        refreshMarks();
        buildLabels();
        setShown(true);
        if (!scrubbing) {
            postDelayed(hideRunnable, HIDE_DELAY_MS * 2);
        }
    }

    private void setShown(boolean value) {
        removeCallbacks(hideRunnable);
        if (shown == value) {
            return;
        }
        shown = value;
        if (value) {
            setVisibility(VISIBLE);
        }
        invalidate();
    }

    private void loadOldest() {
        if (oldestLoading) {
            return;
        }
        final TLRPC.InputPeer peer = MessagesController.getInstance(currentAccount).getInputPeer(dialogId);
        if (peer == null) {
            unavailable = true;
            return;
        }
        oldestLoading = true;
        final TLRPC.TL_messages_getHistory req = new TLRPC.TL_messages_getHistory();
        req.peer = peer;
        req.offset_id = 1;
        req.add_offset = -1;
        req.limit = 1;
        final int reqId = ConnectionsManager.getInstance(currentAccount).sendRequest(req, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
            oldestLoading = false;
            if (!(response instanceof TLRPC.messages_Messages)) {
                retryAt = SystemClock.uptimeMillis() + RETRY_DELAY_MS;
                return;
            }
            if (!((TLRPC.messages_Messages) response).messages.isEmpty()) {
                oldestDate = ((TLRPC.messages_Messages) response).messages.get(0).date;
            }
            if (oldestDate == 0) {
                unavailable = true;
                return;
            }
            if (SystemClock.uptimeMillis() - lastScrollTime < LATE_SHOW_MS) {
                requestShow();
            }
        }));
        ConnectionsManager.getInstance(currentAccount).bindRequestToGuid(reqId, classGuid);
    }

    private void refreshMarks() {
        final int version = delegate.getUnreadMarksVersion();
        if (version != unreadVersion) {
            unreadVersion = version;
            if (version < 0) {
                unreadMarks.clear();
            } else {
                loadUnread(MARK_MENTION);
                loadUnread(MARK_REACTION);
            }
        }
        rebuildMarks();
    }

    private void rebuildMarks() {
        marks.clear();
        delegate.collectPinnedMarks(marks);
        marks.addAll(unreadMarks);
        Collections.sort(marks, (a, b) -> Integer.compare(b.date, a.date));
        invalidate();
    }

    private void loadUnread(int type) {
        final TLRPC.InputPeer peer = MessagesController.getInstance(currentAccount).getInputPeer(dialogId);
        if (peer == null) {
            return;
        }
        final int topicId = delegate.getTopicId();
        final TLRPC.TL_messages_search req = new TLRPC.TL_messages_search();
        req.peer = peer;
        req.q = "";
        req.limit = UNREAD_LIMIT;
        if (type == MARK_MENTION) {
            req.filter = new TLRPC.TL_inputMessagesFilterMyMentions();
        } else {
            req.filter = new TLRPC.TL_inputMessagesFilterEmpty();
            req.from_id = new TLRPC.TL_inputPeerSelf();
        }
        if (topicId != 0) {
            req.flags |= 2;
            req.top_msg_id = topicId;
        }
        final int version = unreadVersion;
        final int reqId = ConnectionsManager.getInstance(currentAccount).sendRequest(req, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
            if (version != unreadVersion || !(response instanceof TLRPC.messages_Messages)) {
                return;
            }
            for (int i = unreadMarks.size() - 1; i >= 0; i--) {
                if (unreadMarks.get(i).type == type) {
                    unreadMarks.remove(i);
                }
            }
            for (TLRPC.Message message : ((TLRPC.messages_Messages) response).messages) {
                if (message == null || message.id == 0) {
                    continue;
                }
                if (type == MARK_REACTION && !hasForeignReactions(message)) {
                    continue;
                }
                final CharSequence text = TextUtils.isEmpty(message.message) ? LocaleController.getString(R.string.FG_Timeline_Media) : message.message;
                unreadMarks.add(new Mark(type, message.id, message.date, text));
            }
            rebuildMarks();
        }));
        ConnectionsManager.getInstance(currentAccount).bindRequestToGuid(reqId, classGuid);
    }

    private static boolean hasForeignReactions(TLRPC.Message message) {
        if (message.reactions == null || message.reactions.results == null) {
            return false;
        }
        for (TLRPC.ReactionCount result : message.reactions.results) {
            if (result != null && result.count - (result.chosen ? 1 : 0) > 0) {
                return true;
            }
        }
        return false;
    }

    private float span() {
        return Math.max(MIN_SPAN, now - oldestDate);
    }

    private float fractionForDate(int date) {
        final float age = Math.max(0, now - date);
        return (float) Math.min(1.0, Math.log1p(age / TAU) / Math.log1p(span() / TAU));
    }

    private int dateForFraction(float fraction) {
        final double age = TAU * Math.expm1(fraction * Math.log1p(span() / TAU));
        return (int) (now - age);
    }

    private float yForFraction(float fraction) {
        return railBottom - fraction * (railBottom - railTop);
    }

    private float fractionForY(float y) {
        if (railBottom <= railTop) {
            return 0f;
        }
        return Math.max(0f, Math.min(1f, (railBottom - y) / (railBottom - railTop)));
    }

    private static int dayStart(int date) {
        final Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(date * 1000L);
        calendar.set(Calendar.HOUR_OF_DAY, 0);
        calendar.set(Calendar.MINUTE, 0);
        calendar.set(Calendar.SECOND, 0);
        calendar.set(Calendar.MILLISECOND, 0);
        return (int) (calendar.getTimeInMillis() / 1000L);
    }

    private void buildLabels() {
        labels.clear();
        final float span = span();
        labels.add(new Label(0f, LocaleController.getString(R.string.FG_Timeline_Now), 0));
        final int today = dayStart(now);
        if (now - today > 3 * 3600 && today > oldestDate) {
            labels.add(new Label(fractionForDate(today), LocaleController.getString(R.string.FG_Timeline_Today), 1));
        }
        if (span > WEEK) {
            labels.add(new Label(fractionForDate(now - WEEK), LocaleController.getString(R.string.FG_Timeline_Week), 1));
        }
        if (span > MONTH) {
            labels.add(new Label(fractionForDate(now - MONTH), LocaleController.getString(R.string.FG_Timeline_Month), 1));
        }
        final Locale locale = currentLocale();
        final Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(now * 1000L);
        final int currentYear = calendar.get(Calendar.YEAR);
        calendar.setTimeInMillis(oldestDate * 1000L);
        final int oldestYear = calendar.get(Calendar.YEAR);
        for (int year = currentYear - 1; year >= oldestYear; year--) {
            calendar.clear();
            calendar.set(year, Calendar.JULY, 1);
            final int date = Math.max(oldestDate, (int) (calendar.getTimeInMillis() / 1000L));
            labels.add(new Label(fractionForDate(date), String.valueOf(year), 2));
        }
        final SimpleDateFormat monthFormat = new SimpleDateFormat("LLLL", locale);
        calendar.setTimeInMillis(now * 1000L);
        calendar.set(Calendar.DAY_OF_MONTH, 15);
        for (int i = 1; i <= 11; i++) {
            calendar.add(Calendar.MONTH, -1);
            final int date = (int) (calendar.getTimeInMillis() / 1000L);
            if (now - date <= MONTH + WEEK) {
                continue;
            }
            if (date < oldestDate) {
                break;
            }
            labels.add(new Label(fractionForDate(date), capitalize(monthFormat.format(calendar.getTime()), locale), 3));
        }
        Collections.sort(labels, (a, b) -> Integer.compare(a.priority, b.priority));
        handleDay = -1;
    }

    private static Locale currentLocale() {
        final Locale locale = LocaleController.getInstance().getCurrentLocale();
        return locale == null ? Locale.getDefault() : locale;
    }

    private static String capitalize(String text, Locale locale) {
        if (TextUtils.isEmpty(text)) {
            return "";
        }
        return text.substring(0, 1).toUpperCase(locale) + text.substring(1);
    }

    private String shortDate(int date) {
        final int day = dayStart(date);
        if (day == handleDay) {
            return handleText;
        }
        handleDay = day;
        final int today = dayStart(now);
        if (day >= today) {
            handleText = LocaleController.getString(R.string.FG_Timeline_Today);
        } else if (day >= today - 86400 - 3600) {
            handleText = LocaleController.getString(R.string.FG_Timeline_Yesterday);
        } else {
            final Calendar calendar = Calendar.getInstance();
            calendar.setTimeInMillis(now * 1000L);
            final int currentYear = calendar.get(Calendar.YEAR);
            calendar.setTimeInMillis(day * 1000L);
            if (calendar.get(Calendar.YEAR) == currentYear) {
                handleText = LocaleController.getInstance().getFormatterDayMonth().format(day * 1000L);
            } else {
                if (monthYearFormat == null) {
                    monthYearFormat = new SimpleDateFormat("d MMM yyyy", currentLocale());
                }
                handleText = monthYearFormat.format(calendar.getTime());
            }
        }
        return handleText;
    }

    private void layoutLabels() {
        final float gap = dp(LABEL_GAP_DP);
        float maxWidth = 0;
        for (int i = 0; i < labels.size(); i++) {
            final Label label = labels.get(i);
            label.y = yForFraction(label.fraction);
            label.accepted = true;
            for (int j = 0; j < i; j++) {
                final Label other = labels.get(j);
                if (other.accepted && Math.abs(other.y - label.y) < gap) {
                    label.accepted = false;
                    break;
                }
            }
            if (label.accepted) {
                maxWidth = Math.max(maxWidth, labelPaint.measureText(label.text));
            }
        }
        columnWidth = Math.max(dp(56), Math.min(dp(120), maxWidth + dp(34)));
    }

    private void updateFinger(float y, boolean first) {
        final float clamped = Math.max(railTop, Math.min(railBottom, y));
        final float previous = fingerY;
        final boolean wasNow = nowZone;
        nowZone = clamped >= railBottom - dp(NOW_ZONE_DP);
        Mark nearest = null;
        if (!nowZone) {
            float best = dp(SNAP_DP);
            for (int i = 0; i < marks.size(); i++) {
                final Mark mark = marks.get(i);
                final float distance = Math.abs(yForFraction(fractionForDate(mark.date)) - clamped);
                if (distance < best) {
                    best = distance;
                    nearest = mark;
                }
            }
        }
        boolean ticked = false;
        if (nearest != snapped) {
            snapped = nearest;
            if (!first && nearest != null) {
                AndroidUtilities.vibrateCursor(this);
                ticked = true;
            }
        }
        if (!first && nowZone != wasNow && nowZone) {
            AndroidUtilities.vibrateCursor(this);
            ticked = true;
        }
        fingerY = nowZone ? railBottom : snapped != null ? yForFraction(fractionForDate(snapped.date)) : clamped;
        if (!nowZone && snapped == null) {
            requestDay(fingerDay());
        }
        if (!first && !ticked && snapped == null && !nowZone) {
            for (int i = 0; i < labels.size(); i++) {
                final Label label = labels.get(i);
                if (label.accepted && (previous - label.y) * (fingerY - label.y) < 0) {
                    AndroidUtilities.vibrateCursor(this);
                    break;
                }
            }
        }
    }

    private int fingerDay() {
        return dayStart(dateForFraction(fractionForY(fingerY)));
    }

    private int actualDay(int day) {
        final int actual = resolvedDays.get(day, 0);
        return actual > 0 ? dayStart(actual) : day;
    }

    private void requestDay(int day) {
        if (resolvedDays.indexOfKey(day) >= 0 || day == resolvingDay) {
            return;
        }
        pendingDay = day;
        removeCallbacks(resolveRunnable);
        postDelayed(resolveRunnable, 120);
    }

    private void resolvePendingDay() {
        final int day = pendingDay;
        if (day == 0 || resolvingDay != 0 || resolvedDays.indexOfKey(day) >= 0) {
            return;
        }
        final TLRPC.InputPeer peer = MessagesController.getInstance(currentAccount).getInputPeer(dialogId);
        if (peer == null) {
            return;
        }
        resolvingDay = day;
        final int threadId = delegate.getThreadId();
        final org.telegram.tgnet.TLObject request;
        if (threadId != 0) {
            final TLRPC.TL_messages_getReplies req = new TLRPC.TL_messages_getReplies();
            req.peer = peer;
            req.msg_id = threadId;
            req.offset_date = day + 86400;
            req.limit = 1;
            request = req;
        } else {
            final TLRPC.TL_messages_getHistory req = new TLRPC.TL_messages_getHistory();
            req.peer = peer;
            req.offset_date = day + 86400;
            req.limit = 1;
            request = req;
        }
        final int reqId = ConnectionsManager.getInstance(currentAccount).sendRequest(request, (response, error) -> AndroidUtilities.runOnUIThread(() -> {
            resolvingDay = 0;
            if (response instanceof TLRPC.messages_Messages) {
                final ArrayList<TLRPC.Message> messages = ((TLRPC.messages_Messages) response).messages;
                resolvedDays.put(day, messages.isEmpty() ? Math.max(oldestDate, 1) : messages.get(0).date);
                invalidate();
            }
            if (pendingDay != day && scrubbing) {
                resolvePendingDay();
            }
        }));
        ConnectionsManager.getInstance(currentAccount).bindRequestToGuid(reqId, classGuid);
    }

    private void finishScrub(boolean commit) {
        scrubbing = false;
        removeCallbacks(resolveRunnable);
        if (commit) {
            if (nowZone) {
                delegate.jumpToLatest();
            } else if (snapped != null) {
                delegate.jumpToMessage(snapped.messageId);
            } else {
                delegate.jumpToDate(actualDay(fingerDay()));
            }
        }
        snapped = null;
        nowZone = false;
        removeCallbacks(hideRunnable);
        postDelayed(hideRunnable, HIDE_DELAY_MS);
        invalidate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            if (!shown || visibility.get() < 0.5f || railBottom - railTop < dp(MIN_RAIL_DP) || event.getX() < getWidth() - dp(TOUCH_WIDTH_DP)
                    || event.getY() < railTop - dp(SNAP_DP) || event.getY() > railBottom + dp(SNAP_DP)) {
                return false;
            }
            scrubbing = true;
            removeCallbacks(hideRunnable);
            if (getParent() != null) {
                getParent().requestDisallowInterceptTouchEvent(true);
            }
            layoutLabels();
            updateFinger(event.getY(), true);
            AndroidUtilities.vibrateCursor(this);
            invalidate();
            return true;
        }
        if (!scrubbing) {
            return false;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            updateFinger(event.getY(), false);
            invalidate();
        } else if (action == MotionEvent.ACTION_UP) {
            finishScrub(true);
        } else if (action == MotionEvent.ACTION_CANCEL) {
            finishScrub(false);
        }
        return true;
    }

    private void drawGlass(Canvas canvas, BlurredBackgroundDrawable glass, RectF bounds, float radius, float alpha) {
        glassBounds.set((int) bounds.left, (int) bounds.top, (int) Math.ceil(bounds.right), (int) Math.ceil(bounds.bottom));
        glass.setRadius(radius);
        glass.setBounds(glassBounds);
        glass.setAlpha((int) (255 * alpha));
        glass.draw(canvas);
    }

    private int markAccent;
    private final int[] markPalette = new int[3];

    private int markColor(int type) {
        final int accent = Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider);
        if (accent != markAccent || markPalette[MARK_PINNED] == 0) {
            markAccent = accent;
            final float[] hsl = new float[3];
            ColorUtils.colorToHSL(accent, hsl);
            markPalette[MARK_PINNED] = accent;
            if (hsl[1] < 0.18f) {
                markPalette[MARK_MENTION] = ColorUtils.blendARGB(accent, 0xFFFFFFFF, 0.35f);
                markPalette[MARK_REACTION] = ColorUtils.blendARGB(accent, 0xFF000000, 0.35f);
            } else {
                markPalette[MARK_MENTION] = shiftHue(hsl, 58f);
                markPalette[MARK_REACTION] = shiftHue(hsl, -58f);
            }
        }
        return markPalette[type];
    }

    private static int shiftHue(float[] hsl, float degrees) {
        final float[] shifted = {(hsl[0] + degrees + 360f) % 360f, Math.max(0.45f, hsl[1]), Math.min(0.62f, Math.max(0.42f, hsl[2]))};
        return ColorUtils.HSLToColor(shifted);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final float alpha = visibility.set(shown ? 1f : 0f);
        if (alpha <= 0f) {
            if (!shown) {
                post(() -> {
                    if (!shown && visibility.get() <= 0f) {
                        setVisibility(INVISIBLE);
                    }
                });
            }
            return;
        }
        final float center = getHeight() / 2f;
        final float half = Math.min(center - delegate.getRailTop() - dp(CAPSULE_MARGIN_DP), delegate.getRailBottom() - dp(CAPSULE_MARGIN_DP) - center);
        railTop = center - half + contentShift;
        railBottom = center + half + contentShift;
        if (railBottom - railTop < dp(MIN_RAIL_DP)) {
            return;
        }
        final float expand = expansion.set(scrubbing ? 1f : 0f);
        final int textColor = Theme.getColor(Theme.key_glass_defaultText, resourcesProvider);
        final float right = getWidth() - dp(EDGE_DP);
        if (expand > 0f && columnWidth == 0) {
            layoutLabels();
        }

        if (expand > 0f) {
            rect.set(right - AndroidUtilities.lerp(dp(6), columnWidth, expand), railTop - dp(14), right, railBottom + dp(14));
            drawGlass(canvas, columnGlass, rect, dp(18), alpha * expand);
        }

        final float trackX = AndroidUtilities.lerp(right - dp(3), right - dp(14), expand);
        railPaint.setColor(Theme.multAlpha(textColor, alpha * AndroidUtilities.lerp(0.22f, 0.35f, expand)));
        canvas.drawLine(trackX, railTop, trackX, railBottom, railPaint);

        final float markRadius = AndroidUtilities.lerp(dp(2.5f), dp(3.5f), expand);
        for (int i = 0; i < marks.size(); i++) {
            final Mark mark = marks.get(i);
            fillPaint.setColor(Theme.multAlpha(markColor(mark.type), alpha));
            canvas.drawCircle(trackX, yForFraction(fractionForDate(mark.date)), markRadius, fillPaint);
        }

        if (expand > 0f) {
            drawLabels(canvas, trackX - dp(10), alpha * expand, textColor);
        }

        final int anchorDate = delegate.getAnchorDate();
        final boolean atBottom = delegate.isAtBottom();
        final float target = scrubbing ? fingerY : atBottom ? railBottom : yForFraction(fractionForDate(anchorDate == 0 ? now : anchorDate));
        final float y = thumb.set(target, scrubbing || alpha < 1f);

        if (expand > 0f) {
            fillPaint.setColor(Theme.multAlpha(textColor, alpha * expand));
            canvas.drawCircle(trackX, y, dp(5), fillPaint);
        }

        final String shortText = shortDate(scrubbing ? actualDay(fingerDay()) : (anchorDate == 0 ? now : anchorDate));
        final float handleWidth = Math.max(dp(44), handlePaint.measureText(shortText) + dp(22));
        final float handleHeight = dp(30);
        handleRect.set(right - handleWidth, y - handleHeight / 2f, right, y + handleHeight / 2f);
        layoutBubble(y, right - columnWidth - dp(8));

        rect.set(
            AndroidUtilities.lerp(handleRect.left, bubbleRect.left, expand),
            AndroidUtilities.lerp(handleRect.top, bubbleRect.top, expand),
            AndroidUtilities.lerp(handleRect.right, bubbleRect.right, expand),
            AndroidUtilities.lerp(handleRect.bottom, bubbleRect.bottom, expand)
        );
        drawGlass(canvas, handleGlass, rect, AndroidUtilities.lerp(dp(15), dp(18), expand), alpha);

        if (expand < 1f) {
            final Paint.FontMetrics metrics = handlePaint.getFontMetrics();
            handlePaint.setColor(Theme.multAlpha(textColor, alpha * (1f - expand)));
            canvas.drawText(shortText, rect.centerX() - handlePaint.measureText(shortText) / 2f, rect.centerY() - (metrics.ascent + metrics.descent) / 2f, handlePaint);
        }
        if (expand > 0f) {
            drawBubbleText(canvas, rect, alpha * expand, textColor);
        }
    }

    private void drawLabels(Canvas canvas, float labelRight, float alpha, int textColor) {
        final Paint.FontMetrics metrics = labelPaint.getFontMetrics();
        labelPaint.setColor(Theme.multAlpha(textColor, alpha * 0.75f));
        for (int i = 0; i < labels.size(); i++) {
            final Label label = labels.get(i);
            if (!label.accepted) {
                continue;
            }
            final float width = labelPaint.measureText(label.text);
            canvas.drawText(label.text, labelRight - width, label.y - (metrics.ascent + metrics.descent) / 2f, labelPaint);
        }
    }

    private CharSequence bubbleTitle;
    private CharSequence bubbleSubtitle;

    private void layoutBubble(float y, float right) {
        final int date = snapped != null ? snapped.date : actualDay(fingerDay());
        if (bubbleDateText == null || date != bubbleDate) {
            bubbleDate = date;
            bubbleDateText = LocaleController.formatDateChat(date);
        }
        final float maxWidth = getWidth() * 0.55f;
        if (nowZone) {
            bubbleTitle = LocaleController.getString(R.string.FG_Timeline_Latest);
            bubbleSubtitle = null;
        } else if (snapped != null) {
            if (snapped != bubbleMark || maxWidth != bubbleMaxWidth) {
                bubbleMark = snapped;
                bubbleMaxWidth = maxWidth;
                bubbleMarkText = TextUtils.ellipsize(MARK_PREFIX[snapped.type] + snapped.preview, titlePaint, maxWidth, TextUtils.TruncateAt.END);
            }
            bubbleTitle = bubbleMarkText;
            bubbleSubtitle = bubbleDateText;
        } else {
            bubbleTitle = bubbleDateText;
            bubbleSubtitle = null;
        }
        final float titleWidth = titlePaint.measureText(bubbleTitle, 0, bubbleTitle.length());
        final float subtitleWidth = bubbleSubtitle == null ? 0 : subtitlePaint.measureText(bubbleSubtitle, 0, bubbleSubtitle.length());
        final Paint.FontMetrics titleMetrics = titlePaint.getFontMetrics();
        final Paint.FontMetrics subtitleMetrics = subtitlePaint.getFontMetrics();
        final float height = (titleMetrics.descent - titleMetrics.ascent) + (bubbleSubtitle == null ? 0 : subtitleMetrics.descent - subtitleMetrics.ascent) + dp(16);
        final float width = Math.max(titleWidth, subtitleWidth) + dp(28);
        final float top = Math.max(railTop - dp(14), Math.min(railBottom + dp(14) - height, y - height / 2f));
        bubbleRect.set(right - width, top, right, top + height);
    }

    private void drawBubbleText(Canvas canvas, RectF bounds, float alpha, int textColor) {
        if (bubbleTitle == null) {
            return;
        }
        final Paint.FontMetrics titleMetrics = titlePaint.getFontMetrics();
        final Paint.FontMetrics subtitleMetrics = subtitlePaint.getFontMetrics();
        final float titleHeight = titleMetrics.descent - titleMetrics.ascent;
        final float subtitleHeight = bubbleSubtitle == null ? 0 : subtitleMetrics.descent - subtitleMetrics.ascent;
        final float top = bounds.centerY() - (titleHeight + subtitleHeight) / 2f;
        final float left = bounds.left + dp(14);
        if (snapped != null) {
            fillPaint.setColor(Theme.multAlpha(markColor(snapped.type), alpha));
            canvas.drawCircle(bounds.left + dp(7), bounds.centerY(), dp(2.5f), fillPaint);
        }
        final boolean pending = !nowZone && snapped == null && resolvedDays.indexOfKey(fingerDay()) < 0;
        titlePaint.setColor(Theme.multAlpha(textColor, alpha * (pending ? 0.55f : 1f)));
        canvas.drawText(bubbleTitle, 0, bubbleTitle.length(), left, top - titleMetrics.ascent, titlePaint);
        if (bubbleSubtitle != null) {
            subtitlePaint.setColor(Theme.multAlpha(textColor, alpha * 0.7f));
            canvas.drawText(bubbleSubtitle, 0, bubbleSubtitle.length(), left, top + titleHeight - subtitleMetrics.ascent, subtitlePaint);
        }
    }
}
