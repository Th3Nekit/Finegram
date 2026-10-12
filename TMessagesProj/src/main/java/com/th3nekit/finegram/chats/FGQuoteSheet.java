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

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.view.Gravity;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.content.FileProvider;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.MessageObject;
import org.telegram.messenger.R;
import org.telegram.messenger.Utilities;
import org.telegram.ui.ActionBar.BottomSheet;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.BulletinFactory;
import org.telegram.ui.Components.LayoutHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.util.List;

import com.th3nekit.finegram.core.FinegramLogger;

public class FGQuoteSheet extends BottomSheet {

    private final Bitmap quote;

    public FGQuoteSheet(Context context, Theme.ResourcesProvider resourcesProvider, Bitmap quote) {
        super(context, false, resourcesProvider);
        this.quote = quote;

        fixNavigationBar();
        setApplyBottomPadding(false);

        LinearLayout container = new LinearLayout(context);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(16), dp(16), dp(16), dp(12));

        TextView title = new TextView(context);
        title.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
        title.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 20);
        title.setTypeface(AndroidUtilities.bold());
        title.setText(getString(R.string.FG_Quote_Title));
        container.addView(title, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, 6, 0, 0, 10));

        ImageView preview = new ImageView(context);
        preview.setImageBitmap(quote);
        preview.setAdjustViewBounds(true);
        preview.setScaleType(ImageView.ScaleType.FIT_CENTER);
        container.addView(preview, LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT));

        container.addView(button(context, getString(R.string.FG_Quote_Send), true, () -> share()),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 14, 0, 0));
        container.addView(button(context, getString(R.string.FG_Quote_Save), false, () -> saveToGallery()),
                LayoutHelper.createLinear(LayoutHelper.MATCH_PARENT, 48, 0, 8, 0, 0));

        setCustomView(container);
    }

    private TextView button(Context context, String text, boolean accent, Runnable action) {
        TextView view = new TextView(context);
        view.setGravity(Gravity.CENTER);
        view.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 15);
        view.setTypeface(AndroidUtilities.bold());
        view.setText(text);
        if (accent) {
            view.setTextColor(getThemedColor(Theme.key_featuredStickers_buttonText));
            view.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(8),
                    getThemedColor(Theme.key_featuredStickers_addButton),
                    getThemedColor(Theme.key_featuredStickers_addButtonPressed)));
        } else {
            view.setTextColor(getThemedColor(Theme.key_dialogTextBlack));
            view.setBackground(Theme.createSimpleSelectorRoundRectDrawable(dp(8),
                    getThemedColor(Theme.key_dialogButtonSelector),
                    getThemedColor(Theme.key_listSelector)));
        }
        view.setOnClickListener(v -> action.run());
        return view;
    }

    private File writeToCache() {
        try {
            File directory = new File(ApplicationLoader.applicationContext.getCacheDir(), "quotes");
            if (!directory.exists() && !directory.mkdirs()) {
                return null;
            }
            File file = new File(directory, "quote_" + System.currentTimeMillis() + ".png");
            try (FileOutputStream stream = new FileOutputStream(file)) {
                quote.compress(Bitmap.CompressFormat.PNG, 100, stream);
            }
            return file;
        } catch (Throwable e) {
            FinegramLogger.e("FGQuote", () -> "цитата не сохранилась", e);
            return null;
        }
    }

    private void share() {
        File file = writeToCache();
        if (file == null) {
            reportError();
            return;
        }
        try {
            Context context = getContext();
            Uri uri = FileProvider.getUriForFile(context,
                    ApplicationLoader.getApplicationId() + ".provider", file);
            Intent intent = new Intent(Intent.ACTION_SEND);
            intent.setType("image/png");
            intent.putExtra(Intent.EXTRA_STREAM, uri);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            context.startActivity(Intent.createChooser(intent, getString(R.string.FG_Quote_Send)));
            dismiss();
        } catch (Throwable e) {
            FinegramLogger.e("FGQuote", () -> "цитата не отправилась", e);
            reportError();
        }
    }

    private void saveToGallery() {
        File file = writeToCache();
        if (file == null) {
            reportError();
            return;
        }
        Utilities.globalQueue.postRunnable(() -> {
            boolean saved = false;
            try {
                org.telegram.messenger.MediaController.saveFile(
                        file.getAbsolutePath(), getContext(), 0, null, null, null);
                saved = true;
            } catch (Throwable e) {
                FinegramLogger.e("FGQuote", () -> "цитата не сохранилась в галерею", e);
            }
            final boolean done = saved;
            AndroidUtilities.runOnUIThread(() -> {
                if (done) {
                    BulletinFactory.global().createSimpleBulletin(R.raw.ic_save_to_gallery,
                            getString(R.string.FG_Quote_Saved)).show();
                    dismiss();
                } else {
                    reportError();
                }
            });
        });
    }

    private void reportError() {
        BulletinFactory.global().createErrorBulletin(getString(R.string.FG_Quote_Failed)).show();
    }

    public static void show(Context context, Theme.ResourcesProvider resourcesProvider,
                            List<MessageObject> messages, int account) {
        Bitmap bitmap = FGQuoteMaker.render(messages, account);
        if (bitmap == null) {
            BulletinFactory.global().createErrorBulletin(getString(R.string.FG_Quote_Failed)).show();
            return;
        }
        new FGQuoteSheet(context, resourcesProvider, bitmap).show();
    }
}
