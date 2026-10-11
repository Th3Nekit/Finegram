/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.plugins;

import android.net.Uri;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class FGPluginAbilities {

    private FGPluginAbilities() {
    }

    public static final class Ability {
        public final int icon;
        public final int title;

        Ability(int icon, int title) {
            this.icon = icon;
            this.title = title;
        }
    }

    private static final int READ_LIMIT = 256 * 1024;

    public static List<Ability> of(String pathOrUri) {
        return from(read(pathOrUri));
    }

    private static List<Ability> from(String code) {
        final List<Ability> found = new ArrayList<>();
        if (code == null) {
            return found;
        }

        if (contains(code, "hook_method", "hook_all_methods", "MethodHook", "add_hook", "joverload")) {
            found.add(new Ability(R.drawable.msg_customize, R.string.FG_Plugin_Ability_Hooks));
        }
        if (contains(code, "send_message", "send_file", "SendMessagesHelper", "forward_messages")) {
            found.add(new Ability(R.drawable.msg_send, R.string.FG_Plugin_Ability_Messages));
        }
        if (contains(code, "requests.", "urlopen", "http_get", "socket.", "urllib")) {
            found.add(new Ability(R.drawable.msg_link2, R.string.FG_Plugin_Ability_Network));
        }
        if (contains(code, "file_utils", "open(", "os.remove", "shutil", "zipfile")) {
            found.add(new Ability(R.drawable.msg_download, R.string.FG_Plugin_Ability_Files));
        }
        if (contains(code, "get_messages", "on_update_hook", "pre_request_hook", "post_request_hook")) {
            found.add(new Ability(R.drawable.msg_media, R.string.FG_Plugin_Ability_Updates));
        }
        if (contains(code, "create_settings", "Setting(", "Switch(", "Selector(")) {
            found.add(new Ability(R.drawable.msg_settings, R.string.FG_Plugin_Ability_Settings));
        }
        return found;
    }

    private static boolean contains(String code, String... needles) {
        for (String needle : needles) {
            if (code.contains(needle)) {
                return true;
            }
        }
        return false;
    }

    private static String read(String pathOrUri) {
        if (pathOrUri == null || pathOrUri.isEmpty()) {
            return null;
        }

        final File direct = new File(pathOrUri);
        if (direct.isDirectory()) {
            return FGPluginBundle.readCode(direct, READ_LIMIT, true);
        }
        if (FGPluginBundle.looksLikeBundle(direct)) {
            return FGPluginBundle.readCode(direct, READ_LIMIT);
        }
        try (InputStream input = open(pathOrUri)) {
            if (input == null) {
                return null;
            }
            final ByteArrayOutputStream collected = new ByteArrayOutputStream();
            final byte[] chunk = new byte[8 * 1024];
            while (collected.size() < READ_LIMIT) {
                final int read = input.read(chunk);
                if (read <= 0) {
                    break;
                }
                collected.write(chunk, 0, Math.min(read, READ_LIMIT - collected.size()));
            }
            return collected.toString(StandardCharsets.UTF_8.name());
        } catch (Throwable ignore) {

            return null;
        }
    }

    private static InputStream open(String pathOrUri) throws Throwable {
        if (pathOrUri.startsWith("content://") || pathOrUri.startsWith("file://")) {
            return ApplicationLoader.applicationContext.getContentResolver()
                    .openInputStream(Uri.parse(pathOrUri));
        }
        final File file = new File(pathOrUri);
        return file.isFile() ? new FileInputStream(file) : null;
    }
}
