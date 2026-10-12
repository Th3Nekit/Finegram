package com.th3nekit.finegram.core.ui.proxy;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.core.graphics.ColorUtils;
import com.exteragram.messenger.proxy.ProxyController;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.NotificationCenter;
import org.telegram.messenger.R;
import org.telegram.messenger.SharedConfig;
import org.telegram.tgnet.ConnectionsManager;
import org.telegram.tgnet.TLRPC;
import org.telegram.proxy.ProxySettings;
import org.telegram.ui.ActionBar.ActionBar;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.ActionBar.ThemeDescription;
import java.util.ArrayList;
import java.util.WeakHashMap;
import static org.telegram.messenger.LocaleController.getString;

public final class ProxyConnectionActivity extends BaseFragment implements NotificationCenter.NotificationCenterDelegate {
    private static final WeakHashMap<SharedConfig.ProxyInfo, ProxySettings> activeChecks = new WeakHashMap<>();
    private final SharedConfig.ProxyInfo proxy;
    private TextView proxyStatus;
    private TextView telegramStatus;
    private TextView retry;
    private ConnectionDiagram diagram;
    private boolean resumed;
    private int generation;
    private int availability;
    private long ping;
    private Runnable timeout;
    private ProxySettings checkedSettings;
    private ProxySettings confirmedSettings;
    private int confirmationRequest;
    private int confirmationGeneration;

    public ProxyConnectionActivity(int account, SharedConfig.ProxyInfo proxy) {
        setCurrentAccount(account);
        this.proxy = proxy;
    }

    @Override public boolean onFragmentCreate() {
        super.onFragmentCreate();
        getNotificationCenter().addObserver(this, NotificationCenter.didUpdateConnectionState);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().addObserver(this, NotificationCenter.proxyCheckDone);
        return true;
    }

    @Override public View createView(Context context) {
        actionBar.setTitle(getString(R.string.FG_ProxyConnection));
        actionBar.setBackButtonImage(R.drawable.ic_ab_back);
        actionBar.setActionBarMenuOnItemClick(new ActionBar.ActionBarMenuOnItemClick() {
            @Override public void onItemClick(int id) { if (id == -1) finishFragment(); }
        });
        ScrollView scroll = new ScrollView(context);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(getThemedColor(Theme.key_windowBackgroundGray));
        LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setLayoutDirection(LocaleController.isRTL ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        body.setPadding(AndroidUtilities.dp(24), AndroidUtilities.dp(36), AndroidUtilities.dp(24), AndroidUtilities.dp(28));
        scroll.addView(body, new ScrollView.LayoutParams(-1, -2));

        TextView title = text(context, 28, Theme.key_windowBackgroundWhiteBlackText);
        String name = ProxyController.getInstance().getName(proxy);
        title.setText(TextUtils.isEmpty(name) ? ProxyController.getProxyTypeName(proxy) : name);
        title.setGravity(Gravity.CENTER);
        body.addView(title, new LinearLayout.LayoutParams(-1, -2));
        TextView type = text(context, 14, Theme.key_windowBackgroundWhiteGrayText);
        type.setText(ProxyController.getProxyTypeName(proxy));
        type.setGravity(Gravity.CENTER);
        add(body, type, 8);

        diagram = new ConnectionDiagram(context);
        LinearLayout.LayoutParams drawing = new LinearLayout.LayoutParams(-1, AndroidUtilities.dp(150));
        drawing.topMargin = AndroidUtilities.dp(24);
        body.addView(diagram, drawing);
        LinearLayout labels = new LinearLayout(context);
        for (int id : new int[]{R.string.FG_ProxyPhone, R.string.FG_ProxyNode, R.string.FG_ProxyTelegram}) {
            TextView label = text(context, 13, Theme.key_windowBackgroundWhiteBlackText);
            label.setText(getString(id)); label.setGravity(Gravity.CENTER);
            labels.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
        }
        body.addView(labels, new LinearLayout.LayoutParams(-1, -2));
        proxyStatus = text(context, 17, Theme.key_windowBackgroundWhiteBlackText);
        proxyStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        proxyStatus.setPadding(AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(20));
        proxyStatus.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(20), getThemedColor(Theme.key_windowBackgroundWhite)));
        add(body, proxyStatus, 32);
        telegramStatus = text(context, 17, Theme.key_windowBackgroundWhiteBlackText);
        telegramStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        telegramStatus.setPadding(AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(20), AndroidUtilities.dp(20));
        telegramStatus.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(20), getThemedColor(Theme.key_windowBackgroundWhite)));
        add(body, telegramStatus, 12);
        TextView detail = text(context, 14, Theme.key_windowBackgroundWhiteGrayText);
        detail.setText(getString(R.string.FG_ProxyDetails));
        add(body, detail, 16);
        retry = text(context, 16, Theme.key_featuredStickers_buttonText);
        retry.setText(getString(R.string.FG_ProxyRetry));
        retry.setGravity(Gravity.CENTER);
        retry.setPadding(AndroidUtilities.dp(16), AndroidUtilities.dp(16), AndroidUtilities.dp(16), AndroidUtilities.dp(16));
        retry.setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(24), getThemedColor(Theme.key_featuredStickers_addButton), getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        retry.setOnClickListener(v -> check());
        add(body, retry, 28);
        fragmentView = scroll;
        render();
        return fragmentView;
    }

    private TextView text(Context context, int size, int color) {
        TextView view = new TextView(context);
        view.setTextSize(size);
        view.setTextColor(getThemedColor(color));
        view.setTag(color);
        return view;
    }

    private void add(LinearLayout body, View view, int margin) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = AndroidUtilities.dp(margin);
        body.addView(view, lp);
    }

    private boolean selected() {
        return SharedConfig.currentProxy == proxy && MessagesController.getGlobalMainSettings().getBoolean("proxy_enabled", false);
    }

    private boolean connected() {
        return confirmedSettings == proxy.settings && ApplicationLoader.isNetworkOnline() && selected()
                && getConnectionsManager().getConnectionState() == ConnectionsManager.ConnectionStateConnected;
    }

    private void resetConfirmation() {
        confirmedSettings = null;
        confirmationGeneration++;
        if (confirmationRequest != 0) getConnectionsManager().cancelRequest(confirmationRequest, true);
        confirmationRequest = 0;
    }

    private void confirmConnection() {
        if (!resumed || !selected() || confirmationRequest != 0 || connected()
                || !ApplicationLoader.isNetworkOnline()
                || getConnectionsManager().getConnectionState() != ConnectionsManager.ConnectionStateConnected) return;
        final ProxySettings settings = proxy.settings;
        final int token = confirmationGeneration;
        confirmationRequest = getConnectionsManager().sendRequest(new TLRPC.TL_help_getNearestDc(), (response, error) -> AndroidUtilities.runOnUIThread(() -> {
            if (token != confirmationGeneration) return;
            confirmationRequest = 0;
            if (resumed && selected() && proxy.settings == settings && error == null && response instanceof TLRPC.TL_nearestDc) {
                confirmedSettings = settings;
            }
            render();
        }));
    }

    private void check() {
        if (!resumed) return;
        if (!ApplicationLoader.isNetworkOnline()) { availability = -3; render(); return; }
        if (!proxy.settings.isValid()) { availability = -1; render(); return; }
        if (timeout != null) AndroidUtilities.cancelRunOnUIThread(timeout);
        final int token = ++generation;
        checkedSettings = proxy.settings;
        final ProxySettings settings = checkedSettings;
        availability = 0;
        render();
        timeout = () -> { if (resumed && generation == token) { availability = -2; render(); } };
        AndroidUtilities.runOnUIThread(timeout, 35000);
        ProxySettings activeSettings = activeChecks.get(proxy);
        if (proxy.checking && activeSettings != null && activeSettings != settings) proxy.checking = false;
        if (proxy.checking) return;
        proxy.checking = true;
        activeChecks.put(proxy, settings);
        getConnectionsManager().checkProxy(settings, time -> AndroidUtilities.runOnUIThread(() -> {
            if (activeChecks.get(proxy) != settings) return;
            activeChecks.remove(proxy);
            proxy.checking = false;
            if (proxy.settings != settings) return;
            proxy.available = time >= 0;
            proxy.ping = time < 0 ? 0 : time;
            proxy.availableCheckTime = SystemClock.elapsedRealtime();
            NotificationCenter.getGlobalInstance().postNotificationName(NotificationCenter.proxyCheckDone, proxy);
        }));
    }

    private void render() {
        if (diagram == null) return;
        boolean online = connected();
        boolean offline = !ApplicationLoader.isNetworkOnline();
        int id = offline ? R.string.FG_ProxyNoNetwork : availability == 1 ? R.string.FG_ProxyReady : availability == -2 ? R.string.FG_ProxyTimeout : availability == -1 ? R.string.FG_ProxyFailed : R.string.FG_ProxyChecking;
        proxyStatus.setText(getString(id) + (availability == 1 ? " · " + ping + " ms" : ""));
        telegramStatus.setText(getString(!selected() ? R.string.FG_ProxyInactive : online ? R.string.FG_ProxyOnline : R.string.FG_ProxyWaiting));
        retry.setEnabled(availability != 0);
        retry.setAlpha(availability == 0 ? .5f : 1f);
        diagram.first = !offline && (online || availability == 1);
        diagram.second = online;
        diagram.offline = offline;
        diagram.failed = availability < 0 && !online && !offline;
        diagram.animate(resumed && !offline && (availability == 0 || (selected() && !online && availability == 1)));
        diagram.invalidate();
    }

    @Override public void didReceivedNotification(int id, int account, Object... args) {
        if (!resumed) return;
        if (id == NotificationCenter.didUpdateConnectionState && availability == -3 && ApplicationLoader.isNetworkOnline()) { check(); return; }
        if (id == NotificationCenter.proxySettingsChanged) {
            resetConfirmation();
            if (checkedSettings != proxy.settings) {
                proxy.checking = false;
                check();
            }
            AndroidUtilities.runOnUIThread(this::confirmConnection);
        }
        if (id == NotificationCenter.didUpdateConnectionState) {
            if (getConnectionsManager().getConnectionState() != ConnectionsManager.ConnectionStateConnected) resetConfirmation();
            else confirmConnection();
        }
        if (id == NotificationCenter.proxyCheckDone) {
            if (args.length == 0 || args[0] != proxy || checkedSettings != proxy.settings) return;
            if (timeout != null) AndroidUtilities.cancelRunOnUIThread(timeout);
            availability = proxy.available ? 1 : -1;
            ping = proxy.ping;
        }
        render();
    }
    @Override public void onResume() { super.onResume(); resumed = true; updateColors(); check(); confirmConnection(); }

    private void updateTextColors(View view) {
        if (view instanceof TextView && view.getTag() instanceof Integer) ((TextView) view).setTextColor(getThemedColor((int) view.getTag()));
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) updateTextColors(group.getChildAt(i));
        }
    }

    private void updateColors() {
        if (fragmentView == null || proxyStatus == null) return;
        fragmentView.setBackgroundColor(getThemedColor(Theme.key_windowBackgroundGray));
        updateTextColors(fragmentView);
        proxyStatus.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(20), getThemedColor(Theme.key_windowBackgroundWhite)));
        telegramStatus.setBackground(Theme.createRoundRectDrawable(AndroidUtilities.dp(20), getThemedColor(Theme.key_windowBackgroundWhite)));
        retry.setBackground(Theme.createSimpleSelectorRoundRectDrawable(AndroidUtilities.dp(24), getThemedColor(Theme.key_featuredStickers_addButton), getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        diagram.invalidate();
    }

    @Override public ArrayList<ThemeDescription> getThemeDescriptions() {
        ArrayList<ThemeDescription> descriptions = new ArrayList<>();
        for (int key : new int[]{Theme.key_windowBackgroundGray, Theme.key_windowBackgroundWhite, Theme.key_windowBackgroundWhiteBlackText,
                Theme.key_windowBackgroundWhiteGrayText, Theme.key_windowBackgroundWhiteBlueText, Theme.key_text_RedRegular,
                Theme.key_featuredStickers_buttonText, Theme.key_featuredStickers_addButton, Theme.key_featuredStickers_addButtonPressed}) {
            descriptions.add(new ThemeDescription(null, 0, null, null, null, this::updateColors, key));
        }
        return descriptions;
    }
    @Override public void onPause() {
        super.onPause(); resumed = false; generation++; resetConfirmation();
        if (timeout != null) AndroidUtilities.cancelRunOnUIThread(timeout);
        if (diagram != null) diagram.animate(false);
    }
    @Override public void onFragmentDestroy() {
        resumed = false; generation++; resetConfirmation();
        if (timeout != null) AndroidUtilities.cancelRunOnUIThread(timeout);
        if (diagram != null) diagram.animate(false);
        getNotificationCenter().removeObserver(this, NotificationCenter.didUpdateConnectionState);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxySettingsChanged);
        NotificationCenter.getGlobalInstance().removeObserver(this, NotificationCenter.proxyCheckDone);
        super.onFragmentDestroy();
    }

    private final class ConnectionDiagram extends View {
        final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Path path = new Path();
        boolean first, second, failed, offline;
        float phase;
        ValueAnimator animator;
        ConnectionDiagram(Context context) { super(context); setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); }
        void animate(boolean active) {
            active &= SharedConfig.animationsEnabled() && (Build.VERSION.SDK_INT < 26 || ValueAnimator.areAnimatorsEnabled());
            if (!active) { if (animator != null) animator.cancel(); animator = null; return; }
            if (animator != null) return;
            animator = ValueAnimator.ofFloat(0, 1);
            animator.setDuration(1400); animator.setRepeatCount(ValueAnimator.INFINITE);
            animator.setInterpolator(new android.view.animation.LinearInterpolator());
            animator.addUpdateListener(a -> { phase = (float) a.getAnimatedValue(); invalidate(); }); animator.start();
        }
        @Override protected void onDetachedFromWindow() { animate(false); super.onDetachedFromWindow(); }
        @Override protected void onDraw(Canvas canvas) {
            canvas.save();
            if (LocaleController.isRTL) canvas.scale(-1, 1, getWidth() / 2f, 0);
            float width = getWidth(), y = getHeight() * .58f;
            float[] x = {width / 6, width / 2, width * 5 / 6};
            float radius = Math.min(AndroidUtilities.dp(28), width / 11);
            int accent = getThemedColor(Theme.key_windowBackgroundWhiteBlueText);
            int muted = getThemedColor(Theme.key_windowBackgroundWhiteGrayText);
            int base = getThemedColor(Theme.key_windowBackgroundWhite);
            paint.setStrokeWidth(AndroidUtilities.dp(3)); paint.setStrokeCap(Paint.Cap.ROUND);
            for (int i=0; i<2; i++) {
                boolean ready = i == 0 ? first : second;
                float from=x[i]+radius+AndroidUtilities.dp(8), to=x[i+1]-radius-AndroidUtilities.dp(8);
                paint.setColor(ready ? accent : ColorUtils.setAlphaComponent(muted, 65));
                canvas.drawLine(from, y, to, y, paint);
                if (!ready && animator != null) { paint.setColor(accent); canvas.drawCircle(from+(to-from)*phase, y, AndroidUtilities.dp(3), paint); }
            }
            for (int i=0; i<3; i++) {
                boolean ready = i==0 || (i==1 ? first : second);
                paint.setStyle(Paint.Style.FILL); paint.setColor(ready ? ColorUtils.blendARGB(base,accent,.16f) : base);
                canvas.drawCircle(x[i],y,radius,paint);
                paint.setColor((i==1 && failed) || (i==0 && offline) ? getThemedColor(Theme.key_text_RedRegular) : ready ? accent : muted);
                paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(AndroidUtilities.dp(2));
                float s=radius*.44f;
                if(i==0) {
                    canvas.drawRoundRect(x[i]-s*.65f,y-s,x[i]+s*.65f,y+s,AndroidUtilities.dp(3),AndroidUtilities.dp(3),paint);
                    canvas.drawPoint(x[i], y+s*.72f,paint);
                } else if(i==1) {
                    for(int row=-1; row<=1; row++) {
                        canvas.drawRoundRect(x[i]-s,y+row*s*.72f-s*.25f,x[i]+s,y+row*s*.72f+s*.25f,AndroidUtilities.dp(2),AndroidUtilities.dp(2),paint);
                        canvas.drawPoint(x[i]-s*.6f,y+row*s*.72f,paint);
                    }
                } else {
                    path.reset(); path.moveTo(x[i]-s,y); path.lineTo(x[i]+s,y-s); path.lineTo(x[i]+s*.45f,y+s); path.lineTo(x[i],y+s*.2f); path.close();
                    canvas.drawPath(path,paint); canvas.drawLine(x[i],y+s*.2f,x[i]+s*.5f,y-s*.5f,paint);
                }
                paint.setStyle(Paint.Style.FILL);
            }
            canvas.restore();
        }
    }
}
