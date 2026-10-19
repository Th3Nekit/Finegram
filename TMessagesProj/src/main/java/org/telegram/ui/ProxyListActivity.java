/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.ui;

import static org.telegram.messenger.LocaleController.getString;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.DownloadController;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.ProxyRotationController;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.messenger.browser.Browser;
import org.telegram.proxy.ProxySettings;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.ActionBarMenu;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BackDrawable;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ActionBar.ThemeDescription;
import org.telegram.ui.Cells.TextCell;
import org.telegram.ui.Cells.TextInfoPrivacyCell;
import org.telegram.ui.Components.CheckBox2;
import org.telegram.ui.Components.CubicBezierInterpolator;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.NumberTextView;
import org.telegram.ui.Components.RecyclerListView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.misc.Constants;
import com.th3nekit.finegram.core.ui.ProxySettingsSheet25;
import com.th3nekit.finegram.net.bypass.FGBypassConfig;
import com.th3nekit.finegram.net.bypass.FGBypassController;
import com.th3nekit.finegram.preferences.NetworkPreferencesEntry;
import com.th3nekit.finegram.net.outbound.FGSubscription;
import com.th3nekit.finegram.net.outbound.FGSubscriptionManager;
import com.th3nekit.finegram.net.outbound.FGSubscriptionOrbitCell;
import com.th3nekit.finegram.net.outbound.FGSubscriptionUi;

public class ProxyListActivity extends BaseFragment implements NotificationCenter.NotificationCenterDelegate {
    private final static boolean IS_PROXY_ROTATION_AVAILABLE = true;
    private static final int MENU_DELETE = 0;
    private static final int MENU_SHARE = 1;

    private ListAdapter listAdapter;
    private RecyclerListView listView;
    private boolean rowsUpdatePending;
    private final Runnable deferredRowsUpdate = () -> {
        rowsUpdatePending = false;
        updateRows(true);
    };
    @SuppressWarnings("FieldCanBeLocal")
    private LinearLayoutManager layoutManager;

    private int currentConnectionState;

    private boolean useProxySettings;

    private int rowCount;
    private int routeRow;
    private int panelHeaderRow;
    private int panelBottomRow;
    @Keep
    private int useProxyRow;
    private int useProxyShadowRow;
    private int connectionsHeaderRow;
    private int proxyStartRow;
    private int proxyEndRow;
    @Keep
    private int proxyAddRow;

    private int fgOutboundAddRow;
    private int subscriptionHeaderRow;
    private int subscriptionStartRow;
    private int subscriptionEndRow;
    private int subscriptionAddRow;
    private List<FGSubscription> subscriptions = Collections.emptyList();
    private final Set<String> expandedSubscriptions = new HashSet<>();
    private final Set<String> busySubscriptions = new HashSet<>();
    private final Map<String, Integer> subscriptionServerOffset = new HashMap<>();
    private int proxyShadowRow;
    private int rotationRow;
    private int rotationTimeoutRow;
    private int rotationTimeoutInfoRow;
    private int deleteAllRow;

    private ItemTouchHelper itemTouchHelper;
    private NumberTextView selectedCountTextView;
    private ActionBarMenuItem shareMenuItem;
    private ActionBarMenuItem deleteMenuItem;

    private List<SharedConfig.ProxyInfo> selectedItems = new ArrayList<>();
    private List<SharedConfig.ProxyInfo> proxyList = new ArrayList<>();
    private boolean wasCheckedAllList;

    public class TextDetailProxyCell extends FrameLayout {

        private TextView textView;
        private TextView valueTextView;
        private ImageView checkImageView;
        private SharedConfig.ProxyInfo currentInfo;
        private Drawable checkDrawable;

        private CheckBox2 checkBox;
        private boolean isSelected;
        private boolean isSelectionEnabled;
        private ValueAnimator selectionAnimator;

        private int color;
        private final Paint surfacePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF surfaceBounds = new RectF();

        public TextDetailProxyCell(Context context) {
            super(context);

            textView = new TextView(context);
            textView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            textView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            textView.setTypeface(AndroidUtilities.bold());
            textView.setIncludeFontPadding(false);
            textView.setTextDirection(View.TEXT_DIRECTION_FIRST_STRONG);
            textView.setBreakStrategy(android.text.Layout.BREAK_STRATEGY_HIGH_QUALITY);
            textView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            textView.setGravity((LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL);
            addView(textView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP, (LocaleController.isRTL ? 56 : 21), 10, (LocaleController.isRTL ? 21 : 56), 0));

            valueTextView = new TextView(context);
            valueTextView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            valueTextView.setGravity(LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT);
            valueTextView.setIncludeFontPadding(false);
            valueTextView.setTextDirection(View.TEXT_DIRECTION_FIRST_STRONG);
            valueTextView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            valueTextView.setCompoundDrawablePadding(AndroidUtilities.dp(6));
            valueTextView.setPadding(0, 0, 0, 0);
            addView(valueTextView, LayoutHelper.createFrame(LayoutHelper.WRAP_CONTENT, LayoutHelper.WRAP_CONTENT, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.TOP, (LocaleController.isRTL ? 56 : 21), 35, (LocaleController.isRTL ? 21 : 56), 0));

            checkImageView = new ImageView(context);
            checkImageView.setImageDrawable(new ProxySettingsSheet25.Glyph(ProxySettingsSheet25.Glyph.EDIT));
            checkImageView.setColorFilter(new PorterDuffColorFilter(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText3), PorterDuff.Mode.MULTIPLY));
            checkImageView.setScaleType(ImageView.ScaleType.CENTER);
            checkImageView.setBackground(Theme.createSelectorDrawable(Theme.getColor(Theme.key_listSelector), 1));
            checkImageView.setContentDescription(getString(R.string.Edit));
            addView(checkImageView, LayoutHelper.createFrame(48, 48, (LocaleController.isRTL ? Gravity.LEFT : Gravity.RIGHT) | Gravity.TOP, 8, 8, 8, 0));
            checkImageView.setOnClickListener(v -> {
                if (isOwnBypass(currentInfo)) {
                    presentFragment(new NetworkPreferencesEntry());
                } else if (currentInfo.settings.getType() == ProxySettings.Type.OUTBOUND) {
                    com.th3nekit.finegram.net.outbound.FGOutboundUi.editName(ProxyListActivity.this, currentInfo);
                } else {
                    presentFragment(new ProxySettingsActivity(currentInfo));
                }
            });

            checkBox = new CheckBox2(context, 21);
            checkBox.setColor(Theme.key_checkbox, Theme.key_radioBackground, Theme.key_checkboxCheck);
            checkBox.setDrawBackgroundAsArc(14);
            checkBox.setVisibility(GONE);
            checkBox.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            addView(checkBox, LayoutHelper.createFrame(24, 24, (LocaleController.isRTL ? Gravity.RIGHT : Gravity.LEFT) | Gravity.CENTER_VERTICAL, 16, 0, 8, 0));

            setWillNotDraw(false);
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            int textWidth = Math.max(0, width - AndroidUtilities.dp(isSelectionEnabled ? 120 : 88));
            int textSpec = MeasureSpec.makeMeasureSpec(textWidth, MeasureSpec.EXACTLY);
            int wrapSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
            textView.measure(textSpec, wrapSpec);
            valueTextView.measure(textSpec, wrapSpec);
            checkImageView.measure(MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(48), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(48), MeasureSpec.EXACTLY));
            checkBox.measure(MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(24), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(AndroidUtilities.dp(24), MeasureSpec.EXACTLY));
            setMeasuredDimension(width, Math.max(AndroidUtilities.dp(76), textView.getMeasuredHeight() + valueTextView.getMeasuredHeight() + AndroidUtilities.dp(32)));
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            int width = right - left;
            int height = bottom - top;
            int leading = AndroidUtilities.dp(isSelectionEnabled ? 56 : 24);
            int textLeft = LocaleController.isRTL ? width - leading - textView.getMeasuredWidth() : leading;
            int textTop = (height - textView.getMeasuredHeight() - valueTextView.getMeasuredHeight() - AndroidUtilities.dp(6)) / 2;
            textView.layout(textLeft, textTop, textLeft + textView.getMeasuredWidth(), textTop + textView.getMeasuredHeight());
            int valueTop = textTop + textView.getMeasuredHeight() + AndroidUtilities.dp(6);
            valueTextView.layout(textLeft, valueTop, textLeft + valueTextView.getMeasuredWidth(), valueTop + valueTextView.getMeasuredHeight());
            int editLeft = LocaleController.isRTL ? AndroidUtilities.dp(16) : width - AndroidUtilities.dp(64);
            int editTop = (height - AndroidUtilities.dp(48)) / 2;
            checkImageView.layout(editLeft, editTop, editLeft + AndroidUtilities.dp(48), editTop + AndroidUtilities.dp(48));
            int checkLeft = LocaleController.isRTL ? width - AndroidUtilities.dp(48) : AndroidUtilities.dp(24);
            int checkTop = (height - AndroidUtilities.dp(24)) / 2;
            checkBox.layout(checkLeft, checkTop, checkLeft + AndroidUtilities.dp(24), checkTop + AndroidUtilities.dp(24));
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            info.setCheckable(true);
            info.setChecked(isSelectionEnabled ? isSelected : currentInfo == SharedConfig.currentProxy);
            info.setText(textView.getText() + ". " + valueTextView.getText());
        }

        public void setProxy(SharedConfig.ProxyInfo proxyInfo) {

            textView.setText(com.exteragram.messenger.proxy.ProxyController.getDisplayName(proxyInfo));
            checkImageView.setContentDescription(getString(isOwnBypass(proxyInfo) ? R.string.Settings : R.string.Edit) + ": " + textView.getText());
            currentInfo = proxyInfo;
            updateStatus();
        }

        public void updateStatus() {
            if (currentInfo == null) {
                return;
            }
            textView.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlackText));
            checkImageView.setColorFilter(new PorterDuffColorFilter(Theme.getColor(Theme.key_windowBackgroundWhiteGrayText3), PorterDuff.Mode.MULTIPLY));
            Theme.setSelectorDrawableColor(checkImageView.getBackground(), Theme.getColor(Theme.key_listSelector), true);
            int colorKey;
            if (SharedConfig.currentProxy == currentInfo && useProxySettings) {
                if (currentConnectionState == ConnectionsManager.ConnectionStateConnected || currentConnectionState == ConnectionsManager.ConnectionStateUpdating) {
                    colorKey = Theme.key_windowBackgroundWhiteBlueText6;
                    if (hasMeasuredPing(currentInfo)) {
                        valueTextView.setText(getString(R.string.Connected) + ", " + LocaleController.formatString("Ping", R.string.Ping, currentInfo.ping));
                    } else {
                        valueTextView.setText(getString(R.string.Connected));
                    }
                    if (!currentInfo.checking && !currentInfo.available) {
                        currentInfo.availableCheckTime = 0;
                    }
                } else {
                    colorKey = Theme.key_windowBackgroundWhiteGrayText2;
                    valueTextView.setText(getString(currentConnectionState == ConnectionsManager.ConnectionStateWaitingForNetwork ? R.string.WaitingForNetwork : R.string.Connecting));
                }
            } else {
                if (currentInfo.checking) {
                    valueTextView.setText(getString(R.string.Checking));
                    colorKey = Theme.key_windowBackgroundWhiteGrayText2;
                } else if (currentInfo.availableCheckTime == 0) {
                    valueTextView.setText(null);
                    colorKey = Theme.key_windowBackgroundWhiteGrayText2;
                } else if (currentInfo.available) {
                    if (hasMeasuredPing(currentInfo)) {
                        valueTextView.setText(getString(R.string.Available) + ", " + LocaleController.formatString("Ping", R.string.Ping, currentInfo.ping));
                    } else {
                        valueTextView.setText(getString(R.string.Available));
                    }
                    colorKey = Theme.key_windowBackgroundWhiteGreenText;
                } else {
                    valueTextView.setText(getString(R.string.Unavailable));
                    colorKey = Theme.key_text_RedRegular;
                }
            }
            color = Theme.getColor(colorKey);
            valueTextView.setTag(colorKey);
            valueTextView.setTextColor(color);
            if (checkDrawable != null) {
                checkDrawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.MULTIPLY));
            }
        }

        public void setSelectionEnabled(boolean enabled, boolean animated) {
            if (selectionAnimator != null) {
                selectionAnimator.cancel();
                selectionAnimator = null;
            }
            isSelectionEnabled = enabled;
            checkImageView.setEnabled(!enabled);
            requestLayout();
            checkBox.setVisibility(enabled || animated ? VISIBLE : GONE);
            checkImageView.setVisibility(!enabled || animated ? VISIBLE : GONE);
            if (!animated) {
                checkBox.setAlpha(enabled ? 1f : 0f);
                checkImageView.setAlpha(enabled ? 0f : 1f);
            } else {
                selectionAnimator = ValueAnimator.ofFloat(checkBox.getAlpha(), enabled ? 1f : 0f);
                selectionAnimator.setDuration(150);
                selectionAnimator.setInterpolator(CubicBezierInterpolator.DEFAULT);
                selectionAnimator.addUpdateListener(animation -> {
                    float value = (float) animation.getAnimatedValue();
                    checkBox.setAlpha(value);
                    checkImageView.setAlpha(1f - value);
                });
                selectionAnimator.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        checkBox.setVisibility(isSelectionEnabled ? VISIBLE : GONE);
                        checkImageView.setVisibility(isSelectionEnabled ? GONE : VISIBLE);
                    }
                });
                selectionAnimator.start();
            }
            invalidate();
        }

        public void setItemSelected(boolean selected, boolean animated) {
            if (selected == isSelected && animated) {
                return;
            }
            isSelected = selected;
            checkBox.setChecked(selected, animated);
        }

        public void setChecked(boolean checked) {
            if (checked) {
                if (checkDrawable == null) {
                    checkDrawable = getResources().getDrawable(R.drawable.proxy_check).mutate();
                }
                if (checkDrawable != null) {
                    int checkSize = Math.min(AndroidUtilities.dp(11), Math.round(valueTextView.getTextSize() * .72f));
                    checkDrawable.setBounds(0, 0, checkSize, checkSize);
                    checkDrawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.MULTIPLY));
                }
                if (LocaleController.isRTL) {
                    valueTextView.setCompoundDrawables(null, null, checkDrawable, null);
                } else {
                    valueTextView.setCompoundDrawables(checkDrawable, null, null, null);
                }
            } else {
                valueTextView.setCompoundDrawables(null, null, null, null);
            }
        }

        public void setValue(CharSequence value) {
            valueTextView.setText(value);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            updateStatus();
        }

        @Override
        protected void onDetachedFromWindow() {
            if (selectionAnimator != null) {
                selectionAnimator.cancel();
                selectionAnimator = null;
            }
            super.onDetachedFromWindow();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            if (currentInfo != null && currentInfo == SharedConfig.currentProxy && useProxySettings && !isSelectionEnabled) {
                surfacePaint.setColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlueText6));
                surfacePaint.setAlpha(Theme.isCurrentThemeDark() ? 24 : 14);
                surfaceBounds.set(AndroidUtilities.dp(8), AndroidUtilities.dp(4), getMeasuredWidth() - AndroidUtilities.dp(8), getMeasuredHeight() - AndroidUtilities.dp(4));
                canvas.drawRoundRect(surfaceBounds, AndroidUtilities.dp(16), AndroidUtilities.dp(16), surfacePaint);
                surfacePaint.setAlpha(190);
                float marker = LocaleController.isRTL ? getMeasuredWidth() - AndroidUtilities.dp(13) : AndroidUtilities.dp(13);
                surfaceBounds.set(marker, AndroidUtilities.dp(20), marker + AndroidUtilities.dp(2), getMeasuredHeight() - AndroidUtilities.dp(20));
                canvas.drawRoundRect(surfaceBounds, AndroidUtilities.dp(1), AndroidUtilities.dp(1), surfacePaint);
            }
            canvas.drawLine(AndroidUtilities.dp(24), getMeasuredHeight() - 1, getMeasuredWidth() - AndroidUtilities.dp(24), getMeasuredHeight() - 1, Theme.dividerPaint);
        }
    }

    @Override
    public boolean onFragmentCreate() {
        super.onFragmentCreate();

        SharedConfig.loadProxyList();
        currentConnectionState = ConnectionsManager.getInstance(currentAccount).getConnectionState();

        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxyChangedByRotation);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxyCheckDone);
        NotificationCenter.getInstance(currentAccount).addObserver(this, NotificationCenter.didUpdateConnectionState);

        final SharedPreferences preferences = MessagesController.getGlobalMainSettings();
        useProxySettings = preferences.getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();

        updateRows(true);

        if (com.th3nekit.finegram.net.outbound.FGOutboundUi.isAvailable()) {
            FGSubscriptionManager.getInstance().load(result -> {
                if (isFinished) return;
                updateRows(true);
                if (!result.success && fragmentView != null) FGSubscriptionUi.report(this, result);
            });
        }

        return true;
    }

    @Override
    public void onFragmentDestroy() {
        AndroidUtilities.cancelRunOnUIThread(deferredRowsUpdate);
        rowsUpdatePending = false;
        super.onFragmentDestroy();
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxyChangedByRotation);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxyCheckDone);
        NotificationCenter.getInstance(currentAccount).removeObserver(this, NotificationCenter.didUpdateConnectionState);
    }

    @Override
    public View createView(Context context) {
        BackDrawable backDrawable = new BackDrawable(false);
        backDrawable.setShowStick(!FinegramAppearanceConfig.INSTANCE.getCenterTitle());
        actionBar.setBackButtonDrawable(backDrawable);
        actionBar.setAllowOverlayTitle(true);
        actionBar.setTitle(getString(R.string.ProxySettings));
        if (parentLayout != null && parentLayout.isLayersLayout()) {
            actionBar.setOccupyStatusBar(false);
        }
        actionBar.setAllowOverlayTitle(false);
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                if (id == -1) {
                    finishFragment();
                }
            }
        });

        listAdapter = new ListAdapter(context);

        fragmentView = new FrameLayout(context);
        fragmentView.setBackgroundColor(Theme.getColor(Theme.key_windowBackgroundGray));
        FrameLayout frameLayout = (FrameLayout) fragmentView;

        listView = new RecyclerListView(context);
        listView.setSelectorRadius(AndroidUtilities.dp(16));
        listView.addItemDecoration(new RecyclerView.ItemDecoration() {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final RectF bounds = new RectF();

            @Override
            public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
                float top = -AndroidUtilities.dp(32);
                boolean hasPanel = false;
                for (int i = 0; i < parent.getChildCount(); i++) {
                    View child = parent.getChildAt(i);
                    int position = parent.getChildAdapterPosition(child);
                    if (position == panelHeaderRow) {
                        top = child.getTop() + child.getTranslationY();
                        hasPanel = true;
                        break;
                    }
                    if (position > panelHeaderRow) {
                        hasPanel = true;
                    }
                }
                if (hasPanel) {
                    paint.setColor(Theme.getColor(Theme.key_windowBackgroundWhite));
                    bounds.set(0, top, parent.getWidth(), Math.max(parent.getHeight(), top) + AndroidUtilities.dp(32));
                    canvas.drawRoundRect(bounds, AndroidUtilities.dp(32), AndroidUtilities.dp(32), paint);
                }
            }
        });
        actionBar.setAdaptiveBackground(listView);
        ((DefaultItemAnimator) listView.getItemAnimator()).setDelayAnimations(false);
        ((DefaultItemAnimator) listView.getItemAnimator()).setTranslationInterpolator(CubicBezierInterpolator.DEFAULT);
        listView.setVerticalScrollBarEnabled(false);
        listView.setLayoutManager(layoutManager = new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        frameLayout.addView(listView, LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.MATCH_PARENT, Gravity.TOP | Gravity.LEFT));
        listView.setAdapter(listAdapter);
        listView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
                if (newState == RecyclerView.SCROLL_STATE_IDLE && rowsUpdatePending) {
                    AndroidUtilities.cancelRunOnUIThread(deferredRowsUpdate);
                    AndroidUtilities.runOnUIThread(deferredRowsUpdate);
                }
            }
        });
        listView.setOnItemClickListener((view, position) -> {
            if (position == rotationRow) {
                SharedConfig.proxyRotationEnabled = !SharedConfig.proxyRotationEnabled;
                ProxySettingsSheet25.ToggleCell textCheckCell = (ProxySettingsSheet25.ToggleCell) view;
                textCheckCell.setChecked(SharedConfig.proxyRotationEnabled, true);
                SharedConfig.saveConfig();

                updateRows(true);
            } else if (position >= proxyStartRow && position < proxyEndRow) {
                SharedConfig.ProxyInfo info = proxyList.get(position - proxyStartRow);
                if (isOwnBypass(info)) {
                    presentFragment(new NetworkPreferencesEntry());
                    return;
                }
                if (!selectedItems.isEmpty()) {
                    listAdapter.toggleSelected(position);
                    return;
                }

                selectProxy(info);
            } else if (position == rotationTimeoutRow) {
                ArrayList<Integer> options = new ArrayList<>(ProxyRotationController.ROTATION_TIMEOUTS);
                String[] values = new String[options.size()];
                for (int i = 0; i < options.size(); i++) {
                    values[i] = LocaleController.formatString(R.string.ProxyRotationTimeoutSeconds, options.get(i));
                }
                AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
                builder.setTitle(getString(R.string.UseProxyRotation));
                builder.setItems(values, (dialog, which) -> {
                    SharedConfig.proxyRotationTimeout = which;
                    SharedConfig.saveConfig();
                    listAdapter.notifyItemChanged(rotationTimeoutRow);
                });
                showDialog(builder.create());
            } else if (position == proxyAddRow) {
                presentFragment(new ProxySettingsActivity());
            } else if (position == fgOutboundAddRow) {
                com.th3nekit.finegram.net.outbound.FGOutboundUi.askForLink(this);
            } else if (position == subscriptionAddRow) {
                FGSubscriptionUi.add(this, this::subscriptionsChanged);
            } else if (position == deleteAllRow) {
                AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
                builder.setMessage(subscriptions.isEmpty() ? getString(R.string.DeleteAllProxiesConfirm) : FGSubscriptionUi.text("FG_Subscription_DeleteManualInfo"));
                builder.setNegativeButton(getString(R.string.Cancel), null);
                builder.setTitle(getString(R.string.DeleteProxyTitle));
                builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
                    for (SharedConfig.ProxyInfo info : proxyList) {
                        SharedConfig.deleteProxy(info);
                    }
                    useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
                    NotificationCenter.getGlobalInstance().removeObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);
                    NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
                    NotificationCenter.getGlobalInstance().addObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);
                    updateRows(true);
                    if (listAdapter != null) {
                        listAdapter.notifyItemChanged(useProxyRow, ListAdapter.PAYLOAD_CHECKED_CHANGED);
                        listAdapter.clearSelected();
                    }
                });
                AlertDialog dialog = builder.create();
                showDialog(dialog);
                TextView button = (TextView) dialog.getButton(DialogInterface.BUTTON_POSITIVE);
                if (button != null) {
                    button.setTextColor(Theme.getColor(Theme.key_text_RedBold));
                }
            }
        });
        listView.setOnItemLongClickListener((view, position) -> {
            if (position >= proxyStartRow && position < proxyEndRow) {
                listAdapter.toggleSelected(position);
                return true;
            }
            return false;
        });

        ActionBarMenu actionMode = actionBar.createActionMode();
        selectedCountTextView = new NumberTextView(actionMode.getContext());
        selectedCountTextView.setTextSize(18);
        selectedCountTextView.setTypeface(AndroidUtilities.bold());
        selectedCountTextView.setTextColor(Theme.getColor(Theme.key_actionBarActionModeDefaultIcon));
        actionMode.addView(selectedCountTextView, LayoutHelper.createLinear(0, LayoutHelper.MATCH_PARENT, 1.0f, 72, 0, 0, 0));
        selectedCountTextView.setOnTouchListener((v, event) -> true);

        shareMenuItem = actionMode.addItemWithWidth(MENU_SHARE, R.drawable.msg_share, AndroidUtilities.dp(54));
        shareMenuItem.setContentDescription(getString(R.string.StickersShare));
        deleteMenuItem = actionMode.addItemWithWidth(MENU_DELETE, R.drawable.msg_delete, AndroidUtilities.dp(54));
        deleteMenuItem.setContentDescription(getString(R.string.Delete));

        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override
            public void onItemClick(int id) {
                switch (id) {
                    case -1:
                        if (selectedItems.isEmpty()) {
                            finishFragment();
                        } else {
                            listAdapter.clearSelected();
                        }
                        break;
                    case MENU_DELETE:
                        AlertDialog.Builder builder = new AlertDialog.Builder(getParentActivity());
                        builder.setMessage(getString(selectedItems.size() > 1 ? R.string.DeleteProxyMultiConfirm : R.string.DeleteProxyConfirm));
                        builder.setNegativeButton(getString(R.string.Cancel), null);
                        builder.setTitle(getString(R.string.DeleteProxyTitle));
                        builder.setPositiveButton(getString(R.string.Delete), (dialog, which) -> {
                            for (SharedConfig.ProxyInfo info : selectedItems) {
                                SharedConfig.deleteProxy(info);
                            }
                            if (SharedConfig.currentProxy == null) {
                                useProxySettings = false;
                            }
                            NotificationCenter.getGlobalInstance().removeObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);
                            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
                            NotificationCenter.getGlobalInstance().addObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);
                            updateRows(true);
                            if (listAdapter != null) {
                                if (SharedConfig.currentProxy == null) {
                                    listAdapter.notifyItemChanged(useProxyRow, ListAdapter.PAYLOAD_CHECKED_CHANGED);
                                }
                                listAdapter.clearSelected();
                            }
                        });
                        AlertDialog dialog = builder.create();
                        showDialog(dialog);
                        TextView button = (TextView) dialog.getButton(DialogInterface.BUTTON_POSITIVE);
                        if (button != null) {
                            button.setTextColor(Theme.getColor(Theme.key_text_RedBold));
                        }
                        break;
                    case MENU_SHARE:
                        if (selectedItems.isEmpty()) break;
                        boolean canShare = true;
                        for (SharedConfig.ProxyInfo info : selectedItems) {
                            if (!com.exteragram.messenger.proxy.ProxyController.canShareProxy(info)) {
                                canShare = false;
                                break;
                            }
                        }
                        if (!canShare) break;
                        StringBuilder links = new StringBuilder();
                        for (SharedConfig.ProxyInfo info : selectedItems) {
                            if (links.length() > 0) {
                                links.append("\n\n");
                            }
                            links.append(info.settings.getLink());
                        }

                        Intent shareIntent = new Intent(Intent.ACTION_SEND);
                        shareIntent.setType("text/plain");
                        shareIntent.putExtra(Intent.EXTRA_TEXT, links.toString());
                        Intent chooserIntent = Intent.createChooser(shareIntent, getString(selectedItems.size() > 1 ? R.string.ShareLinks : R.string.ShareLink));
                        chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        context.startActivity(chooserIntent);

                        if (listAdapter != null) {
                            listAdapter.clearSelected();
                        }
                        break;
                }
            }
        });

        return fragmentView;
    }

    @Override
    public boolean onBackPressed(boolean invoked) {
        if (!selectedItems.isEmpty()) {
            if (invoked) listAdapter.clearSelected();
            return false;
        }
        return super.onBackPressed(invoked);
    }

    private void updateRows(boolean notify) {
        if (isFinished) return;
        if (notify && listView != null && (listView.isComputingLayout()
                || listView.getScrollState() != RecyclerView.SCROLL_STATE_IDLE)) {
            if (!rowsUpdatePending) {
                rowsUpdatePending = true;
                if (listView.getScrollState() == RecyclerView.SCROLL_STATE_IDLE) {
                    AndroidUtilities.runOnUIThread(deferredRowsUpdate, 16);
                }
            }
            return;
        }
        if (notify && rowsUpdatePending) {
            AndroidUtilities.cancelRunOnUIThread(deferredRowsUpdate);
            rowsUpdatePending = false;
        }
        subscriptions = com.th3nekit.finegram.net.outbound.FGOutboundUi.isAvailable()
                ? FGSubscriptionManager.getInstance().list() : Collections.emptyList();
        rowCount = 0;
        safeSurfRow = -1;
        safeSurfShadowRow = -1;
        routeRow = rowCount++;
        panelHeaderRow = rowCount++;
        useProxyRow = rowCount++;
        connectionsHeaderRow = rowCount++;
        useProxyShadowRow = -1;

        if (notify) {
            proxyList.clear();
            for (SharedConfig.ProxyInfo info : SharedConfig.proxyList) {
                if (!FGSubscriptionManager.getInstance().isManaged(info)) proxyList.add(info);
            }

            boolean checking = false;
            if (!wasCheckedAllList) {
                for (SharedConfig.ProxyInfo info : proxyList) {
                    if (info.checking || info.availableCheckTime == 0) {
                        checking = true;
                        break;
                    }
                }
                if (!checking) {
                    wasCheckedAllList = true;
                }
            }

            boolean isChecking = checking;
            Collections.sort(proxyList, (o1, o2) -> {
                long bias1 = SharedConfig.currentProxy == o1 ? -200000 : 0;
                if (!o1.available) {
                    bias1 += 100000;
                }
                long bias2 = SharedConfig.currentProxy == o2 ? -200000 : 0;
                if (!o2.available) {
                    bias2 += 100000;
                }
                return Long.compare(isChecking && o1 != SharedConfig.currentProxy ? SharedConfig.proxyList.indexOf(o1) * 10000L : o1.ping + bias1,
                        isChecking && o2 != SharedConfig.currentProxy ? SharedConfig.proxyList.indexOf(o2) * 10000L : o2.ping + bias2);
            });
        }

        if (!proxyList.isEmpty()) {
            proxyStartRow = rowCount;
            rowCount += proxyList.size();
            proxyEndRow = rowCount;
        } else {
            proxyStartRow = -1;
            proxyEndRow = -1;
        }
        if (useProxySettings && SharedConfig.currentProxy != null && SharedConfig.proxyList.size() > 1 && IS_PROXY_ROTATION_AVAILABLE) {
            rotationRow = rowCount++;
            if (SharedConfig.proxyRotationEnabled) {
                rotationTimeoutRow = rowCount++;
                rotationTimeoutInfoRow = rowCount++;
            } else {
                rotationTimeoutRow = -1;
                rotationTimeoutInfoRow = -1;
            }
        } else {
            rotationRow = -1;
            rotationTimeoutRow = -1;
            rotationTimeoutInfoRow = -1;
        }
        proxyShadowRow = rowCount++;
        subscriptionHeaderRow = subscriptions.isEmpty() ? -1 : rowCount++;
        subscriptionStartRow = rowCount;
        rowCount += subscriptions.size();
        subscriptionEndRow = rowCount;
        subscriptionAddRow = com.th3nekit.finegram.net.outbound.FGOutboundUi.isAvailable() ? rowCount++ : -1;
        proxyAddRow = rowCount++;

        fgOutboundAddRow = com.th3nekit.finegram.net.outbound.FGOutboundUi.isAvailable()
                ? rowCount++ : -1;
        if (proxyList.size() >= 10) {
            deleteAllRow = rowCount++;
        } else {
            deleteAllRow = -1;
        }
        panelBottomRow = rowCount++;
        checkProxyList();
        if (notify && listAdapter != null) {
            listAdapter.notifyDataSetChanged();
        }
    }

    private void checkProxyList() {
        List<SharedConfig.ProxyInfo> all = new ArrayList<>(SharedConfig.proxyList);
        for (SharedConfig.ProxyInfo proxyInfo : all) {
            if (proxyInfo.checking || (proxyInfo.availableCheckTime != 0
                    && SystemClock.elapsedRealtime() - proxyInfo.availableCheckTime < 2 * 60 * 1000)) {
                continue;
            }
            proxyInfo.checking = true;
            final ProxySettings settings = proxyInfo.settings;
            final long generation = ++proxyInfo.proxyCheckGeneration;
            ConnectionsManager.getInstance(currentAccount).checkProxy(settings, time -> AndroidUtilities.runOnUIThread(() -> {
                if (proxyInfo.settings != settings || proxyInfo.proxyCheckGeneration != generation) return;
                proxyInfo.availableCheckTime = SystemClock.elapsedRealtime();
                proxyInfo.checking = false;
                if (time == -1) {
                    proxyInfo.available = false;
                    proxyInfo.ping = 0;
                } else {
                    proxyInfo.ping = time;
                    proxyInfo.available = true;
                }
                NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxyCheckDone, proxyInfo);
            }));
        }
    }

    private void toggleProxy() {
        if (isFinished) return;
        if (SharedConfig.currentProxy == null) {
            if (!SharedConfig.proxyList.isEmpty()) {
                SharedConfig.currentProxy = SharedConfig.proxyList.get(0);

                if (!useProxySettings) {
                    SharedPreferences.Editor editor = MessagesController.getGlobalMainSettings().edit();
                    SharedConfig.currentProxy.settings.toSharedPreferences(editor);
                    editor.commit();
                }
            } else {
                presentFragment(new ProxySettingsActivity());
                return;
            }
        }
        if (isOwnBypass(SharedConfig.currentProxy)) {
            setBypassEnabled(!useProxySettings);
            return;
        }
        useProxySettings = !useProxySettings;
        updateRows(true);

        SharedPreferences.Editor editor = MessagesController.getGlobalMainSettings().edit();
        editor.putBoolean("proxy_enabled", useProxySettings);
        editor.apply();

        ConnectionsManager.setProxySettings(useProxySettings, SharedConfig.currentProxy.settings);
        NotificationCenter.getGlobalInstance().removeObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().addObserver(ProxyListActivity.this, NotificationCenter.proxySettingsChanged);

        updateProxyStatus(null);
    }

    private void bindToggles(ProxySettingsSheet25.TogglePairCell cell) {
        cell.bind(useProxySettings, FGBypassConfig.enabled, this::toggleProxy, () -> {
            if (!isFinished) setBypassEnabled(!FGBypassConfig.enabled);
        });
    }

    private void subscriptionsChanged() {
        if (!isFinished) {
            useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
            updateRows(true);
        }
    }

    private void refreshSubscription(FGSubscription source) {
        if (!busySubscriptions.add(source.id)) return;
        updateRows(true);
        FGSubscriptionManager.getInstance().refresh(source.id, result -> {
            busySubscriptions.remove(source.id);
            subscriptionsChanged();
            FGSubscriptionUi.report(this, result);
        });
    }

    private void selectProxy(SharedConfig.ProxyInfo info) {
        if (isFinished || !SharedConfig.proxyList.contains(info)) return;
        if (isOwnBypass(info)) {
            presentFragment(new NetworkPreferencesEntry());
            return;
        }
        if (!selectedItems.isEmpty()) listAdapter.clearSelected();
        if (FGBypassConfig.enabled) FGBypassController.getInstance().setEnabled(false);
        useProxySettings = true;
        SharedPreferences.Editor editor = MessagesController.getGlobalMainSettings().edit();
        info.settings.toSharedPreferences(editor);
        editor.putBoolean("proxy_enabled", true);
        editor.apply();
        SharedConfig.currentProxy = info;
        ConnectionsManager.setProxySettings(true, info.settings);
        updateRows(true);
        updateProxyStatus(null);
        presentFragment(new com.th3nekit.finegram.core.ui.proxy.ProxyConnectionActivity(currentAccount, info));
    }

    private void bindSubscription(FGSubscriptionOrbitCell cell, FGSubscription source) {
        boolean expanded = expandedSubscriptions.contains(source.id);
        cell.bind(source, expanded, busySubscriptions.contains(source.id), () -> refreshSubscription(source),
                () -> FGSubscriptionUi.settings(this, source, this::subscriptionsChanged), () -> {
                    if (!expandedSubscriptions.remove(source.id)) expandedSubscriptions.add(source.id);
                    updateRows(true);
                });
        List<View> views = new ArrayList<>();
        List<SharedConfig.ProxyInfo> servers = expanded ? FGSubscriptionManager.getInstance().servers(source.id) : Collections.emptyList();
        int offset = subscriptionServerOffset.getOrDefault(source.id, 0);
        int start = offset < servers.size() ? offset : 0;
        int limit = Math.min(servers.size(), start + 8);
        for (int i = start; i < limit; i++) {
            SharedConfig.ProxyInfo server = servers.get(i);
            TextDetailProxyCell row = new TextDetailProxyCell(cell.getContext());
            row.setProxy(server);
            row.setChecked(server == SharedConfig.currentProxy);
            row.setOnClickListener(v -> selectProxy(server));
            views.add(row);
        }
        if (start > 0) {
            TextView previous = new TextView(cell.getContext());
            previous.setText(FGSubscriptionUi.text("FG_Subscription_PreviousServers"));
            previous.setTextSize(14);
            previous.setTextColor(Theme.getColor(Theme.key_windowBackgroundWhiteBlueText6));
            previous.setGravity(Gravity.CENTER);
            previous.setMinHeight(AndroidUtilities.dp(48));
            previous.setBackground(Theme.createSelectorDrawable(Theme.getColor(Theme.key_listSelector), 2));
            previous.setOnClickListener(v -> {
                subscriptionServerOffset.put(source.id, Math.max(0, start - 8));
                updateRows(true);
            });
            views.add(previous);
        }
        cell.setServerViews(views, limit < servers.size(), () -> {
            subscriptionServerOffset.put(source.id, limit);
            updateRows(true);
        });
    }

    private void setBypassEnabled(boolean enabled) {
        FGBypassController.getInstance().setEnabled(enabled);
        useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
        updateRows(true);
    }

    private static boolean isOwnBypass(SharedConfig.ProxyInfo proxy) {
        return proxy != null
                && com.th3nekit.finegram.net.bypass.WsBypassCore.LOCAL_PROXY_HOST.equals(proxy.settings.getAddress())
                && proxy.settings.getPort() == com.th3nekit.finegram.net.bypass.FGBypassConfig.localPort;
    }

    @Override
    protected void onDialogDismiss(Dialog dialog) {
        DownloadController.getInstance(currentAccount).checkAutodownloadSettings();
    }

    @Override
    public void onResume() {
        super.onResume();
        useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
        currentConnectionState = ConnectionsManager.getInstance(currentAccount).getConnectionState();
        updateRows(true);
    }

    public void appendNavigationDiagnostics(StringBuilder state) {
        if (listView != null) {
            state.append("{rows=").append(rowCount)
                    .append(" list=").append(listView.getWidth()).append('x').append(listView.getHeight())
                    .append(" children=").append(listView.getChildCount())
                    .append(" layout=").append(listView.isComputingLayout())
                    .append(" suppressed=").append(listView.isLayoutSuppressed())
                    .append(" scroll=").append(listView.getScrollState())
                    .append(" canScroll=").append(listView.canScrollVertically(-1)).append('/').append(listView.canScrollVertically(1))
                    .append('}');
        }
    }

    private static boolean hasMeasuredPing(SharedConfig.ProxyInfo info) {
        return info != null && !info.checking && info.available && info.availableCheckTime != 0 && info.ping >= 0;
    }

    private void bindRoute(ProxySettingsSheet25.RouteHeader header) {
        SharedConfig.ProxyInfo info = SharedConfig.currentProxy;
        boolean connected = currentConnectionState == ConnectionsManager.ConnectionStateConnected || currentConnectionState == ConnectionsManager.ConnectionStateUpdating;
        boolean enabled = useProxySettings && info != null;
        CharSequence title = info == null ? getString(R.string.ProxyConnections) : com.exteragram.messenger.proxy.ProxyController.getDisplayName(info);
        String status = getString(currentConnectionState == ConnectionsManager.ConnectionStateWaitingForNetwork ? R.string.WaitingForNetwork : connected ? R.string.FG_ProxyOnline : R.string.Connecting);
        if (!enabled) {
            status = getString(R.string.MenuProxyDisabled) + " · " + status;
        }
        int colorKey = connected ? Theme.key_windowBackgroundWhiteBlueText6 : Theme.key_windowBackgroundWhiteGrayText2;
        header.bind(title, status, colorKey, enabled, connected);
    }

    private void bindPanel(ProxySettingsSheet25.PanelHeader header) {
        SharedConfig.ProxyInfo info = SharedConfig.currentProxy;
        header.bind(useProxySettings && hasMeasuredPing(info) ? LocaleController.formatString("Ping", R.string.Ping, info.ping) : null);
    }

    private void updateProxyTheme() {
        if (listView == null) {
            return;
        }
        for (int i = 0; i < listView.getChildCount(); i++) {
            View child = listView.getChildAt(i);
            if (child instanceof ProxySettingsSheet25.Themed) {
                ((ProxySettingsSheet25.Themed) child).updateColors();
            } else if (child instanceof TextDetailProxyCell) {
                ((TextDetailProxyCell) child).updateStatus();
                child.invalidate();
            }
        }
        updateProxyStatus(null);
        listView.invalidate();
    }

    private void updateProxyStatus(SharedConfig.ProxyInfo proxyInfo) {
        currentConnectionState = ConnectionsManager.getInstance(currentAccount).getConnectionState();
        if (listView == null) {
            return;
        }
        for (int i = 0; i < listView.getChildCount(); i++) {
            View view = listView.getChildAt(i);
            if (view instanceof ProxySettingsSheet25.RouteHeader) {
                bindRoute((ProxySettingsSheet25.RouteHeader) view);
            } else if (view instanceof ProxySettingsSheet25.PanelHeader) {
                bindPanel((ProxySettingsSheet25.PanelHeader) view);
            }
            updateProxyCell(view, proxyInfo);
        }
    }

    private void updateProxyCell(View view, SharedConfig.ProxyInfo proxyInfo) {
        if (view instanceof TextDetailProxyCell) {
            TextDetailProxyCell cell = (TextDetailProxyCell) view;
            if (cell.currentInfo == null || proxyInfo != null && cell.currentInfo != proxyInfo) return;
            cell.setChecked(cell.currentInfo == SharedConfig.currentProxy);
            cell.updateStatus();
            cell.invalidate();
        } else if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) updateProxyCell(group.getChildAt(i), proxyInfo);
        }
    }

    @Override
    public void didReceivedNotification(int id, int account, Object... args) {
        if (id == NotificationCenter.proxyChangedByRotation) {
            useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
            updateProxyStatus(null);
            updateRows(true);
        } else if (id == NotificationCenter.proxySettingsChanged) {
            useProxySettings = MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false) && !SharedConfig.proxyList.isEmpty();
            currentConnectionState = ConnectionsManager.getInstance(currentAccount).getConnectionState();
            updateRows(true);
        } else if (id == NotificationCenter.didUpdateConnectionState) {
            int state = ConnectionsManager.getInstance(account).getConnectionState();
            if (currentConnectionState != state) {
                currentConnectionState = state;
                if (listView != null) {
                    updateProxyStatus(SharedConfig.currentProxy);

                    if (currentConnectionState == ConnectionsManager.ConnectionStateConnected) {
                        updateRows(true);
                    }
                }
            }
        } else if (id == NotificationCenter.proxyCheckDone) {
            if (listView != null) {
                SharedConfig.ProxyInfo proxyInfo = (SharedConfig.ProxyInfo) args[0];
                updateProxyStatus(proxyInfo);

                boolean checking = false;
                if (!wasCheckedAllList) {
                    for (SharedConfig.ProxyInfo info : proxyList) {
                        if (info.checking || info.availableCheckTime == 0) {
                            checking = true;
                            break;
                        }
                    }
                    if (!checking) {
                        wasCheckedAllList = true;
                        updateRows(true);
                    }
                }
            }
        }
    }

    private class ListAdapter extends RecyclerListView.SelectionAdapter {
        private final static int VIEW_TYPE_SHADOW = 0,
            VIEW_TYPE_TEXT_SETTING = 1,
            VIEW_TYPE_HEADER = 2,
            VIEW_TYPE_TEXT_CHECK = 3,
            VIEW_TYPE_INFO = 4,
            VIEW_TYPE_PROXY_DETAIL = 5,
            VIEW_TYPE_TIMEOUT = 6,
            VIEW_TYPE_TEXT_CELL = 7,
            VIEW_TYPE_ROUTE = 8,
            VIEW_TYPE_PANEL_HEADER = 9,
            VIEW_TYPE_PRIMARY = 10,
            VIEW_TYPE_BOTTOM = 11,
            VIEW_TYPE_SUBSCRIPTION = 12;

        public static final int PAYLOAD_CHECKED_CHANGED = 0;
        public static final int PAYLOAD_SELECTION_CHANGED = 1;
        public static final int PAYLOAD_SELECTION_MODE_CHANGED = 2;

        private Context mContext;

        public ListAdapter(Context context) {
            mContext = context;

            setHasStableIds(true);
        }

        public void toggleSelected(int position) {
            if (position < proxyStartRow || position >= proxyEndRow) {
                return;
            }
            SharedConfig.ProxyInfo info = proxyList.get(position - proxyStartRow);
            if (selectedItems.contains(info)) {
                selectedItems.remove(info);
            } else {
                selectedItems.add(info);
            }
            notifyItemChanged(position, PAYLOAD_SELECTION_CHANGED);
            checkActionMode();
        }

        public void clearSelected() {
            selectedItems.clear();
            if (proxyStartRow >= 0) {
                notifyItemRangeChanged(proxyStartRow, proxyEndRow - proxyStartRow, PAYLOAD_SELECTION_CHANGED);
            }
            checkActionMode();
        }

        private void checkActionMode() {
            int selectedCount = selectedItems.size();
            boolean canShare = selectedCount > 0;
            for (SharedConfig.ProxyInfo info : selectedItems) {
                if (!com.exteragram.messenger.proxy.ProxyController.canShareProxy(info)) {
                    canShare = false;
                    break;
                }
            }
            shareMenuItem.setVisibility(canShare ? View.VISIBLE : View.GONE);
            boolean actionModeShowed = actionBar.isActionModeShowed();
            if (selectedCount > 0) {
                selectedCountTextView.setNumber(selectedCount, actionModeShowed);
                if (!actionModeShowed) {
                    actionBar.showActionMode();
                    notifyItemRangeChanged(proxyStartRow, proxyEndRow - proxyStartRow, PAYLOAD_SELECTION_MODE_CHANGED);
                }
            } else if (actionModeShowed) {
                actionBar.hideActionMode();
                if (proxyStartRow >= 0) {
                    notifyItemRangeChanged(proxyStartRow, proxyEndRow - proxyStartRow, PAYLOAD_SELECTION_MODE_CHANGED);
                }
            }
        }

        @Override
        public int getItemCount() {
            return rowCount;
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
            switch (holder.getItemViewType()) {
                case VIEW_TYPE_SHADOW: {
                    break;
                }
                case VIEW_TYPE_ROUTE: {
                    bindRoute((ProxySettingsSheet25.RouteHeader) holder.itemView);
                    break;
                }
                case VIEW_TYPE_PANEL_HEADER: {
                    bindPanel((ProxySettingsSheet25.PanelHeader) holder.itemView);
                    break;
                }
                case VIEW_TYPE_SUBSCRIPTION: {
                    bindSubscription((FGSubscriptionOrbitCell) holder.itemView, subscriptions.get(position - subscriptionStartRow));
                    break;
                }
                case VIEW_TYPE_TEXT_SETTING:
                case VIEW_TYPE_TIMEOUT: {
                    ProxySettingsSheet25.ActionCell cell = (ProxySettingsSheet25.ActionCell) holder.itemView;
                    if (position == proxyAddRow) {
                        cell.bind(getString(R.string.AddProxy), ProxySettingsSheet25.Glyph.ADD, false);
                    } else if (position == fgOutboundAddRow) {
                        cell.bind(getString(R.string.FG_Outbound_Add), ProxySettingsSheet25.Glyph.IMPORT, false);
                    } else if (position == subscriptionAddRow) {
                        cell.bind(FGSubscriptionUi.text("FG_Subscription_Add"), ProxySettingsSheet25.Glyph.ADD, false);
                    } else if (position == rotationTimeoutRow) {
                        int index = Math.max(0, Math.min(SharedConfig.proxyRotationTimeout, ProxyRotationController.ROTATION_TIMEOUTS.size() - 1));
                        cell.bind(LocaleController.formatString(R.string.ProxyRotationTimeoutSeconds, ProxyRotationController.ROTATION_TIMEOUTS.get(index)), ProxySettingsSheet25.Glyph.ROTATE, false);
                    } else if (position == deleteAllRow) {
                        cell.bind(subscriptions.isEmpty() ? getString(R.string.DeleteAllProxies) : FGSubscriptionUi.text("FG_Subscription_DeleteManual"), ProxySettingsSheet25.Glyph.DELETE, true);
                    }
                    break;
                }
                case VIEW_TYPE_HEADER: {
                    ProxySettingsSheet25.SectionHeader header = (ProxySettingsSheet25.SectionHeader) holder.itemView;
                    header.setText(position == subscriptionHeaderRow ? FGSubscriptionUi.text("FG_Subscription_Heading") : getString(R.string.ProxyConnections));
                    header.updateColors();
                    break;
                }
                case VIEW_TYPE_PRIMARY:
                    bindToggles((ProxySettingsSheet25.TogglePairCell) holder.itemView);
                    break;
                case VIEW_TYPE_TEXT_CHECK: {
                    ProxySettingsSheet25.ToggleCell cell = (ProxySettingsSheet25.ToggleCell) holder.itemView;
                    cell.bind(getString(R.string.UseProxyRotation), SharedConfig.proxyRotationEnabled);
                    break;
                }
                case VIEW_TYPE_INFO: {
                    TextInfoPrivacyCell cell = (TextInfoPrivacyCell) holder.itemView;
                    if (position == rotationTimeoutInfoRow) {
                        cell.setText(getString(R.string.ProxyRotationTimeoutInfo));
                    } else if (position == safeSurfShadowRow) {
                        CharSequence tosInfoText = AndroidUtilities.replaceSingleTag(LocaleController.getString(R.string.FG_SafeSurf_TOS),
                                Theme.key_windowBackgroundWhiteLinkText,
                                AndroidUtilities.REPLACING_TAG_TYPE_LINKBOLD,
                                () -> Browser.openUrl(getContext(), Constants.FG_PRIVACY_URL)
                        );
                        cell.setText(tosInfoText);
                    }
                    break;
                }
                case VIEW_TYPE_PROXY_DETAIL: {
                    TextDetailProxyCell cell = (TextDetailProxyCell) holder.itemView;
                    SharedConfig.ProxyInfo info = proxyList.get(position - proxyStartRow);
                    cell.setProxy(info);
                    cell.setChecked(SharedConfig.currentProxy == info);
                    cell.setItemSelected(selectedItems.contains(info), false);
                    cell.setSelectionEnabled(!selectedItems.isEmpty(), false);
                    break;
                }
                case VIEW_TYPE_TEXT_CELL: {
                    TextCell textCell = (TextCell) holder.itemView;

                    if (position == safeSurfRow) {
                        textCell.getImageView().clearColorFilter();
                        textCell.setTextAndIcon("SafeSurf VPN", R.drawable.safesurf_logo, false);
                    }
                    break;
                }
            }
        }

        @SuppressWarnings("unchecked")
        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position, @NonNull List payloads) {
            if (holder.getItemViewType() == VIEW_TYPE_PROXY_DETAIL && !payloads.isEmpty()) {
                TextDetailProxyCell cell = (TextDetailProxyCell) holder.itemView;
                if (payloads.contains(PAYLOAD_SELECTION_CHANGED)) {
                    cell.setItemSelected(selectedItems.contains(proxyList.get(position - proxyStartRow)), true);
                }
                if (payloads.contains(PAYLOAD_SELECTION_MODE_CHANGED)) {
                    cell.setSelectionEnabled(!selectedItems.isEmpty(), true);
                }
            } else if (holder.getItemViewType() == VIEW_TYPE_PRIMARY && payloads.contains(PAYLOAD_CHECKED_CHANGED)) {
                bindToggles((ProxySettingsSheet25.TogglePairCell) holder.itemView);
            } else if (holder.getItemViewType() == VIEW_TYPE_TEXT_CHECK && payloads.contains(PAYLOAD_CHECKED_CHANGED)) {
                ((ProxySettingsSheet25.ToggleCell) holder.itemView).setChecked(SharedConfig.proxyRotationEnabled, false);
            } else {
                super.onBindViewHolder(holder, position, payloads);
            }
        }

        @Override
        public void onViewAttachedToWindow(RecyclerView.ViewHolder holder) {
            if (holder.itemView instanceof ProxySettingsSheet25.Themed) {
                ((ProxySettingsSheet25.Themed) holder.itemView).updateColors();
            }
            if (holder.itemView instanceof ProxySettingsSheet25.RouteHeader) {
                bindRoute((ProxySettingsSheet25.RouteHeader) holder.itemView);
            } else if (holder.itemView instanceof ProxySettingsSheet25.PanelHeader) {
                bindPanel((ProxySettingsSheet25.PanelHeader) holder.itemView);
            }
            int viewType = holder.getItemViewType();
            if (viewType == VIEW_TYPE_PRIMARY) {
                bindToggles((ProxySettingsSheet25.TogglePairCell) holder.itemView);
            } else if (viewType == VIEW_TYPE_TEXT_CHECK) {
                ((ProxySettingsSheet25.ToggleCell) holder.itemView).setChecked(SharedConfig.proxyRotationEnabled, false);
            }
        }

        @Override
        public boolean isEnabled(RecyclerView.ViewHolder holder) {
            int position = holder.getAdapterPosition();
            return position >= 0 && (position == rotationTimeoutRow || position == rotationRow || position == proxyAddRow || position == fgOutboundAddRow || position == subscriptionAddRow || position == deleteAllRow || position >= proxyStartRow && position < proxyEndRow || position == safeSurfRow);
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view;
            switch (viewType) {
                case VIEW_TYPE_ROUTE:
                    view = new ProxySettingsSheet25.RouteHeader(mContext);
                    break;
                case VIEW_TYPE_PANEL_HEADER:
                    view = new ProxySettingsSheet25.PanelHeader(mContext);
                    break;
                case VIEW_TYPE_SUBSCRIPTION:
                    view = new FGSubscriptionOrbitCell(mContext);
                    break;
                case VIEW_TYPE_SHADOW:
                case VIEW_TYPE_BOTTOM:
                    view = new View(mContext);
                    view.setMinimumHeight(AndroidUtilities.dp(viewType == VIEW_TYPE_BOTTOM ? 24 : 16));
                    break;
                case VIEW_TYPE_TEXT_SETTING:
                case VIEW_TYPE_TIMEOUT:
                    view = new ProxySettingsSheet25.ActionCell(mContext);
                    break;
                case VIEW_TYPE_HEADER:
                    view = new ProxySettingsSheet25.SectionHeader(mContext);
                    break;
                case VIEW_TYPE_PRIMARY:
                    view = new ProxySettingsSheet25.TogglePairCell(mContext);
                    break;
                case VIEW_TYPE_TEXT_CHECK:
                    view = new ProxySettingsSheet25.ToggleCell(mContext, false);
                    break;
                case VIEW_TYPE_INFO:
                    TextInfoPrivacyCell info = new TextInfoPrivacyCell(mContext);
                    info.getTextView().setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
                    view = info;
                    break;
                case VIEW_TYPE_TEXT_CELL:
                    view = new TextCell(mContext);
                    break;
                case VIEW_TYPE_PROXY_DETAIL:
                default:
                    view = new TextDetailProxyCell(mContext);
                    break;
            }
            RecyclerView.LayoutParams params = new RecyclerView.LayoutParams(RecyclerView.LayoutParams.MATCH_PARENT, RecyclerView.LayoutParams.WRAP_CONTENT);
            if (viewType == VIEW_TYPE_PRIMARY || viewType == VIEW_TYPE_TEXT_SETTING || viewType == VIEW_TYPE_TIMEOUT || viewType == VIEW_TYPE_SUBSCRIPTION) {
                params.leftMargin = params.rightMargin = AndroidUtilities.dp(16);
                params.topMargin = params.bottomMargin = AndroidUtilities.dp(viewType == VIEW_TYPE_PRIMARY ? 6 : 4);
            }
            view.setLayoutParams(params);
            return new RecyclerListView.Holder(view);
        }

        @Override
        public long getItemId(int position) {

            if (position == routeRow) {
                return -14;
            } else if (position == panelHeaderRow) {
                return -15;
            } else if (position == panelBottomRow) {
                return -16;
            } else if (position == useProxyShadowRow) {
                return -1;
            } else if (position == proxyShadowRow) {
                return -2;
            } else if (position == proxyAddRow) {
                return -3;
            } else if (position == fgOutboundAddRow) {
                return -13;
            } else if (position == subscriptionAddRow) {
                return -18;
            } else if (position == subscriptionHeaderRow) {
                return -19;
            } else if (position >= subscriptionStartRow && position < subscriptionEndRow) {
                return 0x100000000L + (subscriptions.get(position - subscriptionStartRow).id.hashCode() & 0xffffffffL);
            } else if (position == useProxyRow) {
                return -4;

            } else if (position == connectionsHeaderRow) {
                return -6;
            } else if (position == deleteAllRow) {
                return -8;
            } else if (position == rotationRow) {
                return -9;
            } else if (position == rotationTimeoutRow) {
                return -10;
            } else if (position == rotationTimeoutInfoRow) {
                return -11;
            } else if (position >= proxyStartRow && position < proxyEndRow) {
                return proxyList.get(position - proxyStartRow).hashCode();
            } else if (position == safeSurfRow) {
                return -1390;
            } else if (position == safeSurfShadowRow) {
                return -1391;
            } else {
                return -7;
            }
        }

        @Override
        public int getItemViewType(int position) {
            if (position == routeRow) {
                return VIEW_TYPE_ROUTE;
            } else if (position == panelHeaderRow) {
                return VIEW_TYPE_PANEL_HEADER;
            } else if (position == panelBottomRow) {
                return VIEW_TYPE_BOTTOM;
            } else if (position == useProxyRow) {
                return VIEW_TYPE_PRIMARY;
            } else if (position == useProxyShadowRow || position == proxyShadowRow) {
                return VIEW_TYPE_SHADOW;
            } else if (position == proxyAddRow || position == deleteAllRow
                    || position == fgOutboundAddRow || position == subscriptionAddRow) {

                return VIEW_TYPE_TEXT_SETTING;
            } else if (position == rotationRow) {
                return VIEW_TYPE_TEXT_CHECK;
            } else if (position == connectionsHeaderRow || position == subscriptionHeaderRow) {
                return VIEW_TYPE_HEADER;
            } else if (position >= subscriptionStartRow && position < subscriptionEndRow) {
                return VIEW_TYPE_SUBSCRIPTION;
            } else if (position == rotationTimeoutRow) {
                return VIEW_TYPE_TIMEOUT;
            } else if (position >= proxyStartRow && position < proxyEndRow) {
                return VIEW_TYPE_PROXY_DETAIL;
            } else if (position == safeSurfRow) {
                return VIEW_TYPE_TEXT_CELL;
            } else {
                return VIEW_TYPE_INFO;
            }
        }
    }

    @Override
    public ArrayList<ThemeDescription> getThemeDescriptions() {
        ArrayList<ThemeDescription> themeDescriptions = new ArrayList<>();

        ThemeDescription.ThemeDescriptionDelegate delegate = this::updateProxyTheme;
        int[] keys = {
                Theme.key_windowBackgroundWhite,
                Theme.key_windowBackgroundGray,
                Theme.key_windowBackgroundWhiteBlackText,
                Theme.key_windowBackgroundWhiteGrayText2,
                Theme.key_windowBackgroundWhiteGrayText3,
                Theme.key_windowBackgroundWhiteBlueText6,
                Theme.key_windowBackgroundWhiteBlueButton,
                Theme.key_featuredStickers_buttonText,
                Theme.key_windowBackgroundWhiteGreenText,
                Theme.key_text_RedRegular,
                Theme.key_listSelector,
                Theme.key_divider,
                Theme.key_switchTrack,
                Theme.key_switchTrackChecked,
                Theme.key_checkbox,
                Theme.key_radioBackground,
                Theme.key_checkboxCheck
        };
        for (int key : keys) {
            themeDescriptions.add(new ThemeDescription(listView, 0, null, null, null, delegate, key));
        }
        themeDescriptions.add(new ThemeDescription(fragmentView, ThemeDescription.FLAG_BACKGROUND, null, null, null, null, Theme.key_windowBackgroundGray));
        themeDescriptions.add(new ThemeDescription(listView, ThemeDescription.FLAG_LISTGLOWCOLOR, null, null, null, null, Theme.key_actionBarDefault));
        themeDescriptions.add(new ThemeDescription(actionBar, ThemeDescription.FLAG_AB_ITEMSCOLOR, null, null, null, null, Theme.key_actionBarDefaultIcon));
        themeDescriptions.add(new ThemeDescription(actionBar, ThemeDescription.FLAG_AB_TITLECOLOR, null, null, null, null, Theme.key_actionBarDefaultTitle));
        themeDescriptions.add(new ThemeDescription(actionBar, ThemeDescription.FLAG_AB_SELECTORCOLOR, null, null, null, null, Theme.key_actionBarDefaultSelector));
        themeDescriptions.add(new ThemeDescription(listView, ThemeDescription.FLAG_SELECTOR, null, null, null, null, Theme.key_listSelector));
        themeDescriptions.add(new ThemeDescription(listView, 0, null, Theme.dividerPaint, null, delegate, Theme.key_divider));
        themeDescriptions.add(new ThemeDescription(listView, 0, new Class[]{TextInfoPrivacyCell.class}, new String[]{"textView"}, null, null, null, Theme.key_windowBackgroundWhiteGrayText4));

        return themeDescriptions;
    }
    private int safeSurfRow = -1;
    private int safeSurfShadowRow = -1;

}
