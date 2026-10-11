/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.helpers;

import android.graphics.Bitmap;
import android.graphics.Color;

import com.google.zxing.EncodeHintType;
import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel;

import org.telegram.messenger.TelegramQRCodeWriter;
import org.telegram.ui.ActionBar.Theme;

import java.util.HashMap;

import com.th3nekit.finegram.core.FinegramLogger;

public class QrHelper {

    public static Bitmap createQR(String text) {
        try {
            HashMap<EncodeHintType, Object> hints = new HashMap<>();
            hints.put(EncodeHintType.ERROR_CORRECTION, ErrorCorrectionLevel.M);
            hints.put(EncodeHintType.MARGIN, 0);
            hints.put(EncodeHintType.CHARACTER_SET, "UTF-8");
            TelegramQRCodeWriter writer = new TelegramQRCodeWriter();
            return writer.encode(text, 768, 768, hints, null, 1.0f, Color.WHITE, Color.BLACK                                                        );
        } catch (Exception e) {
            FinegramLogger.e(e);
        }
        return null;
    }

}
