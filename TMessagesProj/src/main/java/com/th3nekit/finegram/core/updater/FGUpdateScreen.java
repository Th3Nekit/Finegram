/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.core.updater;

import static org.telegram.messenger.AndroidUtilities.dp;
import static org.telegram.messenger.LocaleController.getString;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
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
import androidx.core.app.NotificationManagerCompat;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.FileLoader;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.browser.Browser;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Cells.TextCell;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.LauncherIconController;
import org.telegram.ui.Stories.recorder.ButtonWithCounterView;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.core.helpers.FGResourcesHelper;
import com.th3nekit.finegram.misc.Constants;

import java.util.ArrayList;

/**
 * Экран обновления клиента.
 *
 * Не шторка, а отдельный экран: список изменений бывает длинным, и в шторке он оказывался
 * зажат между значком и кнопками. Сверху — значок приложения на градиенте, ниже отдельным
 * разделом «Что нового», внизу кнопка на своей полке.
 *
 * Экран подписывается на состояние загрузки и не владеет её жизненным циклом.
 */
public class FGUpdateScreen extends BaseFragment implements NotificationCenter.NotificationCenterDelegate {

    private final TLRPC.TL_help_appUpdate update;
    private final FGClientUpdates.Update directUpdate;
    private final boolean preview;

    private ButtonWithCounterView actionButton;
    private ProgressLine progressLine;
    private TextView changelogView;
    private TextView downloadStatus;
    private TextView storageHint;
    private TextView headerTitle;
    private boolean checking;
    private int checkGeneration;
    private Runnable checkTimeout;
    private final FGUpdateDownload.Listener downloadListener = this::renderDownload;

    public FGUpdateScreen(TLRPC.TL_help_appUpdate update) {
        this(update, false);
    }

    public FGUpdateScreen(TLRPC.TL_help_appUpdate update, boolean preview) {
        FGClientUpdates.Update pending = FGClientUpdates.getPending();
        FGUpdateDownload.State saved = FGUpdateDownload.getState();
        final boolean hasSavedDownload = saved.update != null && saved.phase != FGUpdateDownload.Phase.IDLE;
        if (!preview && hasSavedDownload) {
            pending = saved.update;
        }
        this.directUpdate = !preview && pending != null
                && (hasSavedDownload || update == null || update.document == null && pending.version.equals(update.version)) ? pending : null;
        this.update = directUpdate != null ? UpdateHelper.toAppUpdate(directUpdate) : update;
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
        FGUpdateDownload.initialize();
        FGUpdateDownload.addListener(downloadListener);
        if (!preview) FGUpdatePrompts.recordShown(update, directUpdate, System.currentTimeMillis());
        return super.onFragmentCreate();
    }

    @Override
    public void onFragmentDestroy() {
        checkGeneration++;
        if (checkTimeout != null) AndroidUtilities.cancelRunOnUIThread(checkTimeout);
        changelogView = null;
        actionButton = null;
        downloadStatus = null;
        storageHint = null;
        FGUpdateDownload.removeListener(downloadListener);
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

        final int toolbarHeight = ActionBar.getCurrentActionBarHeight()
                + (actionBar.getOccupyStatusBar() ? AndroidUtilities.statusBarHeight : 0);
        root.setPadding(0, toolbarHeight, 0, 0);

        final LinearLayout scrollContent = new LinearLayout(context);
        scrollContent.setOrientation(LinearLayout.VERTICAL);
        final View header = header(context, background, accent, text, muted);
        final GradientDrawable headerBackground = (GradientDrawable) header.getBackground();
        final int headerTopColor = ColorUtils.blendARGB(background, accent, 0.20f);
        final GradientDrawable toolbarBackground = new GradientDrawable(
                GradientDrawable.Orientation.TOP_BOTTOM, new int[]{headerTopColor, headerTopColor});
        scrollContent.addView(header,
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(20), dp(18), dp(20), dp(18));
        body.setClipToPadding(false);

        body.addView(sectionLabel(context, accent, getString(R.string.FG_Update_WhatsNew)),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final TextView history = new TextView(context);
        history.setText(getString(R.string.FGUpdateChangelogHistory));
        history.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        history.setTextColor(accent);
        history.setGravity(Gravity.CENTER_VERTICAL);
        history.setMinHeight(dp(48));
        history.setBackground(Theme.createSelectorDrawable(Theme.getColor(Theme.key_listSelector, getResourceProvider()), 2));
        history.setOnClickListener(v -> showChangelogHistory());
        body.addView(history, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 12, 0, 0));

        final TextView changelog = new TextView(context);
        changelogView = changelog;
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

        storageHint = new TextView(context);
        storageHint.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        storageHint.setTextColor(muted);
        storageHint.setLineSpacing(dp(3), 1f);
        storageHint.setPadding(0, dp(16), 0, 0);
        storageHint.setOnClickListener(v -> {
            if (!NotificationManagerCompat.from(context).areNotificationsEnabled()) {
                Intent intent = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
                if (Build.VERSION.SDK_INT < 26) {
                    intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                            android.net.Uri.parse("package:" + context.getPackageName()));
                }
                try { context.startActivity(intent); } catch (Exception ignored) { }
            }
        });
        body.addView(storageHint, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final ScrollView scroll = new ScrollView(context);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setClipToPadding(false);
        scrollContent.addView(body, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        scroll.addView(scrollContent, new FrameLayout.LayoutParams(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
        root.addView(scroll, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 0, 1f));

        final Runnable updateHeaderBackground = () -> updateHeaderBackgrounds(
                toolbarBackground, headerBackground, headerTopColor, background,
                root.getPaddingTop(), header.getHeight(), scroll.getScrollY());
        header.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                updateHeaderBackground.run());
        scroll.getViewTreeObserver().addOnScrollChangedListener(updateHeaderBackground::run);

        root.addView(dock(context, background, muted),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        final FrameLayout container = new FrameLayout(context);
        container.setBackgroundColor(background);
        container.addView(root, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT));
        actionBar.setBackground(toolbarBackground);
        container.addView(actionBar, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.TOP));
        fragmentView = container;
        renderDownload(FGUpdateDownload.getState());
        if (!hasUpdate() && !preview) {
            FGClientUpdates.refreshInstalledChangelog(this::refreshChangelog);
        }
        return container;
    }

    private static void updateHeaderBackgrounds(GradientDrawable toolbar, GradientDrawable header,
                                               int topColor, int background, int toolbarHeight,
                                               int headerHeight, int scrollY) {
        final float height = Math.max(1, toolbarHeight + headerHeight);
        final float toolbarTop = Math.min(1f, Math.max(0f, scrollY / height));
        final float toolbarBottom = Math.min(1f, Math.max(0f, (scrollY + toolbarHeight) / height));
        toolbar.setColors(new int[]{ColorUtils.blendARGB(topColor, background, toolbarTop),
                ColorUtils.blendARGB(topColor, background, toolbarBottom)});
        header.setColors(new int[]{ColorUtils.blendARGB(topColor, background, toolbarHeight / height), background});
    }

    @Override
    public void onResume() {
        super.onResume();
        renderDownload(FGUpdateDownload.getState());
        renderDocumentDownload();
    }

    /** Шапка: градиент, значок, версия и строка со сведениями. */
    private View header(Context context, int background, int accent, int text, int muted) {
        final FrameLayout header = new FrameLayout(context);
        final GradientDrawable gradient = new GradientDrawable(
                GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{ColorUtils.blendARGB(background, accent, 0.20f), background});
        header.setBackground(gradient);

        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);
        column.setPadding(dp(20), dp(10), dp(20), dp(22));

        column.addView(new AppIconView(context), LayoutHelper.createLinear(66, 66, Gravity.CENTER_HORIZONTAL));

        final TextView title = new TextView(context);
        headerTitle = title;
        title.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setTextColor(text);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        title.setText(hasUpdate()
                ? LocaleController.formatString(R.string.FG_Update_Title, update.version)
                : getString(R.string.FG_Update_InstalledTitle));
        column.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 14, 0, 0));

        final TextView meta = new TextView(context);
        meta.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 13);
        meta.setTextColor(muted);
        meta.setGravity(Gravity.CENTER_HORIZONTAL);
        meta.setText(metaText());
        column.addView(meta, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 5, 0, 0));

        header.addView(column, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));
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

    /** Переключатели проверки: нужны только когда обновления нет. */
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

    /** Полка с кнопкой: она всегда на виду, сколько бы ни было изменений. */
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

        downloadStatus = new TextView(context);
        downloadStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        downloadStatus.setTextColor(muted);
        downloadStatus.setGravity(Gravity.CENTER);
        downloadStatus.setVisibility(View.GONE);
        dock.addView(downloadStatus, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 0, 0, 0, 10));

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
                FGUpdatePrompts.defer(update, directUpdate, System.currentTimeMillis());
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

    /** Что показать в разделе «Что нового»: описание обновления или описание своей версии. */
    private CharSequence changelogText() {
        final String changelog = hasUpdate() ? update.text : FGClientUpdates.getInstalledChangelog();
        return TextUtils.isEmpty(changelog) ? getString(R.string.AppUpdateChangelogEmpty) : changelog;
    }

    private void showChangelogHistory() {
        if (getParentActivity() == null) return;
        final ArrayList<FGClientUpdates.Changelog> entries = FGClientUpdates.getChangelogHistory();
        if (entries.isEmpty()) return;
        final CharSequence[] versions = new CharSequence[entries.size()];
        for (int i = 0; i < entries.size(); i++) versions[i] = "Finegram " + entries.get(i).version;
        showDialog(new AlertDialog.Builder(getParentActivity(), getResourceProvider())
                .setTitle(getString(R.string.FGUpdateChangelogHistory))
                .setItems(versions, (dialog, which) -> {
                    if (getParentActivity() == null) return;
                    FGClientUpdates.Changelog entry = entries.get(which);
                    showDialog(new AlertDialog.Builder(getParentActivity(), getResourceProvider())
                            .setTitle("Finegram " + entry.version)
                            .setMessage(FGMarkdown.parse(entry.text, getThemedColor(Theme.key_windowBackgroundWhiteBlueText)))
                            .setPositiveButton(getString(R.string.OK), null).create());
                })
                .setNegativeButton(getString(R.string.Cancel), null).create());
    }

    private void refreshChangelog() {
        if (changelogView != null) {
            changelogView.setText(FGMarkdown.parse(changelogText(), getThemedColor(Theme.key_windowBackgroundWhiteBlueText)));
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
        if (directUpdate != null) {
            final FGUpdateDownload.State state = FGUpdateDownload.getState();
            if (FGUpdateDownload.isSame(state.update, directUpdate)
                    && state.phase == FGUpdateDownload.Phase.DOWNLOADING) {
                FGUpdateDownload.pause();
            } else if (state.phase != FGUpdateDownload.Phase.VERIFYING) {
                startDirectDownload();
            }
        } else if (update.document != null) {
            startDocumentDownload();
        }
    }

    private void checkForUpdates() {
        if (checking) return;
        checking = true;
        final int generation = ++checkGeneration;
        actionButton.setLoading(true);
        SharedConfig.lastUpdateCheckTime = System.currentTimeMillis();
        SharedConfig.saveConfig();
        checkTimeout = () -> {
            if (generation != checkGeneration || actionButton == null) return;
            checkGeneration++;
            checking = false;
            actionButton.setLoading(false);
            BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_Update_CheckFailed)).show();
        };
        AndroidUtilities.runOnUIThread(checkTimeout, 45_000);
        UpdateHelper.getInstance().checkNewVersionAvailable((result, error) -> AndroidUtilities.runOnUIThread(() -> {
            if (generation != checkGeneration || actionButton == null || changelogView == null) return;
            if (checkTimeout != null) AndroidUtilities.cancelRunOnUIThread(checkTimeout);
            checking = false;
            actionButton.setLoading(false);
            if (result != null) {
                SharedConfig.setNewAppVersionAvailable(result);
                NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.appUpdateAvailable);
                presentFragment(new FGUpdateScreen(result), true);
            } else if (error != null) {
                BulletinFactory.of(this).createErrorBulletin(getString(R.string.FG_Update_CheckFailed)).show();
            } else {
                SharedConfig.setNewAppVersionAvailable(null);
                if (headerTitle != null) headerTitle.setText(getString(R.string.FG_Update_UpToDate));
                refreshChangelog();
                BulletinFactory.of(this).createSimpleBulletin(R.raw.chats_infotip, getString(R.string.YourVersionIsLatest)).show();
            }
            SharedConfig.saveConfig();
        }));
    }

    private void startDirectDownload() {
        final java.io.File ready = FGClientUpdates.readyFile(directUpdate);
        if (ready != null) {
            FGClientUpdates.install(ready, getParentActivity(), getResourceProvider());
            return;
        }
        if (Build.VERSION.SDK_INT >= 33 && getParentActivity() != null
                && getParentActivity().checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                != android.content.pm.PackageManager.PERMISSION_GRANTED
                && !BaseRemoteHelper.preferences.getBoolean("update_notification_asked", false)) {
            BaseRemoteHelper.preferences.edit().putBoolean("update_notification_asked", true).apply();
            getParentActivity().requestPermissions(new String[]{android.Manifest.permission.POST_NOTIFICATIONS}, 416);
        }
        FGUpdateDownload.start(directUpdate);
    }

    private void renderDownload(FGUpdateDownload.State state) {
        if (actionButton == null || progressLine == null) return;
        if (storageHint != null) {
            String hint = getString(R.string.FG_Update_StorageHint);
            if (!NotificationManagerCompat.from(storageHint.getContext()).areNotificationsEnabled()) {
                hint += "\n\n" + getString(R.string.FG_Update_NotificationsOff);
            }
            storageHint.setText(hint);
        }
        if (directUpdate == null || !FGUpdateDownload.isSame(state.update, directUpdate)) return;
        final boolean downloading = state.phase == FGUpdateDownload.Phase.DOWNLOADING;
        final boolean verifying = state.phase == FGUpdateDownload.Phase.VERIFYING;
        final boolean progressVisible = state.downloaded > 0 || downloading || verifying;
        if (progressVisible) {
            progressLine.show();
            progressLine.setProgress(state.total > 0 ? (float) state.downloaded / state.total : 0);
        } else {
            progressLine.hide();
        }
        actionButton.setEnabled(!verifying);
        actionButton.setSubText(null, false);
        if (downloading) {
            actionButton.setText(getString(R.string.FG_Update_Pause), false);
        } else if (verifying) {
            actionButton.setText(getString(R.string.FG_Update_Verifying), false);
        } else if (state.phase == FGUpdateDownload.Phase.READY) {
            actionButton.setText(getString(R.string.FG_Update_Install), false);
        } else if (state.phase == FGUpdateDownload.Phase.PAUSED || state.phase == FGUpdateDownload.Phase.FAILED) {
            actionButton.setText(getString(R.string.FG_Update_Resume), false);
        } else {
            actionButton.setText(downloadButtonText(), false);
        }
        downloadStatus.setVisibility(state.phase == FGUpdateDownload.Phase.IDLE ? View.GONE : View.VISIBLE);
        downloadStatus.setText(FGUpdateBanner.statusText(state, true));
    }

    private void renderDocumentDownload() {
        if (actionButton == null || directUpdate != null || update == null || update.document == null) return;
        final String name = FileLoader.getAttachFileName(update.document);
        if (FileLoader.getInstance(currentAccount).isLoadingFile(name)) {
            Float fraction = org.telegram.messenger.ImageLoader.getInstance().getFileProgress(name);
            progressLine.show();
            progressLine.setProgress(fraction == null ? 0 : fraction);
            actionButton.setSubText(LocaleController.formatString(R.string.FG_Update_Downloading,
                    (int) ((fraction == null ? 0 : fraction) * 100)), false);
        } else {
            progressLine.hide();
            actionButton.setText(downloadButtonText(), false);
        }
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

    /** Значок приложения — тот, что выбран на рабочем столе. */
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
            // слои адаптивного значка рисуются крупнее рамки: маска обрезает края, как в лаунчере
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

    /** Тонкая полоса загрузки над кнопкой. */
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
            if (getVisibility() != VISIBLE) setVisibility(VISIBLE);
        }

        void hide() {
            if (animator != null) animator.cancel();
            setVisibility(GONE);
            shown = 0;
        }

        /** Значение ползёт плавно: иначе на быстрой сети полоса дёргается рывками. */
        void setProgress(float value) {
            final float target = Math.max(0, Math.min(1, value));
            if (animator != null) animator.cancel();
            if (Math.abs(target - shown) < .001f) return;
            if (!isAttachedToWindow() || !SharedConfig.animationsEnabled()
                    || org.telegram.messenger.LiteMode.isPowerSaverApplied()) {
                shown = target;
                invalidate();
                return;
            }
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
        protected void onDetachedFromWindow() {
            if (animator != null) animator.cancel();
            super.onDetachedFromWindow();
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
