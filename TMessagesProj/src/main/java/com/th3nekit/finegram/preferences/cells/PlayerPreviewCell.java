/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.preferences.cells;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.SystemClock;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.ColorUtils;

import com.th3nekit.finegram.core.configs.FinegramPlayerConfig;
import com.th3nekit.finegram.core.ui.MusicPlayerLayout;
import com.th3nekit.finegram.core.ui.MusicPlayerSurface;
import com.th3nekit.finegram.core.ui.ProfileMusicLayout;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.ProfilePillView;

public class PlayerPreviewCell extends LinearLayout implements Theme.Colorable {
    public static final int FULL = 0, MINI = 1, PROFILE = 2;
    private static final int ART = 0xFFB54270;
    private final Theme.ResourcesProvider resourcesProvider;
    private final TextView[] tabs = new TextView[3];
    private final Preview preview;
    private final LinearLayout modes;
    private int mode;
    private boolean playing;

    public PlayerPreviewCell(Context context, Theme.ResourcesProvider resourcesProvider) {
        super(context);
        this.resourcesProvider = resourcesProvider;
        setClickable(true);
        setOrientation(VERTICAL);
        setPadding(dp(12), dp(12), dp(12), dp(8));
        modes = new LinearLayout(context);
        modes.setGravity(Gravity.CENTER_VERTICAL);
        int[] labels = {R.string.FG_Player_Preview_Full, R.string.FG_Player_Preview_Mini, R.string.FG_Player_Preview_Profile};
        for (int i = 0; i < tabs.length; i++) {
            final int selected = i;
            TextView tab = tabs[i] = new TextView(context);
            tab.setText(getString(labels[i]));
            tab.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tab.setTypeface(AndroidUtilities.bold());
            tab.setGravity(Gravity.CENTER);
            tab.setMinHeight(dp(48));
            tab.setPadding(dp(4), dp(8), dp(4), dp(8));
            tab.setFocusable(true);
            tab.setOnClickListener(v -> showMode(selected));
            modes.addView(tab, LayoutHelper.createLinear(0, LayoutHelper.WRAP_CONTENT, 1f));
        }
        addView(modes, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        preview = new Preview(context);
        preview.setFocusable(true);
        preview.setOnClickListener(v -> {
            playing = !playing;
            refresh();
        });
        addView(preview, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        refresh();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        boolean stacked = MeasureSpec.getSize(widthMeasureSpec) < dp(360) && getResources().getConfiguration().fontScale > 1.5f;
        modes.setOrientation(stacked ? VERTICAL : HORIZONTAL);
        for (TextView tab : tabs) {
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) tab.getLayoutParams();
            params.width = stacked ? LayoutHelper.MATCH_PARENT : 0;
            params.weight = stacked ? 0f : 1f;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    public void showMode(int mode) {
        this.mode = Math.max(FULL, Math.min(PROFILE, mode));
        refresh();
    }

    public void refresh() {
        updateColors();
        preview.setContentDescription(getString(playing ? R.string.FG_Player_Preview_Pause : R.string.FG_Player_Preview_Play));
        preview.requestLayout();
        preview.invalidate();
    }

    @Override
    public void updateColors() {
        int text = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
        int accent = Theme.getColor(Theme.key_windowBackgroundWhiteBlueText, resourcesProvider);
        for (int i = 0; i < tabs.length; i++) {
            tabs[i].setSelected(i == mode);
            tabs[i].setTextColor(i == mode ? accent : text);
            tabs[i].setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(12),
                    i == mode ? Theme.multAlpha(accent, .12f) : Color.TRANSPARENT, Theme.multAlpha(text, .1f)));
        }
        if (preview != null) preview.invalidate();
    }

    private final class Preview extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final TextPaint text = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private final Path path = new Path();
        private final MusicPlayerSurface motion;
        private final ProfilePillView stockProfile;
        private final Shader artwork = new LinearGradient(0, 0, 1, 1,
                new int[]{0xFF5741A6, ART, 0xFFEDAA68}, null, Shader.TileMode.CLAMP);
        private final android.util.SparseArray<android.graphics.drawable.Drawable> icons = new android.util.SparseArray<>();
        private final android.graphics.Matrix artMatrix = new android.graphics.Matrix();
        private final float[] artworkHsv = new float[3];
        private int base, foreground, gray, accent;
        private float font, width;
        private MusicPlayerLayout full;
        private MusicPlayerLayout.Compact compact;
        private ProfileMusicLayout profile;
        private MusicPlayerLayout.ToolRow tools;
        private int queueExtra;
        private float spin;
        private long frame;

        Preview(Context context) {
            super(context);
            motion = new MusicPlayerSurface(this);
            stockProfile = new ProfilePillView(context, resourcesProvider);
            stockProfile.setText(getString(R.string.FG_Player_Preview_Artist), " - " + getString(R.string.FG_Player_Preview_Title));
        }

        private float font() {
            return Math.max(1f, getResources().getConfiguration().fontScale);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            float w = MeasureSpec.getSize(widthMeasureSpec) / AndroidUtilities.density;
            float f = font();
            text.setTextSize(14 * f);
            text.setTypeface(AndroidUtilities.bold());
            tools = MusicPlayerLayout.toolRow(Math.max(280, Math.round(w)), (int) Math.ceil(text.measureText(getString(R.string.Next))) + 54);
            queueExtra = tools.stacked ? Math.max(48, (int) Math.ceil(20 * f)) + 8 : 0;
            full = MusicPlayerLayout.createScrollable(Math.max(280, Math.round(w)), 580 - queueExtra, f, false);
            compact = MusicPlayerLayout.compact(f, false);
            profile = new ProfileMusicLayout(w, f, LocaleController.isRTL);
            stockProfile.measure(widthMeasureSpec, heightMeasureSpec);
            stockProfile.layout(0, 0, MeasureSpec.getSize(widthMeasureSpec), stockProfile.getMeasuredHeight());
            stockProfile.updatePosition(dp(12), dp(37));
            float height = mode == FULL ? (FinegramPlayerConfig.INSTANCE.getFullscreenPlayer() ? (full.height + queueExtra) * .6f : compact.height) + 32
                    : mode == MINI ? Math.max(64, (float) Math.ceil(40 * f) + 24) + 40
                    : (FinegramPlayerConfig.stockProfileCard() ? 37 : ProfileMusicLayout.height(f)) + 32;
            setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), dp(height));
        }

        @Override
        protected void onDraw(Canvas canvas) {
            width = getWidth() / AndroidUtilities.density;
            font = font();
            base = Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider);
            foreground = Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider);
            gray = Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider);
            accent = FinegramPlayerConfig.INSTANCE.getAccent() == FinegramPlayerConfig.ACCENT_ARTWORK ? ART
                    : Theme.getColor(Theme.key_player_buttonActive, resourcesProvider);
            long now = SystemClock.uptimeMillis();
            boolean moving = playing && motion.canAnimate();
            if (moving && frame != 0) spin = (spin + Math.min(64, now - frame) * .03f) % 360;
            frame = moving ? now : 0;
            int save = canvas.save();
            canvas.scale(AndroidUtilities.density, AndroidUtilities.density);
            canvas.translate(0, 16);
            if (mode == FULL) drawFull(canvas, moving);
            else if (mode == MINI) drawMini(canvas);
            else drawProfile(canvas, moving);
            canvas.restoreToCount(save);
            boolean continuous = mode == FULL && (FinegramPlayerConfig.INSTANCE.getWavyProgress()
                    || FinegramPlayerConfig.INSTANCE.getCoverStyle() == FinegramPlayerConfig.COVER_VINYL
                    || FinegramPlayerConfig.liveBackground())
                    || mode == PROFILE && !FinegramPlayerConfig.stockProfileCard() && (FinegramPlayerConfig.INSTANCE.getProfileRoundCover()
                    || FinegramPlayerConfig.INSTANCE.getProfileEqualizer()
                    || FinegramPlayerConfig.INSTANCE.getProfileProgress() == FinegramPlayerConfig.PROGRESS_WAVE);
            if (moving && continuous) postInvalidateDelayed(40);
        }

        private void box(Canvas c, float l, float t, float r, float b, float radius, int color) {
            paint.setShader(null);
            paint.setColor(color);
            rect.set(l, t, r, b);
            c.drawRoundRect(rect, radius, radius, paint);
        }

        private void label(Canvas c, String value, float left, float top, float available, float size, int color, boolean bold) {
            text.setTextSize(size * font);
            text.setTypeface(bold ? AndroidUtilities.bold() : android.graphics.Typeface.DEFAULT);
            text.setColor(color);
            String visible = TextUtils.ellipsize(value, text, Math.max(0, available), TextUtils.TruncateAt.END).toString();
            c.drawText(visible, left, top - text.ascent(), text);
        }

        private void cover(Canvas c, float left, float top, float size, boolean round, float rotation) {
            cover(c, left, top, size, round, rotation, size * .1f);
        }

        private void cover(Canvas c, float left, float top, float size, boolean round, float rotation, float radius) {
            int save = c.save();
            c.rotate(rotation, left + size / 2, top + size / 2);
            artMatrix.setScale(size, size);
            artMatrix.postTranslate(left, top);
            artwork.setLocalMatrix(artMatrix);
            paint.setShader(artwork);
            rect.set(left, top, left + size, top + size);
            c.drawRoundRect(rect, round ? size / 2 : radius, round ? size / 2 : radius, paint);
            paint.setShader(null);
            paint.setColor(Theme.multAlpha(Color.WHITE, .22f));
            c.drawCircle(left + size * .32f, top + size * .36f, size * .18f, paint);
            if (round) {
                paint.setColor(base);
                c.drawCircle(left + size / 2, top + size / 2, size * .06f, paint);
            }
            c.restoreToCount(save);
        }

        private void play(Canvas c, float cx, float cy, float size, int surface, int ink) {
            box(c, cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, playing ? size * .18f : size / 2, surface);
            paint.setColor(ink);
            float s = Math.min(10, size * .18f);
            if (playing) {
                c.drawRoundRect(cx - s * .75f, cy - s, cx - s * .2f, cy + s, 1.5f, 1.5f, paint);
                c.drawRoundRect(cx + s * .2f, cy - s, cx + s * .75f, cy + s, 1.5f, 1.5f, paint);
            } else {
                path.rewind();
                path.moveTo(cx - s * .55f, cy - s);
                path.lineTo(cx + s, cy);
                path.lineTo(cx - s * .55f, cy + s);
                path.close();
                c.drawPath(path, paint);
            }
        }

        private int onAccent() {
            return ColorUtils.calculateContrast(Color.BLACK, ColorUtils.setAlphaComponent(accent, 255))
                    >= ColorUtils.calculateContrast(Color.WHITE, ColorUtils.setAlphaComponent(accent, 255)) ? Color.BLACK : Color.WHITE;
        }

        private void icon(Canvas c, int res, float cx, float cy, int color) {
            if (res == -1 || res == -2) {
                drawSkip(c, cx, cy, res == -2, color);
                return;
            }
            android.graphics.drawable.Drawable d = icons.get(res);
            if (d == null) {
                d = getResources().getDrawable(res).mutate();
                icons.put(res, d);
            }
            d.setColorFilter(color, android.graphics.PorterDuff.Mode.SRC_IN);
            d.setBounds(Math.round(cx - 12), Math.round(cy - 12), Math.round(cx + 12), Math.round(cy + 12));
            d.draw(c);
        }

        private void drawSkip(Canvas c, float cx, float cy, boolean next, int color) {
            paint.setColor(color);
            float dir = next ? 1 : -1;
            for (int i = 0; i < 2; i++) {
                float x = cx + dir * (i * 7 - 8);
                path.rewind();
                path.moveTo(x, cy - 7);
                path.lineTo(x + dir * 7, cy);
                path.lineTo(x, cy + 7);
                path.close();
                c.drawPath(path, paint);
            }
            box(c, next ? cx + 7 : cx - 10, cy - 7, next ? cx + 10 : cx - 7, cy + 7, 1, color);
        }

        private void seek(Canvas c, float left, float right, float y, boolean wave, boolean moving, int color) {
            paint.setColor(Theme.multAlpha(foreground, .14f));
            paint.setStrokeWidth(2);
            c.drawLine(left, y, right, y, paint);
            float split = left + (right - left) * .42f;
            paint.setColor(color);
            if (wave) {
                path.rewind();
                float phase = moving ? spin / 15f : 0;
                for (float x = left; x <= split; x += 1.5f) {
                    float yy = y + (playing ? 2 : .6f) * (float) Math.sin((x - left + phase) / 14 * Math.PI * 2);
                    if (x == left) path.moveTo(x, yy); else path.lineTo(x, yy);
                }
                paint.setStyle(Paint.Style.STROKE);
                c.drawPath(path, paint);
                paint.setStyle(Paint.Style.FILL);
            } else c.drawLine(left, y, split, y, paint);
        }

        private void drawFull(Canvas c, boolean moving) {
            boolean expanded = FinegramPlayerConfig.INSTANCE.getFullscreenPlayer();
            int save = c.save();
            float outerWidth = width;
            if (expanded) {
                width = Math.max(280, width);
                c.translate((outerWidth - width * .6f) / 2, 0);
                c.scale(.6f, .6f);
            }
            float h = expanded ? full.height + queueExtra : compact.height;
            int bg = expanded && !FinegramPlayerConfig.themeBackground() ? ColorUtils.blendARGB(base, ART, .16f) : base;
            box(c, 0, 0, width, h, 28, bg);
            if (expanded && !FinegramPlayerConfig.themeBackground()) {
                artMatrix.setScale(width, h);
                if (FinegramPlayerConfig.liveBackground() && moving) artMatrix.postRotate((float) Math.sin(spin / 45f) * 12, width / 2, h / 2);
                artwork.setLocalMatrix(artMatrix);
                paint.setShader(artwork);
                paint.setAlpha(Theme.isCurrentThemeDark() ? 60 : 36);
                rect.set(0, 0, width, h);
                c.drawRoundRect(rect, 28, 28, paint);
                paint.setAlpha(255);
                paint.setShader(null);
            }
            float titleTop, authorTop, seekTop, controlsTop, toolsTop;
            int side = expanded ? MusicPlayerLayout.contentSideInset(Math.round(width)) : 20;
            if (expanded) {
                box(c, MusicPlayerLayout.panelSideInset(Math.round(width)), full.panelTop, width - MusicPlayerLayout.panelSideInset(Math.round(width)), full.panelBottom + queueExtra, 28, ColorUtils.blendARGB(base, accent, .07f));
                int style = FinegramPlayerConfig.INSTANCE.getCoverStyle();
                float scale = FinegramPlayerConfig.INSTANCE.getCoverBreath() && !playing ? .88f : 1;
                float size = full.artwork * scale;
                cover(c, (width - size) / 2, full.artworkTop + (full.artwork - size) / 2, size,
                        style == FinegramPlayerConfig.COVER_VINYL, style == FinegramPlayerConfig.COVER_VINYL ? spin : style == FinegramPlayerConfig.COVER_TILT ? -4 : 0);
                titleTop = full.titleTop; authorTop = full.authorTop; seekTop = full.seekTop; controlsTop = full.controlsTop; toolsTop = full.toolsTop;
            } else {
                cover(c, width - 64, 20, 44, false, 0);
                titleTop = compact.titleTop; authorTop = compact.authorTop; seekTop = compact.seekTop; controlsTop = compact.controlsTop; toolsTop = compact.toolsTop;
            }
            label(c, getString(R.string.FG_Player_Preview_Title), side, titleTop, width - side * 2 - (expanded ? 0 : 64), expanded ? 22 : 17, foreground, true);
            label(c, getString(R.string.FG_Player_Preview_Artist), side, authorTop, width - side * 2 - (expanded ? 0 : 64), expanded ? 14 : 13, gray, false);
            seek(c, side, width - side, seekTop + 22, FinegramPlayerConfig.INSTANCE.getWavyProgress(), moving, accent);
            label(c, "1:24", side, seekTop + 44, 80, 12, gray, false);
            label(c, "3:20", width - side - 45 * font, seekTop + 44, 45 * font, 12, gray, false);
            for (int i : new int[]{4, 1, 2, 3, 0}) {
                if (!expanded && i == 4) continue;
                float size = expanded ? full.buttonSize(i) : 48;
                float x = expanded ? full.buttonLeft(Math.round(width), i) : 4 + (48 + (width - 248) / 4) * i;
                float y = controlsTop + (expanded ? full.controlsHeight : 66) / 2f;
                int tonal = ColorUtils.blendARGB(base, foreground, .13f);
                if (i == 2) play(c, x + size / 2, y, size, accent, onAccent());
                else {
                    box(c, x, y - size / 2, x + size, y + size / 2, size / 2, tonal);
                    icon(c, i == 1 ? -1 : i == 3 ? -2 : i == 4 ? R.drawable.player_new_shuffle : R.drawable.player_new_repeatall, x + size / 2, y, foreground);
                }
            }
            int tonal = ColorUtils.blendARGB(base, foreground, .13f);
            float toolsHeight = expanded ? full.toolsHeight : compact.toolsHeight;
            float speedLeft = expanded ? tools.speedLeft : 20;
            float queueLeft = expanded ? tools.queueLeft : 88;
            float queueWidth = expanded ? tools.queueWidth : width - 108;
            box(c, speedLeft, toolsTop, speedLeft + (expanded ? 72 : 48), toolsTop + toolsHeight, 16, tonal);
            label(c, "1×", speedLeft + 14, toolsTop + (toolsHeight - 18 * font) / 2, expanded ? 48 : 32, 14, foreground, true);
            if (expanded) {
                box(c, tools.optionsLeft, toolsTop, tools.optionsLeft + 48, toolsTop + toolsHeight, 16, tonal);
                icon(c, R.drawable.ic_ab_other, tools.optionsLeft + 24, toolsTop + toolsHeight / 2, foreground);
            }
            float queueTop = toolsTop + (expanded ? queueExtra : 0);
            box(c, queueLeft, queueTop, queueLeft + queueWidth, queueTop + toolsHeight, 16, tonal);
            icon(c, R.drawable.msg_list, queueLeft + 24, queueTop + toolsHeight / 2, foreground);
            label(c, getString(R.string.Next), queueLeft + 44, queueTop + (toolsHeight - 18 * font) / 2, Math.max(0, queueWidth - 54), 14, foreground, true);
            c.restoreToCount(save);
            width = outerWidth;
        }

        private void drawMini(Canvas c) {
            float h = Math.max(64, (float) Math.ceil(40 * font) + 24);
            boolean next = FinegramPlayerConfig.INSTANCE.getMiniPlayerNextTrack() && width - 68 - 148 >= 80;
            float right = next ? 148 : 104;
            boolean showCover = FinegramPlayerConfig.INSTANCE.getMiniPlayerShowCover() && width - 68 - right >= 70;
            int surface = FinegramPlayerConfig.INSTANCE.getMiniPlayerGlass() ? ColorUtils.blendARGB(base, ART, .06f) : base;
            box(c, 0, 0, width, h, 20, surface);
            if (FinegramPlayerConfig.INSTANCE.getMiniPlayerArtworkTint()) box(c, 0, 0, width, h, 20, ColorUtils.blendARGB(surface, ART, .16f));
            if (showCover) cover(c, 16, (h - 40) / 2, 40, false, 0);
            float left = showCover ? 68 : 16;
            label(c, getString(R.string.FG_Player_Preview_Title), left, (h - 40 * font) / 2, width - left - right, 15, foreground, true);
            label(c, getString(R.string.FG_Player_Preview_Artist), left, (h - 40 * font) / 2 + 22 * font, width - left - right, 13, gray, false);
            play(c, width - (next ? 122 : 78), h / 2, 44, Theme.multAlpha(foreground, .14f), foreground);
            if (next) icon(c, -2, width - 78, h / 2, foreground);
            icon(c, R.drawable.msg_close, width - 34, h / 2, foreground);
        }

        private void drawCapsule(Canvas c) {
            float capsuleHeight = 44, capsuleWidth = Math.min(width - 32, 300);
            float left = (width - capsuleWidth) / 2f, top = (profile.height - capsuleHeight) / 2f;
            int textColor = Theme.isCurrentThemeDark() ? Color.WHITE : Theme.getColor(Theme.key_windowBackgroundWhiteBlackText);
            final boolean dark = Theme.isCurrentThemeDark();
            box(c, left, top, left + capsuleWidth, top + capsuleHeight, capsuleHeight / 2, dark ? 0x1FFFFFFF : 0x12000000);
            if (FinegramPlayerConfig.INSTANCE.getProfileProgress() != FinegramPlayerConfig.PROGRESS_NONE) {
                path.rewind();
                rect.set(left, top, left + capsuleWidth, top + capsuleHeight);
                path.addRoundRect(rect, capsuleHeight / 2, capsuleHeight / 2, Path.Direction.CW);
                int save = c.save();
                c.clipPath(path);
                paint.setShader(null);
                paint.setColor(Theme.multAlpha(textColor, dark ? .12f : .08f));
                c.drawRect(left, top, left + capsuleWidth * .42f, top + capsuleHeight, paint);
                c.restoreToCount(save);
            }
            paint.setShader(null);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1f);
            paint.setColor(dark ? 0x29FFFFFF : 0x1A000000);
            rect.set(left + .5f, top + .5f, left + capsuleWidth - .5f, top + capsuleHeight - .5f);
            c.drawRoundRect(rect, capsuleHeight / 2, capsuleHeight / 2, paint);
            paint.setStyle(Paint.Style.FILL);
            cover(c, left + 6, top + 6, 32, true, 0, 16);
            box(c, left + 6, top + 6, left + 38, top + 38, 16, Theme.multAlpha(Color.BLACK, .34f));
            play(c, left + 22, top + 22, 32, Color.TRANSPARENT, Color.WHITE);
            label(c, getString(R.string.FG_Player_Preview_Title) + " — " + getString(R.string.FG_Player_Preview_Artist),
                    left + 48, top + 13, capsuleWidth - 48 - 30, 15, textColor, true);
            paint.setShader(null);
            paint.setColor(Theme.multAlpha(textColor, .6f));
            paint.setStrokeWidth(1.6f);
            paint.setStrokeCap(Paint.Cap.ROUND);
            float chevronX = left + capsuleWidth - 16, chevronY = top + capsuleHeight / 2;
            c.drawLine(chevronX, chevronY - 4.5f, chevronX + 4.2f, chevronY, paint);
            c.drawLine(chevronX + 4.2f, chevronY, chevronX, chevronY + 4.5f, paint);
            paint.setStrokeCap(Paint.Cap.BUTT);
        }

        private void drawProfile(Canvas c, boolean moving) {
            if (FinegramPlayerConfig.stockProfileCard()) {
                stockProfile.setButtonColor(base, false);
                int save = c.save();
                c.scale(1f / AndroidUtilities.density, 1f / AndroidUtilities.density);
                stockProfile.draw(c);
                c.restoreToCount(save);
                return;
            }
            if (FinegramPlayerConfig.capsuleProfileCard()) {
                drawCapsule(c);
                return;
            }
            ProfileMusicLayout l = profile;
            int style = FinegramPlayerConfig.INSTANCE.getProfileCardStyle();
            int surface = base;
            if (style == FinegramPlayerConfig.CARD_VIVID) {
                Color.colorToHSV(ART, artworkHsv);
                artworkHsv[1] = Math.max(.5f, artworkHsv[1]);
                artworkHsv[2] = Theme.isCurrentThemeDark() ? .42f : .58f;
                surface = Color.HSVToColor(artworkHsv);
            } else if (style == FinegramPlayerConfig.CARD_ARTWORK) {
                surface = ColorUtils.blendARGB(base, ART, Theme.isCurrentThemeDark() ? .14f : .05f);
            } else if (style == FinegramPlayerConfig.CARD_GLASS) {
                surface = Theme.multAlpha(surface, Theme.isCurrentThemeDark() ? .58f : .66f);
            }
            int textColor = ColorUtils.calculateContrast(Color.BLACK, ColorUtils.setAlphaComponent(surface, 255))
                    >= ColorUtils.calculateContrast(Color.WHITE, ColorUtils.setAlphaComponent(surface, 255)) ? Color.BLACK : Color.WHITE;
            box(c, 16, 8, width - 16, l.height - 8, 24, surface);
            boolean round = FinegramPlayerConfig.INSTANCE.getProfileRoundCover();
            cover(c, l.coverLeft, l.coverTop, l.coverSize, round, round ? spin : 0, 14);
            label(c, getString(R.string.FG_Player_Preview_Title), l.textLeft, l.titleTop, l.textWidth, 17, textColor, true);
            label(c, getString(R.string.FG_Player_Preview_Artist), l.textLeft, l.authorTop, l.textWidth, 13, Theme.multAlpha(textColor, .78f), false);
            int buttonSurface = ColorUtils.blendARGB(ART, textColor, .55f);
            int buttonInk = ColorUtils.calculateContrast(Color.BLACK, buttonSurface)
                    >= ColorUtils.calculateContrast(Color.WHITE, buttonSurface) ? Color.BLACK : Color.WHITE;
            play(c, l.buttonLeft + 24, l.buttonTop + 24, 48, buttonSurface, buttonInk);
            if (FinegramPlayerConfig.INSTANCE.getProfileEqualizer()) {
                box(c, l.coverLeft, l.coverTop, l.coverLeft + l.coverSize, l.coverTop + l.coverSize,
                        round ? l.coverSize / 2 : 14, Theme.multAlpha(Color.BLACK, .32f));
                for (int i = 0; i < 4; i++) {
                    float height = playing && moving ? 6 + Math.abs((float) Math.sin(spin / 20 + i)) * 10 : 5 + i % 2 * 2;
                    float left = l.coverLeft + (l.coverSize - 21) / 2 + i * 6;
                    float bottom = l.coverTop + l.coverSize / 2 + 8;
                    box(c, left, bottom - height, left + 3, bottom, 1.5f, Color.WHITE);
                }
            }
            int progress = FinegramPlayerConfig.INSTANCE.getProfileProgress();
            if (progress != FinegramPlayerConfig.PROGRESS_NONE) seek(c, l.progressLeft, l.progressRight, l.progressY,
                    progress == FinegramPlayerConfig.PROGRESS_WAVE, moving, textColor);
        }
    }
}
