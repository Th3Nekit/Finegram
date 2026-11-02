/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.Manifest;
import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PathMeasure;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.FileProvider;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AccountInstance;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.ChatObject;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MediaController;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.MessagesStorage;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SendMessagesHelper;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.Utilities;
import org.telegram.tgnet.TLObject;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ChatActivity;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.ScaleStateListAnimator;
import org.telegram.ui.DialogsActivity;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramMessagesConfig;

public class FGQuoteSheet extends BottomSheet {

    private static final int THEME_AUTO = 0;
    private static final int THEME_LIGHT = 1;
    private static final int THEME_DARK = 2;

    private static final int ACTION_SAVE = 0;
    private static final int ACTION_SHARE = 1;
    private static final int ACTION_FORWARD = 2;
    private static final int ACTION_SEND = 3;

    private static final long CACHE_LIFETIME = 24L * 60L * 60L * 1000L;

    private static final TimeInterpolator SETTLE = t -> {
        float k = 9f;
        float end = 1f - (1f + k) * (float) Math.exp(-k);
        return (1f - (1f + k * t) * (float) Math.exp(-k * t)) / end;
    };

    private final ChatActivity fragment;
    private final boolean chatDark;
    private final Bitmap wallpaper;
    private final int stageHeight;
    private final int accent;
    private final int accentText;
    private final int textColor;
    private final int surfaceColor;

    private final StageView stage;
    private final QuoteCanvas quoteCanvas;
    private final FGQuoteMaker maker;
    private final SwatchStrip swatches;
    private final ActionTile[] tiles = new ActionTile[3];
    private final ButtonWithCounterView sendButton;

    private boolean busy;
    private File exported;
    private FGQuoteMaker.Scene exportedScene;
    private boolean exportedSticker;
    private boolean exportedFlat;

    public static void show(ChatActivity fragment, List<MessageObject> messages) {
        if (fragment == null || fragment.getParentActivity() == null) {
            return;
        }
        if (fragment.isSecretChat() || fragment.isPeerNoForwards()) {
            showError(fragment, getString(R.string.FG_Quote_Protected));
            return;
        }
        ArrayList<MessageObject> prepared = FGQuoteMaker.prepare(messages);
        for (MessageObject message : prepared) {
            if (message.isNoForwards() || message.isSecretMedia()) {
                showError(fragment, getString(R.string.FG_Quote_Protected));
                return;
            }
        }
        if (prepared.size() > FGQuoteMaker.MAX_MESSAGES) {
            showError(fragment, LocaleController.formatString(R.string.FG_Quote_Limit, FGQuoteMaker.MAX_MESSAGES));
            return;
        }
        if (prepared.isEmpty()) {
            showError(fragment, getString(R.string.FG_Quote_Failed));
            return;
        }
        FGQuoteSheet sheet = new FGQuoteSheet(fragment, prepared);
        if (sheet.maker.isEmpty()) {
            sheet.maker.release();
            showError(fragment, getString(R.string.FG_Quote_Failed));
            return;
        }
        fragment.showDialog(sheet);
    }

    public static void showSelection(ChatActivity fragment) {
        if (fragment == null) {
            return;
        }
        ArrayList<MessageObject> selected = selectedOf(fragment);
        fragment.clearSelectionMode();
        show(fragment, selected);
    }

    public static boolean canQuoteSelection(ChatActivity fragment) {
        if (fragment == null || !FinegramMessagesConfig.INSTANCE.getCreateQuoteImage()
                || fragment.isSecretChat() || fragment.isPeerNoForwards()) {
            return false;
        }
        ArrayList<MessageObject> selected = selectedOf(fragment);
        if (selected.isEmpty() || selected.size() > FGQuoteMaker.MAX_MESSAGES) {
            return false;
        }
        for (MessageObject message : selected) {
            if (message.messageOwner == null || message.messageOwner.action != null
                    || message.isNoForwards() || message.isSecretMedia()) {
                return false;
            }
        }
        return true;
    }

    private static ArrayList<MessageObject> selectedOf(ChatActivity fragment) {
        ArrayList<MessageObject> result = new ArrayList<>();
        for (int a = 1; a >= 0; a--) {
            for (int b = 0; b < fragment.selectedMessagesIds[a].size(); b++) {
                MessageObject message = fragment.selectedMessagesIds[a].valueAt(b);
                if (message != null) {
                    result.add(message);
                }
            }
        }
        return result;
    }

    private static void showError(ChatActivity fragment, CharSequence text) {
        BulletinFactory.of(fragment).createSimpleBulletin(R.raw.error, text).show();
    }

    private FGQuoteSheet(ChatActivity fragment, ArrayList<MessageObject> messages) {
        super(fragment.getParentActivity(), false, fragment.getResourceProvider());
        this.fragment = fragment;
        Context context = getContext();
        Theme.ResourcesProvider provider = fragment.getResourceProvider();
        chatDark = provider != null ? provider.isDark() : Theme.isCurrentThemeDark();
        wallpaper = FGQuoteMaker.captureWallpaper(provider);
        accent = getThemedColor(Theme.key_featuredStickers_addButton);
        accentText = getThemedColor(Theme.key_featuredStickers_buttonText);
        textColor = getThemedColor(Theme.key_dialogTextBlack);
        surfaceColor = ColorUtils.blendARGB(getThemedColor(Theme.key_dialogBackground), textColor, 0.045f);
        stageHeight = Math.max(dp(200), Math.min(dp(380), AndroidUtilities.displaySize.y - dp(480)));

        fixNavigationBar(getThemedColor(Theme.key_dialogBackground));
        setApplyBottomPadding(false);
        cleanupCache();

        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setClipToPadding(false);
        root.setClipChildren(false);
        root.setPadding(dp(16), dp(16), dp(16), dp(12));

        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = new TextView(context);
        title.setText(getString(R.string.FG_Quote_Title));
        title.setTextColor(textColor);
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setLetterSpacing(-0.01f);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        header.addView(title, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f));
        if (messages.size() > 1) {
            TextView count = new TextView(context);
            count.setText(LocaleController.formatPluralString("messages", messages.size()));
            count.setTextColor(getThemedColor(Theme.key_dialogTextGray2));
            count.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 14);
            header.addView(count, LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT));
        }
        root.addView(header, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 4, 0, 4, 12));

        stage = new StageView(context);
        quoteCanvas = new QuoteCanvas(context);
        TLRPC.Chat chat = fragment.getCurrentChat();
        TLObject source = chat != null ? chat : fragment.getCurrentUser();
        maker = new FGQuoteMaker(quoteCanvas, fragment.getCurrentAccount(), messages, wallpaper, source);
        NotificationCenter.listenEmojiLoading(quoteCanvas);
        ScrollView scroll = new ScrollView(context) {
            @Override
            public boolean onInterceptTouchEvent(MotionEvent event) {
                holdTouch(this, event);
                return super.onInterceptTouchEvent(event);
            }

            @Override
            public boolean onTouchEvent(MotionEvent event) {
                holdTouch(this, event);
                return super.onTouchEvent(event);
            }
        };
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.addView(quoteCanvas, new ScrollView.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        stage.addView(scroll, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));
        root.addView(stage, new LinearLayout.LayoutParams(LayoutHelper.MATCH_PARENT, stageHeight));

        LinearLayout switches = new LinearLayout(context);
        switches.setOrientation(LinearLayout.HORIZONTAL);
        Segmented formatSwitch = new Segmented(context, new String[]{
                getString(R.string.FG_Quote_Picture),
                getString(R.string.FG_Quote_Sticker)
        }, false);
        formatSwitch.select(FinegramMessagesConfig.INSTANCE.getQuoteSticker() ? 1 : 0, false);
        formatSwitch.setListener(index -> {
            FinegramMessagesConfig.INSTANCE.setQuoteSticker(index == 1);
            stage.setBadge(badgeText(), true);
            updateSendButton();
        });
        switches.addView(formatSwitch, LayoutHelper.createLinear(0, 40, 1f));
        Segmented themeSwitch = new Segmented(context, new String[]{
                getString(R.string.FG_Quote_ThemeAuto),
                getString(R.string.FG_Quote_ThemeLight),
                getString(R.string.FG_Quote_ThemeDark)
        }, true);
        themeSwitch.select(FinegramMessagesConfig.INSTANCE.getQuoteTheme(), false);
        themeSwitch.setListener(index -> {
            FinegramMessagesConfig.INSTANCE.setQuoteTheme(index);
            applyStyle();
        });
        switches.addView(themeSwitch, LayoutHelper.createLinear(132, 40, 10, 0, 0, 0));
        root.addView(switches, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        swatches = new SwatchStrip(context);
        swatches.select(FinegramMessagesConfig.INSTANCE.getQuoteBackground(), false);
        swatches.setListener(index -> {
            FinegramMessagesConfig.INSTANCE.setQuoteBackground(index);
            applyStyle();
        });
        HorizontalScrollView swatchScroll = new HorizontalScrollView(context);
        swatchScroll.setHorizontalScrollBarEnabled(false);
        swatchScroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        swatchScroll.addView(swatches, new FrameLayout.LayoutParams(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT));
        root.addView(swatchScroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, -16, 10, -16, 0));

        LinearLayout chipRow = new LinearLayout(context);
        chipRow.setOrientation(LinearLayout.HORIZONTAL);
        chipRow.setPadding(dp(16), 0, dp(8), 0);
        addChip(chipRow, getString(R.string.FG_Quote_Avatars), FinegramMessagesConfig.INSTANCE.getQuoteAvatars(), value -> {
            FinegramMessagesConfig.INSTANCE.setQuoteAvatars(value);
            applyStyle();
        });
        addChip(chipRow, getString(R.string.FG_Quote_Names), FinegramMessagesConfig.INSTANCE.getQuoteNames(), value -> {
            FinegramMessagesConfig.INSTANCE.setQuoteNames(value);
            applyStyle();
        });
        addChip(chipRow, getString(R.string.FG_Quote_Time), FinegramMessagesConfig.INSTANCE.getQuoteTime(), value -> {
            FinegramMessagesConfig.INSTANCE.setQuoteTime(value);
            applyStyle();
        });
        addChip(chipRow, getString(R.string.FG_Quote_Replies), FinegramMessagesConfig.INSTANCE.getQuoteReplies(), value -> {
            FinegramMessagesConfig.INSTANCE.setQuoteReplies(value);
            applyStyle();
        });
        addChip(chipRow, getString(R.string.FG_Quote_Corners), FinegramMessagesConfig.INSTANCE.getQuoteRounded(), value -> {
            FinegramMessagesConfig.INSTANCE.setQuoteRounded(value);
            applyStyle();
        });
        if (source != null) {
            addChip(chipRow, getString(R.string.FG_Quote_Source), FinegramMessagesConfig.INSTANCE.getQuoteHeader(), value -> {
                FinegramMessagesConfig.INSTANCE.setQuoteHeader(value);
                applyStyle();
            });
        }
        HorizontalScrollView chipScroll = new HorizontalScrollView(context);
        chipScroll.setHorizontalScrollBarEnabled(false);
        chipScroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        chipScroll.addView(chipRow, new FrameLayout.LayoutParams(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT));
        root.addView(chipScroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, -16, 6, -16, 0));

        LinearLayout tileRow = new LinearLayout(context);
        tileRow.setOrientation(LinearLayout.HORIZONTAL);
        tiles[ACTION_SAVE] = new ActionTile(context, R.drawable.msg_download, getString(R.string.FG_Quote_Save), () -> export(ACTION_SAVE));
        tiles[ACTION_SHARE] = new ActionTile(context, R.drawable.msg_shareout, getString(R.string.FG_Quote_Share), () -> export(ACTION_SHARE));
        tiles[ACTION_FORWARD] = new ActionTile(context, R.drawable.msg_forward, getString(R.string.FG_Quote_Forward), () -> export(ACTION_FORWARD));
        for (int i = 0; i < tiles.length; i++) {
            tileRow.addView(tiles[i], LayoutHelper.createLinear(0, 64, 1f, i == 0 ? 0 : 8, 0, 0, 0));
        }
        root.addView(tileRow, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 16, 0, 0));

        sendButton = new ButtonWithCounterView(context, resourcesProvider).setRound();
        sendButton.setText(getString(R.string.FG_Quote_Send), false);
        sendButton.setOnClickListener(v -> export(ACTION_SEND));
        root.addView(sendButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 10, 0, 0));
        updateSendButton();

        setCustomView(root);
        quoteCanvas.setScene(maker.build(currentStyle()), false);
        swatches.setDark(currentStyle().dark);
        stage.setBadge(badgeText(), false);
    }

    private void addChip(LinearLayout row, String label, boolean checked, Utilities.Callback<Boolean> listener) {
        row.addView(new ToggleChip(getContext(), label, checked, listener),
                LayoutHelper.createLinear(LayoutHelper.WRAP_CONTENT, 36, 0, 0, 8, 0));
    }

    private static void holdTouch(ScrollView scroll, MotionEvent event) {
        ViewParent parent = scroll.getParent();
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN && parent != null
                && (scroll.canScrollVertically(1) || scroll.canScrollVertically(-1))) {
            parent.requestDisallowInterceptTouchEvent(true);
        }
    }

    @Override
    protected void appendOpenAnimator(boolean opening, ArrayList<Animator> animators) {
        super.appendOpenAnimator(opening, animators);
        if (!opening || !motion()) {
            return;
        }
        quoteCanvas.setPivotX(quoteCanvas.getWidth() > 0 ? quoteCanvas.getWidth() / 2f : AndroidUtilities.displaySize.x / 2f);
        quoteCanvas.setPivotY(stageHeight / 2f);
        applyReveal(0f);
        ValueAnimator reveal = ValueAnimator.ofFloat(0f, 1f);
        reveal.addUpdateListener(a -> applyReveal(Math.max(0f, ((float) a.getAnimatedValue() - 0.12f) / 0.88f)));
        reveal.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                applyReveal(1f);
            }
        });
        animators.add(reveal);
    }

    private void applyReveal(float value) {
        quoteCanvas.setAlpha(value);
        float scale = AndroidUtilities.lerp(0.94f, 1f, value);
        quoteCanvas.setScaleX(scale);
        quoteCanvas.setScaleY(scale);
        quoteCanvas.setTranslationY(dp(14) * (1f - value));
    }

    @Override
    public void dismissInternal() {
        super.dismissInternal();
        maker.release();
    }

    private FGQuoteMaker.Style currentStyle() {
        int theme = FinegramMessagesConfig.INSTANCE.getQuoteTheme();
        boolean dark = theme == THEME_DARK || theme != THEME_LIGHT && chatDark;
        int background = FinegramMessagesConfig.INSTANCE.getQuoteBackground();
        if (background < 0 || background >= FGQuoteMaker.BACKGROUND_COUNT) {
            background = FGQuoteMaker.BACKGROUND_WALLPAPER;
        }
        return new FGQuoteMaker.Style(
                dark,
                background,
                FinegramMessagesConfig.INSTANCE.getQuoteAvatars(),
                FinegramMessagesConfig.INSTANCE.getQuoteNames(),
                FinegramMessagesConfig.INSTANCE.getQuoteTime(),
                FinegramMessagesConfig.INSTANCE.getQuoteReplies(),
                FinegramMessagesConfig.INSTANCE.getQuoteRounded(),
                FinegramMessagesConfig.INSTANCE.getQuoteHeader()
        );
    }

    private void applyStyle() {
        FGQuoteMaker.Style style = currentStyle();
        quoteCanvas.setScene(maker.build(style), true);
        swatches.setDark(style.dark);
        stage.setBadge(badgeText(), true);
    }

    private String badgeText() {
        FGQuoteMaker.Scene scene = quoteCanvas.scene;
        if (scene == null) {
            return null;
        }
        boolean sticker = FinegramMessagesConfig.INSTANCE.getQuoteSticker();
        float scale = maker.exportScale(scene, sticker);
        int width = Math.max(1, Math.round(scene.getWidth() * scale));
        int height = Math.max(1, Math.round(scene.getHeight() * scale));
        if (sticker) {
            width = Math.min(width, 512);
            height = Math.min(height, 512);
        }
        return width + " × " + height + (sticker ? " · WEBP" : " · PNG");
    }

    private boolean canSendAnything() {
        return !fragment.isInScheduleMode() && !fragment.isReport() && fragment.canSendMessage()
                && fragment.getMessagesController().getSendPaidMessagesStars(fragment.getDialogId()) <= 0;
    }

    private boolean canSendFormat() {
        TLRPC.Chat chat = fragment.getCurrentChat();
        if (chat == null) {
            return true;
        }
        return FinegramMessagesConfig.INSTANCE.getQuoteSticker() ? ChatObject.canSendStickers(chat) : ChatObject.canSendPhoto(chat);
    }

    private void updateSendButton() {
        sendButton.setVisibility(canSendAnything() ? View.VISIBLE : View.GONE);
        sendButton.setEnabled(canSendFormat() && !busy);
    }

    private void setBusy(int action, boolean value) {
        busy = value;
        for (int i = 0; i < tiles.length; i++) {
            tiles[i].setLoading(value && action == i);
            tiles[i].setEnabled(!value);
        }
        sendButton.setLoading(value && action == ACTION_SEND);
        sendButton.setEnabled(!value && canSendFormat());
    }

    private void export(int action) {
        FGQuoteMaker.Scene scene = quoteCanvas.scene;
        if (busy || scene == null) {
            return;
        }
        boolean sticker = FinegramMessagesConfig.INSTANCE.getQuoteSticker();
        boolean flat = !sticker && (action == ACTION_FORWARD || action == ACTION_SEND && !sendsAsFile(scene));
        if (exported != null && exportedScene == scene && exportedSticker == sticker && exportedFlat == flat && exported.isFile()) {
            deliver(action, exported, sticker, scene);
            return;
        }
        setBusy(action, true);
        quoteCanvas.post(() -> {
            Bitmap bitmap = maker.render(scene, sticker, flat);
            if (bitmap == null) {
                setBusy(action, false);
                fail();
                return;
            }
            Utilities.globalQueue.postRunnable(() -> {
                File file = write(bitmap, sticker);
                bitmap.recycle();
                AndroidUtilities.runOnUIThread(() -> {
                    setBusy(action, false);
                    if (file == null) {
                        fail();
                        return;
                    }
                    exported = file;
                    exportedScene = scene;
                    exportedSticker = sticker;
                    exportedFlat = flat;
                    deliver(action, file, sticker, scene);
                });
            });
        });
    }

    private void deliver(int action, File file, boolean sticker, FGQuoteMaker.Scene scene) {
        if (!isShowing()) {
            return;
        }
        switch (action) {
            case ACTION_SAVE:
                save(file);
                break;
            case ACTION_SHARE:
                share(file, sticker);
                break;
            case ACTION_FORWARD:
                forward(file, sticker);
                break;
            case ACTION_SEND:
                sendHere(file, sticker, scene);
                break;
        }
    }

    private void save(File file) {
        Activity activity = fragment.getParentActivity();
        if (activity == null) {
            return;
        }
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P
                && activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 4);
            return;
        }
        MediaController.saveFile(file.getAbsolutePath(), activity, 0, null, null, uri -> {
            if (isShowing()) {
                BulletinFactory.of(container, resourcesProvider)
                        .createSimpleBulletin(R.raw.ic_save_to_gallery, getString(R.string.FG_Quote_Saved)).show();
            } else {
                BulletinFactory.of(fragment)
                        .createSimpleBulletin(R.raw.ic_save_to_gallery, getString(R.string.FG_Quote_Saved)).show();
            }
        });
    }

    private void share(File file, boolean sticker) {
        Activity activity = fragment.getParentActivity();
        if (activity == null) {
            return;
        }
        try {
            Uri uri = FileProvider.getUriForFile(activity, ApplicationLoader.getApplicationId() + ".provider", file);
            Intent intent = new Intent(Intent.ACTION_SEND);
            intent.setType(sticker ? "image/webp" : "image/png");
            intent.putExtra(Intent.EXTRA_STREAM, uri);
            intent.setClipData(ClipData.newRawUri(null, uri));
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            activity.startActivity(Intent.createChooser(intent, getString(R.string.FG_Quote_Share)));
        } catch (Throwable e) {
            FinegramLogger.e("FGQuote", () -> "цитата не отправилась", e);
            fail();
        }
    }

    private void forward(File file, boolean sticker) {
        String path = file.getAbsolutePath();
        Bundle args = new Bundle();
        args.putBoolean("onlySelect", true);
        args.putInt("dialogsType", DialogsActivity.DIALOGS_TYPE_FORWARD);
        args.putBoolean("canSelectTopics", true);
        DialogsActivity chooser = new DialogsActivity(args);
        chooser.setDelegate((picker, dialogs, message, param, notify, scheduleDate, scheduleRepeatPeriod, topicsFragment) -> {
            if (dialogs == null || dialogs.isEmpty()) {
                return false;
            }
            AccountInstance account = fragment.getAccountInstance();
            for (MessagesStorage.TopicKey key : dialogs) {
                MessageObject topic = topicOf(key);
                if (sticker) {
                    SendMessagesHelper.prepareSendingDocument(account, path, path, null, null, null, key.dialogId,
                            topic, topic, null, null, null, notify, scheduleDate, null, null, false);
                } else {
                    SendMessagesHelper.prepareSendingPhoto(account, path, null, key.dialogId, topic, topic, null,
                            null, null, null, null, 0, null, notify, scheduleDate, 0, null);
                }
            }
            if (topicsFragment != null) {
                topicsFragment.finishFragment();
            }
            picker.finishFragment();
            BulletinFactory.of(fragment).createSimpleBulletin(R.raw.forward, getString(R.string.FG_Quote_Sent)).show();
            return true;
        });
        dismiss();
        fragment.presentFragment(chooser);
    }

    private MessageObject topicOf(MessagesStorage.TopicKey key) {
        if (key.topicId == 0) {
            return null;
        }
        TLRPC.TL_forumTopic topic = fragment.getMessagesController().getTopicsController().findTopic(-key.dialogId, key.topicId);
        if (topic == null || topic.topicStartMessage == null) {
            return null;
        }
        MessageObject message = new MessageObject(fragment.getCurrentAccount(), topic.topicStartMessage, false, false);
        message.isTopicMainMessage = true;
        return message;
    }

    private void sendHere(File file, boolean sticker, FGQuoteMaker.Scene scene) {
        if (!canSendAnything() || !canSendFormat()) {
            return;
        }
        String path = file.getAbsolutePath();
        AccountInstance account = fragment.getAccountInstance();
        long dialogId = fragment.getDialogId();
        MessageObject thread = fragment.getThreadMessage();
        if (sticker) {
            SendMessagesHelper.prepareSendingDocument(account, path, path, null, null, null, dialogId,
                    null, thread, null, null, null, true, 0, null, fragment.getMessageChatSendParams(), false);
        } else {
            SendMessagesHelper.prepareSendingPhoto(account, path, null, null, dialogId, null, thread, null, null,
                    null, null, null, 0, null, null, true, 0, 0, fragment.getChatMode(), sendsAsFile(scene), null,
                    fragment.getMessageChatSendParams(), 0, 0, fragment.getSendMonoForumPeerId(),
                    fragment.getSendMessageSuggestionParams());
        }
        dismiss();
    }

    private boolean sendsAsFile(FGQuoteMaker.Scene scene) {
        TLRPC.Chat chat = fragment.getCurrentChat();
        return scene.getHeight() > scene.getWidth() * 3 && (chat == null || ChatObject.canSendDocument(chat));
    }

    private void fail() {
        BulletinFactory.of(container, resourcesProvider).createErrorBulletin(getString(R.string.FG_Quote_Failed)).show();
    }

    private static File cacheDirectory() {
        return new File(AndroidUtilities.getSharingDirectory(), "quotes");
    }

    private static void cleanupCache() {
        Utilities.globalQueue.postRunnable(() -> {
            File[] files = cacheDirectory().listFiles();
            if (files == null) {
                return;
            }
            long border = System.currentTimeMillis() - CACHE_LIFETIME;
            for (File file : files) {
                if (file.isFile() && file.lastModified() < border && !file.delete()) {
                    FinegramLogger.w("FGQuote", () -> "старая цитата не удалилась: " + file.getName());
                }
            }
        });
    }

    private static File write(Bitmap bitmap, boolean sticker) {
        File directory = cacheDirectory();
        if (!directory.isDirectory() && !directory.mkdirs()) {
            return null;
        }
        File file = new File(directory, "quote_" + System.currentTimeMillis() + (sticker ? ".webp" : ".png"));
        try (FileOutputStream stream = new FileOutputStream(file)) {
            boolean written = sticker
                    ? bitmap.compress(webpFormat(), 100, stream)
                    : bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
            if (written) {
                return file;
            }
        } catch (Throwable e) {
            FinegramLogger.e("FGQuote", () -> "цитата не сохранилась", e);
        }
        if (file.exists() && !file.delete()) {
            FinegramLogger.w("FGQuote", () -> "неудачная цитата осталась на диске");
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private static Bitmap.CompressFormat webpFormat() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ? Bitmap.CompressFormat.WEBP_LOSSLESS : Bitmap.CompressFormat.WEBP;
    }

    private static boolean motion() {
        return SharedConfig.animationsEnabled() && !LiteMode.isPowerSaverApplied()
                && !AndroidUtilities.isAccessibilityTouchExplorationEnabled()
                && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled());
    }

    private static void tick(View view) {
        try {
            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK);
        } catch (Exception ignore) {
        }
    }

    private class QuoteCanvas extends View {

        private final RectF frame = new RectF();
        private final Path clip = new Path();
        private final Paint shadow = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final int inset = dp(18);
        private FGQuoteMaker.Scene scene;
        private FGQuoteMaker.Scene outgoing;
        private float outgoingAlpha = 1f;
        private float progress = 1f;
        private float fromWidth;
        private float fromHeight;
        private float fromRadius;
        private ValueAnimator transition;

        QuoteCanvas(Context context) {
            super(context);
            shadow.setColor(surfaceColor);
        }

        void setScene(FGQuoteMaker.Scene next, boolean animated) {
            if (transition != null) {
                ValueAnimator running = transition;
                transition = null;
                running.cancel();
            }
            if (scene == null || !animated || !motion() || getWidth() == 0) {
                scene = next;
                outgoing = null;
                progress = 1f;
                requestLayout();
                invalidate();
                return;
            }
            float t = outgoing == null ? 1f : progress;
            fromWidth = frameWidth(t);
            fromHeight = frameHeight(t);
            fromRadius = frameRadius(t);
            if (outgoing == null || t >= 0.5f) {
                outgoingAlpha = outgoing == null ? 1f : t;
                outgoing = scene;
            } else {
                outgoingAlpha = outgoingAlpha * (1f - t);
            }
            scene = next;
            progress = 0f;
            requestLayout();

            ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setDuration(380);
            animator.setInterpolator(SETTLE);
            animator.addUpdateListener(a -> {
                progress = (float) a.getAnimatedValue();
                invalidate();
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (transition != animation) {
                        return;
                    }
                    transition = null;
                    outgoing = null;
                    progress = 1f;
                    requestLayout();
                    invalidate();
                }
            });
            transition = animator;
            animator.start();
        }

        private float fit(FGQuoteMaker.Scene target, int width) {
            if (target.getWidth() <= 0) {
                return 1f;
            }
            return Math.max(0.1f, Math.min(1f, (width - inset * 2f) / target.getWidth()));
        }

        private float frameWidth(float t) {
            float target = scene.getWidth() * fit(scene, getWidth());
            return outgoing == null ? target : AndroidUtilities.lerp(fromWidth, target, t);
        }

        private float frameHeight(float t) {
            float target = scene.getHeight() * fit(scene, getWidth());
            return outgoing == null ? target : AndroidUtilities.lerp(fromHeight, target, t);
        }

        private float frameRadius(float t) {
            float target = scene.getRadius() * fit(scene, getWidth());
            return outgoing == null ? target : AndroidUtilities.lerp(fromRadius, target, t);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            float content = 0f;
            if (scene != null) {
                content = scene.getHeight() * fit(scene, width);
            }
            if (outgoing != null) {
                content = Math.max(content, outgoing.getHeight() * fit(outgoing, width));
            }
            setMeasuredDimension(width, Math.max(stageHeight, (int) Math.ceil(content + inset * 2f)));
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            if (scene == null) {
                return;
            }
            float t = outgoing == null ? 1f : progress;
            float w = frameWidth(t);
            float h = frameHeight(t);
            float r = frameRadius(t);
            float cy = h + inset * 2f <= stageHeight ? stageHeight / 2f : inset + h / 2f;
            float left = (getWidth() - w) / 2f;
            frame.set(left, cy - h / 2f, left + w, cy + h / 2f);
            clip.rewind();
            clip.addRoundRect(frame, r, r, Path.Direction.CW);

            drawShadow(canvas, t, r);
            if (outgoing != null) {
                drawScene(canvas, outgoing, outgoingAlpha * (1f - t), 1f);
            }
            drawScene(canvas, scene, outgoing == null ? 1f : t, outgoing == null ? 1f : AndroidUtilities.lerp(0.97f, 1f, t));
        }

        private void drawShadow(Canvas canvas, float t, float r) {
            if (Build.VERSION.SDK_INT < 28) {
                return;
            }
            float amount = (scene.getOpaque() ? (outgoing == null ? 1f : t) : 0f)
                    + (outgoing != null && outgoing.getOpaque() ? outgoingAlpha * (1f - t) : 0f);
            if (amount <= 0.01f) {
                return;
            }
            shadow.setShadowLayer(dp(16), 0, dp(6), ColorUtils.setAlphaComponent(Color.BLACK, (int) (52 * Math.min(1f, amount))));
            canvas.drawRoundRect(frame, r, r, shadow);
        }

        private void drawScene(Canvas canvas, FGQuoteMaker.Scene target, float alpha, float scale) {
            if (alpha <= 0.004f) {
                return;
            }
            float f = fit(target, getWidth());
            float cx = frame.centerX();
            float cy = frame.centerY();
            int save = alpha < 0.996f
                    ? canvas.saveLayerAlpha(frame.left, frame.top, frame.right, frame.bottom, (int) (255 * alpha))
                    : canvas.save();
            canvas.clipPath(clip);
            if (scale != 1f) {
                canvas.scale(scale, scale, cx, cy);
            }
            canvas.translate(cx - target.getWidth() * f / 2f, cy - target.getHeight() * f / 2f);
            canvas.scale(f, f);
            target.draw(canvas);
            canvas.restoreToCount(save);
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            if (transition != null) {
                ValueAnimator running = transition;
                transition = null;
                running.cancel();
                outgoing = null;
                progress = 1f;
            }
        }
    }

    private class StageView extends FrameLayout {

        private final Paint dotPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint edgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final TextPaint badgePaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private final int badgeAlpha;
        private float[] dots;
        private String badge;
        private String previousBadge;
        private float swap = 1f;
        private ValueAnimator swapAnimator;

        StageView(Context context) {
            super(context);
            setWillNotDraw(false);
            dotPaint.setColor(Theme.multAlpha(textColor, 0.1f));
            dotPaint.setStrokeCap(Paint.Cap.ROUND);
            dotPaint.setStrokeWidth(AndroidUtilities.dpf2(2.2f));
            pillPaint.setColor(ColorUtils.setAlphaComponent(getThemedColor(Theme.key_dialogBackground), 230));
            edgePaint.setStyle(Paint.Style.STROKE);
            edgePaint.setStrokeWidth(Math.max(1f, AndroidUtilities.dpf2(0.7f)));
            edgePaint.setColor(Theme.multAlpha(textColor, 0.08f));
            badgePaint.setTextSize(dp(11));
            badgePaint.setTypeface(AndroidUtilities.bold());
            badgePaint.setColor(Theme.multAlpha(textColor, 0.66f));
            badgeAlpha = badgePaint.getAlpha();
            setOutlineProvider(new ViewOutlineProvider() {
                @Override
                public void getOutline(View view, Outline outline) {
                    outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), dp(26));
                }
            });
            setClipToOutline(true);
        }

        void setBadge(String text, boolean animated) {
            if (TextUtils.equals(text, badge)) {
                return;
            }
            if (swapAnimator != null) {
                ValueAnimator running = swapAnimator;
                swapAnimator = null;
                running.cancel();
            }
            if (!animated || !motion() || badge == null) {
                badge = text;
                previousBadge = null;
                swap = 1f;
                invalidate();
                return;
            }
            previousBadge = swap >= 0.5f ? badge : previousBadge;
            badge = text;
            swap = 0f;
            ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setDuration(240);
            animator.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            animator.addUpdateListener(a -> {
                swap = (float) a.getAnimatedValue();
                invalidate();
            });
            swapAnimator = animator;
            animator.start();
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            float step = dp(16);
            int columns = Math.max(1, (int) (w / step));
            int rows = Math.max(1, (int) (h / step));
            dots = new float[columns * rows * 2];
            float offsetX = (w - (columns - 1) * step) / 2f;
            float offsetY = (h - (rows - 1) * step) / 2f;
            int i = 0;
            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    dots[i++] = offsetX + column * step;
                    dots[i++] = offsetY + row * step;
                }
            }
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            canvas.drawColor(surfaceColor);
            if (dots != null) {
                canvas.drawPoints(dots, dotPaint);
            }
        }

        @Override
        protected void dispatchDraw(@NonNull Canvas canvas) {
            super.dispatchDraw(canvas);
            if (badge == null) {
                return;
            }
            float to = badgePaint.measureText(badge);
            float from = previousBadge != null && swap < 1f ? badgePaint.measureText(previousBadge) : to;
            float textWidth = AndroidUtilities.lerp(from, to, swap);
            float right = getWidth() - dp(10);
            float bottom = getHeight() - dp(10);
            rect.set(right - textWidth - dp(16), bottom - dp(22), right, bottom);
            canvas.drawRoundRect(rect, dp(11), dp(11), pillPaint);
            canvas.drawRoundRect(rect, dp(11), dp(11), edgePaint);
            canvas.save();
            canvas.clipRect(rect);
            float baseline = bottom - dp(7.5f);
            float shift = dp(5);
            badgePaint.setAlpha((int) (badgeAlpha * swap));
            canvas.drawText(badge, right - dp(8) - to, baseline + shift * (1f - swap), badgePaint);
            if (previousBadge != null && swap < 1f) {
                badgePaint.setAlpha((int) (badgeAlpha * (1f - swap)));
                canvas.drawText(previousBadge, right - dp(8) - from, baseline - shift * swap, badgePaint);
            }
            badgePaint.setAlpha(badgeAlpha);
            canvas.restore();
        }
    }

    private class Segmented extends View {

        private final String[] labels;
        private final String[] shown;
        private final boolean icons;
        private final TextPaint labelPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final Paint trackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint iconPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private final Path moon = new Path();
        private final int idleColor;
        private Utilities.Callback<Integer> listener;
        private int selected;
        private float pillLeft;
        private float pillRight;
        private ValueAnimator move;

        Segmented(Context context, String[] labels, boolean icons) {
            super(context);
            this.labels = labels;
            this.icons = icons;
            shown = labels.clone();
            idleColor = Theme.multAlpha(textColor, 0.72f);
            labelPaint.setTextSize(dp(14));
            labelPaint.setTypeface(AndroidUtilities.bold());
            trackPaint.setColor(Theme.multAlpha(textColor, 0.06f));
            pillPaint.setColor(accent);
            iconPaint.setStrokeCap(Paint.Cap.ROUND);
            iconPaint.setStrokeWidth(AndroidUtilities.dpf2(1.7f));

            float s = AndroidUtilities.dpf2(1f);
            Path disc = new Path();
            disc.addCircle(0f, 0f, 7.5f * s, Path.Direction.CW);
            Path cut = new Path();
            cut.addCircle(4.4f * s, -3.6f * s, 6.2f * s, Path.Direction.CW);
            moon.op(disc, cut, Path.Op.DIFFERENCE);
            setContentDescription(labels[0]);
        }

        void setListener(Utilities.Callback<Integer> listener) {
            this.listener = listener;
        }

        private float inset() {
            return dp(3);
        }

        private float segment() {
            return (getWidth() - inset() * 2f) / labels.length;
        }

        void select(int index, boolean animated) {
            int target = Math.max(0, Math.min(labels.length - 1, index));
            if (move != null) {
                ValueAnimator running = move;
                move = null;
                running.cancel();
            }
            selected = target;
            setContentDescription(labels[target]);
            if (!animated || !motion() || getWidth() == 0) {
                invalidate();
                return;
            }
            final float fromLeft = pillLeft;
            final float fromRight = pillRight;
            final float toLeft = inset() + target * segment();
            final float toRight = toLeft + segment();
            final boolean forward = toLeft > fromLeft;
            ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
            animator.setDuration(420);
            animator.setInterpolator(t -> t);
            animator.addUpdateListener(a -> {
                float t = (float) a.getAnimatedValue();
                float quick = SETTLE.getInterpolation(Math.min(1f, t * 1.35f));
                float slow = SETTLE.getInterpolation(t);
                pillLeft = AndroidUtilities.lerp(fromLeft, toLeft, forward ? slow : quick);
                pillRight = AndroidUtilities.lerp(fromRight, toRight, forward ? quick : slow);
                invalidate();
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (move == animation) {
                        move = null;
                        invalidate();
                    }
                }
            });
            move = animator;
            animator.start();
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            if (!icons) {
                for (int i = 0; i < labels.length; i++) {
                    shown[i] = TextUtils.ellipsize(labels[i], labelPaint, Math.max(0f, segment() - dp(12)), TextUtils.TruncateAt.END).toString();
                }
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!isEnabled()) {
                return false;
            }
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_UP) {
                int index = Math.max(0, Math.min(labels.length - 1, (int) ((event.getX() - inset()) / segment())));
                if (index != selected) {
                    select(index, true);
                    tick(this);
                    if (listener != null) {
                        listener.run(index);
                    }
                }
                performClick();
                return true;
            }
            return action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE;
        }

        @Override
        public boolean performClick() {
            return super.performClick();
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            float h = getHeight();
            float inset = inset();
            float segment = segment();
            rect.set(0, 0, getWidth(), h);
            canvas.drawRoundRect(rect, h / 2f, h / 2f, trackPaint);
            if (move == null) {
                pillLeft = inset + selected * segment;
                pillRight = pillLeft + segment;
            }
            float radius = (h - inset * 2f) / 2f;
            rect.set(pillLeft, inset, pillRight, h - inset);
            canvas.drawRoundRect(rect, radius, radius, pillPaint);

            for (int i = 0; i < labels.length; i++) {
                float left = inset + i * segment;
                float right = left + segment;
                float overlap = Math.max(0f, Math.min(pillRight, right) - Math.max(pillLeft, left)) / segment;
                int color = ColorUtils.blendARGB(idleColor, accentText, Math.min(1f, overlap));
                float cx = (left + right) / 2f;
                float cy = h / 2f;
                if (icons) {
                    drawIcon(canvas, i, cx, cy, color);
                } else {
                    labelPaint.setColor(color);
                    String text = shown[i];
                    canvas.drawText(text, cx - labelPaint.measureText(text) / 2f, cy - (labelPaint.descent() + labelPaint.ascent()) / 2f, labelPaint);
                }
            }
        }

        private void drawIcon(Canvas canvas, int index, float cx, float cy, int color) {
            float s = AndroidUtilities.dpf2(1f);
            iconPaint.setColor(color);
            if (index == THEME_AUTO) {
                float r = 7.5f * s;
                iconPaint.setStyle(Paint.Style.STROKE);
                canvas.drawCircle(cx, cy, r, iconPaint);
                iconPaint.setStyle(Paint.Style.FILL);
                rect.set(cx - r, cy - r, cx + r, cy + r);
                canvas.drawArc(rect, 90f, 180f, true, iconPaint);
            } else if (index == THEME_LIGHT) {
                iconPaint.setStyle(Paint.Style.FILL);
                canvas.drawCircle(cx, cy, 3.8f * s, iconPaint);
                iconPaint.setStyle(Paint.Style.STROKE);
                for (int i = 0; i < 8; i++) {
                    double angle = Math.PI / 4.0 * i;
                    float cos = (float) Math.cos(angle);
                    float sin = (float) Math.sin(angle);
                    canvas.drawLine(cx + cos * 6.2f * s, cy + sin * 6.2f * s, cx + cos * 8.4f * s, cy + sin * 8.4f * s, iconPaint);
                }
            } else {
                iconPaint.setStyle(Paint.Style.FILL);
                canvas.save();
                canvas.translate(cx - 0.8f * s, cy + 0.6f * s);
                canvas.drawPath(moon, iconPaint);
                canvas.restore();
            }
        }
    }

    private class SwatchStrip extends View {

        private final int count = FGQuoteMaker.BACKGROUND_COUNT;
        private final float[] focus = new float[count];
        private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final int size = dp(40);
        private final int gap = dp(12);
        private final int edge = dp(16);
        private Utilities.Callback<Integer> listener;
        private ValueAnimator animator;
        private int selected;
        private boolean dark;
        private boolean placed;

        SwatchStrip(Context context) {
            super(context);
            ring.setStyle(Paint.Style.STROKE);
            ring.setStrokeWidth(AndroidUtilities.dpf2(2f));
            ring.setColor(accent);
        }

        void setListener(Utilities.Callback<Integer> listener) {
            this.listener = listener;
        }

        void setDark(boolean value) {
            if (dark != value) {
                dark = value;
                invalidate();
            }
        }

        private String nameOf(int index) {
            if (index == FGQuoteMaker.BACKGROUND_WALLPAPER) {
                return getString(R.string.FG_Quote_BgWallpaper);
            } else if (index == FGQuoteMaker.BACKGROUND_PLAIN) {
                return getString(R.string.FG_Quote_BgPlain);
            } else if (index == FGQuoteMaker.BACKGROUND_NONE) {
                return getString(R.string.FG_Quote_BgNone);
            }
            return getString(R.string.FG_Quote_BgGradient);
        }

        private float centerOf(int index) {
            return edge + index * (size + gap) + size / 2f;
        }

        void select(int index, boolean animated) {
            int target = index >= 0 && index < count ? index : FGQuoteMaker.BACKGROUND_WALLPAPER;
            selected = target;
            setContentDescription(nameOf(target));
            if (animator != null) {
                ValueAnimator running = animator;
                animator = null;
                running.cancel();
            }
            if (!animated || !motion()) {
                for (int i = 0; i < count; i++) {
                    focus[i] = i == target ? 1f : 0f;
                }
                invalidate();
                return;
            }
            final float[] from = focus.clone();
            ValueAnimator next = ValueAnimator.ofFloat(0f, 1f);
            next.setDuration(260);
            next.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            next.addUpdateListener(a -> {
                float t = (float) a.getAnimatedValue();
                for (int i = 0; i < count; i++) {
                    focus[i] = AndroidUtilities.lerp(from[i], i == target ? 1f : 0f, t);
                }
                invalidate();
            });
            animator = next;
            next.start();
            reveal(true);
        }

        private void reveal(boolean smooth) {
            ViewParent parent = getParent();
            if (!(parent instanceof HorizontalScrollView) || ((HorizontalScrollView) parent).getWidth() == 0) {
                return;
            }
            HorizontalScrollView scroll = (HorizontalScrollView) parent;
            float cx = centerOf(selected);
            int left = (int) (cx - size / 2f - edge);
            int right = (int) (cx + size / 2f + edge);
            int target;
            if (left < scroll.getScrollX()) {
                target = left;
            } else if (right > scroll.getScrollX() + scroll.getWidth()) {
                target = right - scroll.getWidth();
            } else {
                return;
            }
            if (smooth) {
                scroll.smoothScrollTo(target, 0);
            } else {
                scroll.scrollTo(target, 0);
            }
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            if (!placed) {
                placed = true;
                reveal(false);
            }
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMeasuredDimension(edge * 2 + count * size + (count - 1) * gap, size + dp(10));
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_UP) {
                float x = event.getX() - edge;
                int index = (int) (x / (size + gap));
                float within = x - index * (size + gap);
                if (x >= 0 && index < count && within <= size && index != selected) {
                    select(index, true);
                    tick(this);
                    if (listener != null) {
                        listener.run(index);
                    }
                }
                performClick();
                return true;
            }
            return action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE;
        }

        @Override
        public boolean performClick() {
            return super.performClick();
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            float cy = getHeight() / 2f;
            float outer = size / 2f - ring.getStrokeWidth() / 2f;
            for (int i = 0; i < count; i++) {
                float cx = centerOf(i);
                float f = focus[i];
                FGQuoteMaker.drawSwatch(canvas, i, cx, cy, size / 2f - AndroidUtilities.dpf2(5f) * f, dark, wallpaper);
                if (f > 0.01f) {
                    ring.setAlpha((int) (255 * Math.min(1f, f * 1.4f)));
                    canvas.drawCircle(cx, cy, outer * AndroidUtilities.lerp(0.86f, 1f, f), ring);
                }
            }
        }
    }

    private class ToggleChip extends View {

        private final String label;
        private final Utilities.Callback<Boolean> listener;
        private final TextPaint textPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint mark = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path markPath = new Path();
        private final Path markPart = new Path();
        private final PathMeasure measure = new PathMeasure();
        private final RectF rect = new RectF();
        private boolean checked;
        private float progress;
        private ValueAnimator animator;

        ToggleChip(Context context, String label, boolean checked, Utilities.Callback<Boolean> listener) {
            super(context);
            this.label = label;
            this.listener = listener;
            this.checked = checked;
            progress = checked ? 1f : 0f;
            textPaint.setTextSize(dp(14));
            textPaint.setTypeface(AndroidUtilities.bold());
            textPaint.setColor(textColor);
            stroke.setStyle(Paint.Style.STROKE);
            stroke.setStrokeWidth(AndroidUtilities.dpf2(1.2f));
            mark.setStyle(Paint.Style.STROKE);
            mark.setStrokeWidth(AndroidUtilities.dpf2(1.8f));
            mark.setStrokeCap(Paint.Cap.ROUND);
            mark.setStrokeJoin(Paint.Join.ROUND);
            mark.setColor(accentText);
            float s = AndroidUtilities.dpf2(1f);
            markPath.moveTo(-3.6f * s, 0.2f * s);
            markPath.lineTo(-1.1f * s, 2.7f * s);
            markPath.lineTo(3.8f * s, -2.6f * s);
            measure.setPath(markPath, false);
            setContentDescription(label);
            setOnClickListener(v -> toggle());
            ScaleStateListAnimator.apply(this, 0.04f, 1.4f);
        }

        private void toggle() {
            checked = !checked;
            tick(this);
            listener.run(checked);
            if (animator != null) {
                ValueAnimator running = animator;
                animator = null;
                running.cancel();
            }
            float target = checked ? 1f : 0f;
            if (!motion()) {
                progress = target;
                invalidate();
                return;
            }
            ValueAnimator next = ValueAnimator.ofFloat(progress, target);
            next.setDuration(220);
            next.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            next.addUpdateListener(a -> {
                progress = (float) a.getAnimatedValue();
                invalidate();
            });
            animator = next;
            next.start();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = dp(12) + dp(18) + dp(8) + (int) Math.ceil(textPaint.measureText(label)) + dp(14);
            setMeasuredDimension(width, dp(36));
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            float half = stroke.getStrokeWidth() / 2f;
            rect.set(half, half, getWidth() - half, getHeight() - half);
            float radius = rect.height() / 2f;
            if (progress > 0f) {
                fill.setColor(Theme.multAlpha(accent, 0.15f * progress));
                canvas.drawRoundRect(rect, radius, radius, fill);
            }
            if (progress < 1f) {
                stroke.setColor(Theme.multAlpha(textColor, 0.16f * (1f - progress)));
                canvas.drawRoundRect(rect, radius, radius, stroke);
            }

            float cx = dp(12) + dp(9);
            float cy = getHeight() / 2f;
            float dot = AndroidUtilities.dpf2(8.5f);
            if (progress < 1f) {
                stroke.setColor(Theme.multAlpha(textColor, 0.35f * (1f - progress)));
                canvas.drawCircle(cx, cy, dot - half, stroke);
            }
            if (progress > 0f) {
                fill.setColor(accent);
                fill.setAlpha((int) (255 * Math.min(1f, progress * 1.5f)));
                canvas.drawCircle(cx, cy, dot * AndroidUtilities.lerp(0.55f, 1f, progress), fill);
                fill.setAlpha(255);
                markPart.rewind();
                measure.getSegment(0f, measure.getLength() * progress, markPart, true);
                canvas.save();
                canvas.translate(cx, cy);
                canvas.drawPath(markPart, mark);
                canvas.restore();
            }
            canvas.drawText(label, dp(12) + dp(18) + dp(8), cy - (textPaint.descent() + textPaint.ascent()) / 2f, textPaint);
        }
    }

    private class ActionTile extends View {

        private final Drawable icon;
        private final String label;
        private final TextPaint textPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final Paint surface = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint spinner = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private String shown;
        private boolean loading;
        private float loadingProgress;
        private float angle;
        private ValueAnimator spin;
        private ValueAnimator fade;

        ActionTile(Context context, int iconRes, String label, Runnable action) {
            super(context);
            this.label = label;
            shown = label;
            icon = context.getResources().getDrawable(iconRes).mutate();
            icon.setColorFilter(new PorterDuffColorFilter(textColor, PorterDuff.Mode.SRC_IN));
            textPaint.setTextSize(dp(13));
            textPaint.setTypeface(AndroidUtilities.bold());
            textPaint.setColor(textColor);
            surface.setColor(Theme.multAlpha(textColor, 0.06f));
            spinner.setStyle(Paint.Style.STROKE);
            spinner.setStrokeWidth(AndroidUtilities.dpf2(2f));
            spinner.setStrokeCap(Paint.Cap.ROUND);
            spinner.setColor(accent);
            setContentDescription(label);
            setOnClickListener(v -> action.run());
            ScaleStateListAnimator.apply(this, 0.05f, 1.4f);
        }

        @Override
        public void setEnabled(boolean enabled) {
            super.setEnabled(enabled);
            setAlpha(enabled || loading ? 1f : 0.5f);
        }

        void setLoading(boolean value) {
            if (loading == value) {
                return;
            }
            loading = value;
            if (value) {
                startSpin();
            }
            if (fade != null) {
                ValueAnimator running = fade;
                fade = null;
                running.cancel();
            }
            float target = value ? 1f : 0f;
            if (!motion()) {
                loadingProgress = target;
                if (!value) {
                    stopSpin();
                }
                invalidate();
                return;
            }
            ValueAnimator next = ValueAnimator.ofFloat(loadingProgress, target);
            next.setDuration(180);
            next.setInterpolator(CubicBezierInterpolator.EASE_OUT_QUINT);
            next.addUpdateListener(a -> {
                loadingProgress = (float) a.getAnimatedValue();
                invalidate();
            });
            next.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (fade == animation && !loading) {
                        stopSpin();
                    }
                }
            });
            fade = next;
            next.start();
        }

        private void startSpin() {
            if (spin != null) {
                return;
            }
            spin = ValueAnimator.ofFloat(0f, 360f);
            spin.setDuration(720);
            spin.setRepeatCount(ValueAnimator.INFINITE);
            spin.setInterpolator(t -> t);
            spin.addUpdateListener(a -> {
                angle = (float) a.getAnimatedValue();
                invalidate();
            });
            spin.start();
        }

        private void stopSpin() {
            if (spin != null) {
                spin.cancel();
                spin = null;
            }
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            stopSpin();
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            shown = TextUtils.ellipsize(label, textPaint, Math.max(0, w - dp(12)), TextUtils.TruncateAt.END).toString();
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            rect.set(0, 0, getWidth(), getHeight());
            canvas.drawRoundRect(rect, dp(20), dp(20), surface);
            float cx = getWidth() / 2f;
            float cy = dp(24);
            if (loadingProgress < 1f) {
                int half = dp(12);
                float scale = 1f - 0.3f * loadingProgress;
                canvas.save();
                canvas.scale(scale, scale, cx, cy);
                icon.setAlpha((int) (255 * (1f - loadingProgress)));
                icon.setBounds((int) cx - half, (int) cy - half, (int) cx + half, (int) cy + half);
                icon.draw(canvas);
                canvas.restore();
            }
            if (loadingProgress > 0f) {
                float r = AndroidUtilities.dpf2(8f) * AndroidUtilities.lerp(0.7f, 1f, loadingProgress);
                spinner.setAlpha((int) (255 * loadingProgress));
                rect.set(cx - r, cy - r, cx + r, cy + r);
                canvas.drawArc(rect, angle - 90f, 110f, false, spinner);
            }
            canvas.drawText(shown, cx - textPaint.measureText(shown) / 2f, dp(52), textPaint);
        }
    }
}
