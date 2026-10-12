package com.th3nekit.finegram.chats.ui;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Build;
import android.os.PowerManager;
import android.view.View;
import android.view.ViewGroup;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.LiteMode;
import org.telegram.messenger.SharedConfig;
import org.telegram.ui.ActionBar.SimpleTextView;
import org.telegram.ui.Cells.DialogCell;
import org.telegram.ui.Components.BackupImageView;
import org.telegram.ui.Components.ChatAvatarContainer;

import java.util.WeakHashMap;

public final class DialogIdentityTransition extends View {
    private static final WeakHashMap<ViewGroup, DialogIdentityTransition> active = new WeakHashMap<>();
    private final ViewGroup host;
    private final DialogCell cell;
    private final BackupImageView avatar;
    private final SimpleTextView title;
    private final long dialogId;
    private final RectF sourceAvatar = new RectF();
    private final RectF targetAvatar = new RectF();
    private final RectF currentAvatar = new RectF();
    private final RectF sourceName = new RectF();
    private final RectF targetName = new RectF();
    private final RectF currentName = new RectF();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final int[] location = new int[2];
    private final int[] hostLocation = new int[2];
    private final boolean opening;
    private final float avatarAlpha;
    private final float titleAlpha;
    private final float sourceTextSize;
    private final float targetTextSize;
    private final float sourceTextX;
    private final float sourceTextY;
    private final float targetTextX;
    private final float targetTextY;
    private final Bitmap sourceImage;
    private final Bitmap targetImage;
    private final Bitmap sourceTitle;
    private final Bitmap targetTitle;
    private final Runnable timeout = this::finish;
    private final OnAttachStateChangeListener attachment = new OnAttachStateChangeListener() {
        public void onViewAttachedToWindow(View view) {}
        public void onViewDetachedFromWindow(View view) { finish(); }
    };
    private float progress;
    private boolean finished;

    public static boolean animationsEnabled() {
        if (!FinegramAppearanceConfig.INSTANCE.getDialogIdentityAnimation()
                || !SharedConfig.animationsEnabled() || LiteMode.isPowerSaverApplied()
                || Build.VERSION.SDK_INT >= 26 && !ValueAnimator.areAnimatorsEnabled()) return false;
        PowerManager power = (PowerManager) ApplicationLoader.applicationContext.getSystemService(Context.POWER_SERVICE);
        return power == null || !power.isPowerSaveMode();
    }

    public static DialogIdentityTransition start(ViewGroup host, DialogCell cell,
                                                 ChatAvatarContainer header, boolean opening) {
        if (host == null) return null;
        DialogIdentityTransition previous = active.get(host);
        if (previous != null) previous.finish();
        if (!animationsEnabled() || cell == null || header == null || !host.isAttachedToWindow()
                || !cell.isAttachedToWindow() || cell.getDialogId() == 0
                || host.getWidth() == 0 || cell.avatarImage.getImageWidth() <= 0
                || cell.avatarImage.getImageHeight() <= 0) return null;
        BackupImageView avatar = header.getAvatarImageView();
        SimpleTextView title = header.getTitleTextView();
        if (avatar == null || avatar.getVisibility() != VISIBLE || avatar.getWidth() == 0
                || avatar.getHeight() == 0 || avatar.getAlpha() <= 0 || title == null
                || title.getWidth() == 0 || title.getHeight() == 0 || title.getAlpha() <= 0
                || cell.nameLayout == null) return null;
        DialogIdentityTransition transition = null;
        try {
            transition = new DialogIdentityTransition(host, cell, avatar, title, opening);
            transition.layout(0, 0, host.getWidth(), host.getHeight());
            host.getOverlay().add(transition);
            active.put(host, transition);
            host.addOnAttachStateChangeListener(transition.attachment);
            cell.addOnAttachStateChangeListener(transition.attachment);
            avatar.addOnAttachStateChangeListener(transition.attachment);
            title.addOnAttachStateChangeListener(transition.attachment);
            cell.setIdentityTransitionHidden(true);
            avatar.setAlpha(0);
            title.setAlpha(0);
            AndroidUtilities.runOnUIThread(transition.timeout, 1500);
            return transition;
        } catch (RuntimeException ignored) {
            if (transition != null) transition.finish();
            return null;
        }
    }

    private DialogIdentityTransition(ViewGroup host, DialogCell cell, BackupImageView avatar,
                                     SimpleTextView title, boolean opening) {
        super(host.getContext());
        this.host = host;
        this.cell = cell;
        this.avatar = avatar;
        this.title = title;
        this.opening = opening;
        dialogId = cell.getDialogId();
        avatarAlpha = avatar.getAlpha();
        titleAlpha = title.getAlpha();
        sourceTextSize = cell.nameLayout.getPaint().getTextSize();
        targetTextSize = title.getTextPaint().getTextSize();
        host.getLocationOnScreen(hostLocation);
        finalLocation(cell);
        sourceAvatar.set(location[0] + cell.avatarImage.getImageX(), location[1] + cell.avatarImage.getImageY(),
                location[0] + cell.avatarImage.getImageX2(), location[1] + cell.avatarImage.getImageY2());
        cell.getIdentityNameBounds(sourceName);
        if (sourceName.isEmpty() || sourceTextSize <= 0 || targetTextSize <= 0) {
            throw new IllegalArgumentException("Empty title layout");
        }
        sourceTextX = cell.getIdentityNameStartX() - sourceName.left;
        sourceTextY = cell.getIdentityNameTop() - sourceName.top;
        sourceTitle = Bitmap.createBitmap(Math.max(1, (int) Math.ceil(sourceName.width())),
                Math.max(1, (int) Math.ceil(sourceName.height())), Bitmap.Config.ARGB_8888);
        Canvas nameCanvas = new Canvas(sourceTitle);
        nameCanvas.translate(-sourceName.left, -sourceName.top);
        cell.drawIdentityTitle(nameCanvas);
        sourceName.offset(location[0], location[1]);
        finalLocation(title);
        targetName.set(location[0], location[1], location[0] + title.getWidth(), location[1] + title.getHeight());
        targetTextX = title.getTextStartX() - title.getX();
        targetTextY = title.getPaddingTop() + (title.getHeight() - title.getPaddingTop()
                - title.getPaddingBottom() - title.getTextHeight()) / 2f;
        targetTitle = Bitmap.createBitmap(title.getWidth(), title.getHeight(), Bitmap.Config.ARGB_8888);
        title.draw(new Canvas(targetTitle));
        finalLocation(avatar);
        targetAvatar.set(location[0], location[1], location[0] + avatar.getWidth(), location[1] + avatar.getHeight());
        int sourceSize = Math.max(1, Math.min(256, Math.round(sourceAvatar.width())));
        sourceImage = Bitmap.createBitmap(sourceSize, sourceSize, Bitmap.Config.ARGB_8888);
        Canvas sourceCanvas = new Canvas(sourceImage);
        sourceCanvas.scale(sourceSize / sourceAvatar.width(), sourceSize / sourceAvatar.height());
        sourceCanvas.translate(-cell.avatarImage.getImageX(), -cell.avatarImage.getImageY());
        cell.avatarImage.draw(sourceCanvas);
        int targetSize = Math.max(1, Math.min(256, avatar.getWidth()));
        targetImage = Bitmap.createBitmap(targetSize, targetSize, Bitmap.Config.ARGB_8888);
        Canvas targetCanvas = new Canvas(targetImage);
        targetCanvas.scale((float) targetSize / avatar.getWidth(), (float) targetSize / avatar.getHeight());
        avatar.draw(targetCanvas);
        progress = opening ? 0 : 1;
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setWillNotDraw(false);
    }

    private void finalLocation(View view) {
        view.getLocationOnScreen(location);
        location[0] -= hostLocation[0];
        location[1] -= hostLocation[1];
        for (View node = view; node != null && node != host;
                node = node.getParent() instanceof View ? (View) node.getParent() : null) {
            if (node == avatar || node == title || node instanceof ChatAvatarContainer) continue;
            location[0] -= Math.round(node.getTranslationX());
            location[1] -= Math.round(node.getTranslationY());
        }
    }

    public void setProgress(boolean isOpen, float value) {
        if (finished || isOpen != opening) return;
        if (cell.getDialogId() != dialogId || !cell.isAttachedToWindow()
                || !host.isAttachedToWindow() || !avatar.isAttachedToWindow() || !title.isAttachedToWindow()
                || !FinegramAppearanceConfig.INSTANCE.getDialogIdentityAnimation()
                || !SharedConfig.animationsEnabled() || LiteMode.isPowerSaverApplied()) {
            finish();
            return;
        }
        float clamped = Math.max(0, Math.min(1, value));
        if (clamped == 1) { finish(); return; }
        progress = opening ? clamped : 1 - clamped;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (finished) return;
        host.getLocationOnScreen(hostLocation);
        finalLocation(avatar);
        targetAvatar.set(location[0], location[1], location[0] + avatar.getWidth(), location[1] + avatar.getHeight());
        finalLocation(title);
        targetName.set(location[0], location[1], location[0] + title.getWidth(), location[1] + title.getHeight());
        float p = progress;
        currentAvatar.set(mix(sourceAvatar.left, targetAvatar.left, p), mix(sourceAvatar.top, targetAvatar.top, p),
                mix(sourceAvatar.right, targetAvatar.right, p), mix(sourceAvatar.bottom, targetAvatar.bottom, p));
        paint.setAlpha(Math.round(255 * (1 - p)));
        canvas.drawBitmap(sourceImage, null, currentAvatar, paint);
        paint.setAlpha(Math.round(255 * p * avatarAlpha));
        canvas.drawBitmap(targetImage, null, currentAvatar, paint);
        float x = mix(sourceName.left + sourceTextX, targetName.left + targetTextX, p);
        float y = mix(sourceName.top + sourceTextY, targetName.top + targetTextY, p);
        float textSize = mix(sourceTextSize, targetTextSize, p);
        drawTitle(canvas, sourceTitle, x, y, sourceTextX, sourceTextY,
                textSize / sourceTextSize, 1 - p);
        drawTitle(canvas, targetTitle, x, y, targetTextX, targetTextY,
                textSize / targetTextSize, p * titleAlpha);
    }

    private void drawTitle(Canvas canvas, Bitmap bitmap, float x, float y, float offsetX,
                           float offsetY, float scale, float alpha) {
        if (alpha <= 0) return;
        float left = x - offsetX * scale;
        float top = y - offsetY * scale;
        currentName.set(left, top, left + bitmap.getWidth() * scale, top + bitmap.getHeight() * scale);
        paint.setAlpha(Math.round(255 * alpha));
        canvas.drawBitmap(bitmap, null, currentName, paint);
    }

    public boolean isFinished() {
        return finished;
    }

    private static float mix(float from, float to, float progress) {
        return from + (to - from) * progress;
    }

    public void finish() {
        if (finished) return;
        finished = true;
        AndroidUtilities.cancelRunOnUIThread(timeout);
        host.removeOnAttachStateChangeListener(attachment);
        cell.removeOnAttachStateChangeListener(attachment);
        avatar.removeOnAttachStateChangeListener(attachment);
        title.removeOnAttachStateChangeListener(attachment);
        cell.clearIdentityTransition(dialogId);
        avatar.setAlpha(avatarAlpha);
        title.setAlpha(titleAlpha);
        host.getOverlay().remove(this);
        if (active.get(host) == this) active.remove(host);
    }
}
