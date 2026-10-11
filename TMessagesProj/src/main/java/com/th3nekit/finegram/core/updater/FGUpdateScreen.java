/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.ColorUtils;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.browser.Browser;
import org.telegram.messenger.browser.Browser;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Cells.TextCell;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.LaunchActivity;
import org.telegram.ui.LauncherIconController;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.concurrent.atomic.AtomicBoolean;

import com.th3nekit.finegram.core.FinegramLogger;
import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;
import com.th3nekit.finegram.misc.Constants;

public class FGUpdateScreen extends BaseFragment implements NotificationCenter.NotificationCenterDelegate {

    private static final String CHANGELOG_ASSET = "changelog.md";

    private final TLRPC.TL_help_appUpdate update;
    private final FGClientUpdates.Update directUpdate;
    private final boolean preview;

    private ButtonWithCounterView actionButton;
    private ProgressLine progressLine;
    private AtomicBoolean downloadCancel;

    public FGUpdateScreen(TLRPC.TL_help_appUpdate update) {
        this(update, false);
    }

    public FGUpdateScreen(TLRPC.TL_help_appUpdate update, boolean preview) {
        this.update = update;
        this.directUpdate = update != null ? FGClientUpdates.getPending() : null;
        this.preview = preview;
    }

    private boolean hasUpdate() {
        return update != null;
    }

    @Override
    public boolean onFragmentCreate() {
        NotificationCenter.getInstance(currentAccount).addObserver(this, NotificationCenter.fileLoaded);
        NotificationCenter.getInstance(currentAccount).addObserver(this, NotificationCenter.fileLoadProgressChanged);
        NotificationCenter.getInstance(currentAccount).addObserver(this, NotificationCenter.fileLoadFailed);
        return super.onFragmentCreate();
    }

    @Override
    public void onFragmentDestroy() {
        if (downloadCancel != null) {
            downloadCancel.set(true);
            downloadCancel = null;
        }
        NotificationCenter.getInstance(currentAccount).removeObserver(this, NotificationCenter.fileLoaded);
        NotificationCenter.getInstance(currentAccount).removeObserver(this, NotificationCenter.fileLoadProgressChanged);
        NotificationCenter.getInstance(currentAccount).removeObserver(this, NotificationCenter.fileLoadFailed);
        super.onFragmentDestroy();
    }

    @Override
    public View createView(Context context) {
        final int background = getThemedColor(Theme.key_windowBackgroundWhite);
        final int accent = getThemedColor(Theme.key_windowBackgroundWhiteBlueText);
        final int text = getThemedColor(Theme.key_windowBackgroundWhiteBlackText);
        final int muted = getThemedColor(Theme.key_windowBackgroundWhiteGrayText);

        actionBar.setBackButtonImage(R.drawable.ic_ab_back);
        actionBar.setAllowOverlayTitle(false);
        actionBar.setCastShadows(false);
        actionBar.setAddToContainer(false);
        actionBar.setBackgroundDrawable(null);
        actionBar.setItemsColor(text, false);
        actionBar.setItemsBackgroundColor(Theme.multAlpha(text, 0.12f), false);
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                if (id == -1) finishFragment();
            }
        });

        final LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(background);

        root.addView(header(context, background, accent, text, muted),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(20), dp(18), dp(20), dp(18));

        body.addView(sectionLabel(context, accent, getString(R.string.FG_Update_WhatsNew)),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final TextView changelog = new TextView(context);
        changelog.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 15);
        changelog.setTextColor(text);
        changelog.setLineSpacing(dp(4), 1.0f);
        changelog.setMovementMethod(new AndroidUtilities.LinkMovementMethodMy());
        changelog.setLinkTextColor(accent);
        changelog.setText(FGMarkdown.parse(changelogText(), accent));
        body.addView(changelog, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 10, 0, 0));

        if (!hasUpdate()) {
            body.addView(sectionLabel(context, accent, getString(R.string.FG_Update_CheckSection)),
                    LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 24, 0, 0));
            body.addView(settings(context), LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, -20, 6, -20, 0));
        }

        final ScrollView scroll = new ScrollView(context);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setClipToPadding(false);
        scroll.addView(body, new FrameLayout.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        root.addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 0, 1f));

        root.addView(dock(context, background, muted),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        fragmentView = root;
        return root;
    }

    private View header(Context context, int background, int accent, int text, int muted) {
        final FrameLayout header = new FrameLayout(context);
        final GradientDrawable gradient = new GradientDrawable(
                GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{ColorUtils.blendARGB(background, accent, 0.20f), background});
        header.setBackground(gradient);

        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);

        final int statusBar = actionBar.getOccupyStatusBar() ? AndroidUtilities.statusBarHeight : 0;
        column.setPadding(dp(20), statusBar + ActionBar.getCurrentActionBarHeight() + dp(6), dp(20), dp(22));

        column.addView(new AppIconView(context), LayoutHelper.createLinear(66, 66, Gravity.CENTER_HORIZONTAL));

        final TextView title = new TextView(context);
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setTextColor(text);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        title.setText(hasUpdate()
                ? LocaleController.formatString(R.string.FG_Update_Title, update.version)
                : getString(R.string.FG_Update_UpToDate));
        column.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        final TextView meta = new TextView(context);
        meta.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        meta.setTextColor(muted);
        meta.setGravity(Gravity.CENTER_HORIZONTAL);
        meta.setText(metaText());
        column.addView(meta, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 5, 0, 0));

        header.addView(column, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        header.addView(actionBar, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        return header;
    }

    private TextView sectionLabel(Context context, int accent, CharSequence label) {
        final TextView view = new TextView(context);
        view.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
        view.setTypeface(AndroidUtilities.bold());
        view.setTextColor(accent);
        view.setLetterSpacing(0.08f);
        view.setAllCaps(true);
        view.setText(label);
        return view;
    }

    private View settings(Context context) {
        final LinearLayout block = new LinearLayout(context);
        block.setOrientation(LinearLayout.VERTICAL);

        final TextCell betas = new TextCell(context, 23, false, true, getResourceProvider());
        betas.setTextAndCheckAndIcon(getString(R.string.UP_InstallBetas), FinegramCoreConfig.INSTANCE.getInstallBetas(), R.drawable.test_tube_solar, true);
        betas.setOnClickListener(v -> {
            FinegramCoreConfig.INSTANCE.setInstallBetas(!FinegramCoreConfig.INSTANCE.getInstallBetas());
            betas.setChecked(FinegramCoreConfig.INSTANCE.getInstallBetas());
        });
        block.addView(betas);

        final TextCell auto = new TextCell(context, 23, false, true, getResourceProvider());
        auto.setTextAndCheckAndIcon(getString(R.string.UP_Auto_OTA), FinegramCoreConfig.INSTANCE.getAutoOTA(), R.drawable.msg_retry, false);
        auto.setOnClickListener(v -> {
            FinegramCoreConfig.INSTANCE.setAutoOTA(!FinegramCoreConfig.INSTANCE.getAutoOTA());
            auto.setChecked(FinegramCoreConfig.INSTANCE.getAutoOTA());
        });
        block.addView(auto);
        return block;
    }

    private View dock(Context context, int background, int muted) {
        final LinearLayout dock = new LinearLayout(context);
        dock.setOrientation(LinearLayout.VERTICAL);
        dock.setBackgroundColor(background);
        dock.setPadding(dp(16), dp(10), dp(16), dp(16));

        final View divider = new View(context);
        divider.setBackgroundColor(Theme.multAlpha(muted, 0.25f));
        dock.addView(divider, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 1, 0, 0, 0, 10));

        progressLine = new ProgressLine(context);
        progressLine.setVisibility(View.GONE);
        dock.addView(progressLine, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 3, 0, 0, 0, 10));

        actionButton = new ButtonWithCounterView(context, getResourceProvider());
        actionButton.setText(hasUpdate() ? downloadButtonText() : getString(R.string.UP_CheckForUpdates), false);
        actionButton.setOnClickListener(v -> onActionClick());
        dock.addView(actionButton, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48));

        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        if (hasUpdate() && !update.can_not_skip) {
            final ButtonWithCounterView later = new ButtonWithCounterView(context, false, getResourceProvider());
            later.setText(getString(R.string.AppUpdateRemindMeLater), false);
            later.setOnClickListener(v -> {
                SharedConfig.lastUpdateCheckTime = System.currentTimeMillis();
                SharedConfig.pendingAppUpdate = null;
                FGClientUpdates.setPending(null);
                SharedConfig.saveConfig();
                NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.appUpdateAvailable);
                finishFragment();
            });
            final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, dp(44), 1f);
            params.rightMargin = dp(8);
            row.addView(later, params);
        }
        final ButtonWithCounterView channel = new ButtonWithCounterView(context, false, getResourceProvider());
        channel.setText(getString(R.string.FG_Update_OpenChannel), false);
        channel.setOnClickListener(v -> {
            final String url = Constants.FG_APKS_CHANNEL_URL.isEmpty() ? Constants.FG_CHANNEL_URL : Constants.FG_APKS_CHANNEL_URL;
            if (!url.isEmpty()) Browser.openUrl(getContext(), url);
        });
        row.addView(channel, new LinearLayout.LayoutParams(0, dp(44), 1f));
        dock.addView(row, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 8, 0, 0));

        return dock;
    }

    private CharSequence metaText() {
        final StringBuilder text = new StringBuilder();
        if (hasUpdate()) {
            text.append(FGResourcesHelper.getFinegramVersion()).append(" → ").append(update.version);
            final String size = updateSize();
            if (size != null) text.append(" · ").append(size);
            if (!TextUtils.isEmpty(update.release_date)) text.append(" · ").append(update.release_date);
        } else {
            text.append(FGResourcesHelper.getFinegramVersion())
                    .append(" · ").append(FGResourcesHelper.getBuildType())
                    .append(" · ").append(FGResourcesHelper.getAbiCode());
        }
        return text;
    }

    private String updateSize() {
        if (directUpdate != null) {
            final FGClientUpdates.Build build = directUpdate.preferred();
            if (build != null && build.size > 0) return AndroidUtilities.formatFileSize(build.size);
        }
        if (update != null && update.document != null && update.document.size > 0) {
            return AndroidUtilities.formatFileSize(update.document.size);
        }
        return null;
    }

    private CharSequence changelogText() {
        if (hasUpdate() && !TextUtils.isEmpty(update.text)) {
            return update.text;
        }
        final String bundled = readBundledChangelog();
        if (!TextUtils.isEmpty(bundled)) {
            return bundled;
        }
        return getString(R.string.AppUpdateChangelogEmpty);
    }

    private String readBundledChangelog() {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                ApplicationLoader.applicationContext.getAssets().open(CHANGELOG_ASSET), "UTF-8"))) {
            final StringBuilder text = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                text.append(line).append('\n');
            }
            return text.toString().trim();
        } catch (Throwable e) {
            FinegramLogger.d("FGUpdates", () -> "описания версии нет в сборке");
            return null;
        }
    }

    private CharSequence downloadButtonText() {
        if (directUpdate != null && FGClientUpdates.readyFile(directUpdate) != null) {
            return getString(R.string.FG_Update_Install);
        }
        if (update != null && update.document != null) {
            final java.io.File path = FileLoader.getInstance(currentAccount).getPathToAttach(update.document, true);
            if (path != null && path.exists()) return getString(R.string.FG_Update_Install);
        }
        return getString(R.string.AppUpdateNow);
    }

    private void onActionClick() {
        if (preview) {
            BulletinFactory.of(this).createSimpleBulletin(R.raw.chats_infotip, getString(R.string.FG_Update_Preview)).show();
            return;
        }
        if (!hasUpdate()) {
            checkForUpdates();
            return;
        }
        if (downloadCancel != null) {
            downloadCancel.set(true);
            downloadCancel = null;
            progressLine.hide();
            actionButton.setText(downloadButtonText(), true);
            actionButton.setSubText(null, true);
            return;
        }
        if (directUpdate != null) {
            startDirectDownload();
        } else if (update.document != null) {
            startDocumentDownload();
        }
    }

    private void checkForUpdates() {
        actionButton.setLoading(true);
        SharedConfig.lastUpdateCheckTime = System.currentTimeMillis();
        if (getParentActivity() instanceof LaunchActivity launchActivity) {
            launchActivity.checkAppUpdate(true, new Browser.Progress() {
                @Override
                public void end() {
                    actionButton.setLoading(false);
                    if (SharedConfig.pendingAppUpdate != null) {
                        finishFragment();
                    } else {
                        BulletinFactory.of(FGUpdateScreen.this)
                                .createSimpleBulletin(R.raw.chats_infotip, getString(R.string.YourVersionIsLatest)).show();
                    }
                }
            });
        } else {
            actionButton.setLoading(false);
        }
    }

    private void startDirectDownload() {
        final java.io.File ready = FGClientUpdates.readyFile(directUpdate);
        if (ready != null) {
            FGClientUpdates.install(ready, getParentActivity(), getResourceProvider());
            return;
        }
        progressLine.show();
        actionButton.setText(getString(R.string.Cancel), true);
        actionButton.setSubText(LocaleController.formatString(R.string.FG_Update_Downloading, 0), true);
        downloadCancel = FGClientUpdates.download(directUpdate,
                (done, total) -> {
                    final float progress = total > 0 ? (float) done / total : 0;
                    progressLine.setProgress(progress);
                    actionButton.setSubText(LocaleController.formatString(R.string.FG_Update_Downloading, (int) (progress * 100)), false);
                },
                (file, error) -> {
                    downloadCancel = null;
                    progressLine.hide();
                    actionButton.setSubText(null, true);
                    if (file != null) {
                        actionButton.setText(getString(R.string.FG_Update_Install), true);
                        FGClientUpdates.install(file, getParentActivity(), getResourceProvider());
                    } else {
                        actionButton.setText(getString(R.string.AppUpdateNow), true);
                        BulletinFactory.of(this).createErrorBulletin(error == null ? getString(R.string.ErrorOccurred) : error).show();
                    }
                });
    }

    private void startDocumentDownload() {
        final java.io.File path = FileLoader.getInstance(currentAccount).getPathToAttach(update.document, true);
        if (path != null && path.exists()) {
            FGClientUpdates.install(path, getParentActivity(), getResourceProvider());
            return;
        }
        progressLine.show();
        actionButton.setSubText(LocaleController.formatString(R.string.FG_Update_Downloading, 0), true);
        FileLoader.getInstance(currentAccount).loadFile(update.document, "update", FileLoader.PRIORITY_NORMAL, 1);
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (update == null || update.document == null || actionButton == null) return;
        final String name = (String) args[0];
        if (!name.equals(FileLoader.getAttachFileName(update.document))) return;
        if (id == NotificationCenter.fileLoaded) {
            progressLine.hide();
            actionButton.setSubText(null, true);
            actionButton.setText(getString(R.string.FG_Update_Install), true);
        } else if (id == NotificationCenter.fileLoadFailed) {
            progressLine.hide();
            actionButton.setSubText(null, true);
            actionButton.setText(getString(R.string.AppUpdateNow), true);
        } else if (id == NotificationCenter.fileLoadProgressChanged) {
            final Long loaded = (Long) args[1];
            final Long total = (Long) args[2];
            final float progress = total > 0 ? Math.min(1f, loaded / (float) total) : 0;
            progressLine.setProgress(progress);
            actionButton.setSubText(LocaleController.formatString(R.string.FG_Update_Downloading, (int) (progress * 100)), false);
        }
    }

    private static class AppIconView extends View {

        private final Drawable background;
        private final Drawable foreground;
        private final RectF bounds = new RectF();
        private final Path clip = new Path();

        AppIconView(Context context) {
            super(context);
            LauncherIconController.LauncherIcon icon = LauncherIconController.LauncherIcon.DEFAULT_ICON;
            for (LauncherIconController.LauncherIcon candidate : LauncherIconController.LauncherIcon.values()) {
                if (LauncherIconController.isEnabled(candidate)) {
                    icon = candidate.replacement != null ? candidate.replacement : candidate;
                    break;
                }
            }
            background = ContextCompat.getDrawable(context, icon.background);
            foreground = ContextCompat.getDrawable(context, icon.foreground);
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            final int size = Math.min(getWidth(), getHeight());
            if (size <= 0) return;
            final float radius = size * 0.24f;
            bounds.set(0, 0, size, size);
            clip.reset();
            clip.addRoundRect(bounds, radius, radius, Path.Direction.CW);

            canvas.save();
            canvas.clipPath(clip);

            final int overscan = (int) (size * 0.125f);
            if (background != null) {
                background.setBounds(-overscan, -overscan, size + overscan, size + overscan);
                background.draw(canvas);
            }
            if (foreground != null) {
                foreground.setBounds(-overscan, -overscan, size + overscan, size + overscan);
                foreground.draw(canvas);
            }
            canvas.restore();
        }
    }

    private class ProgressLine extends View {

        private final Paint track = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF rect = new RectF();
        private float shown;
        private ValueAnimator animator;

        ProgressLine(Context context) {
            super(context);
            final int color = getThemedColor(Theme.key_featuredStickers_addButton);
            track.setColor(Theme.multAlpha(color, 0.2f));
            fill.setColor(color);
        }

        void show() {
            setVisibility(VISIBLE);
            shown = 0;
            invalidate();
        }

        void hide() {
            setVisibility(GONE);
            shown = 0;
        }

        void setProgress(float value) {
            final float target = Math.max(0, Math.min(1, value));
            if (animator != null) animator.cancel();
            animator = ValueAnimator.ofFloat(shown, target);
            animator.setDuration(180);
            animator.setInterpolator(CubicBezierInterpolator.EASE_OUT);
            animator.addUpdateListener(a -> {
                shown = (float) a.getAnimatedValue();
                invalidate();
            });
            animator.start();
        }

        @Override
        protected void onDraw(@NonNull Canvas canvas) {
            final float radius = getHeight() / 2f;
            rect.set(0, 0, getWidth(), getHeight());
            canvas.drawRoundRect(rect, radius, radius, track);
            if (shown > 0) {
                rect.set(0, 0, Math.max(getHeight(), getWidth() * shown), getHeight());
                canvas.drawRoundRect(rect, radius, radius, fill);
            }
        }
    }
}
