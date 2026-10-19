/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.privacy;

import org.telegram.messenger.ChatObject;
import org.telegram.messenger.DialogObject;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.UserConfig;
import org.telegram.tgnet.TLRPC;
import org.telegram.ui.ChatActivity;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

/**
 * Решает, что удалять на самом деле.
 *
 * Клиент удаляет сообщения из своей базы по указанию сервера. Если «сохранять удалённое»
 * включено, мы отдаём ему только то, что удалять действительно надо, а остальное
 * помечаем удалённым и оставляем на месте.
 *
 * Отсюда важное следствие: сохранённое сообщение — это то же самое сообщение, а не копия.
 * Вложения, оформление, ответы и правки остаются как были, потому что мы ничего не
 * переписывали.
 */
public final class FGDeletedKeeper {

    private FGDeletedKeeper() {
    }

    /**
     * Что из списка отдать клиенту на удаление.
     *
     * Возвращает либо исходный список, либо только временные сообщения — те, что ещё не
     * дошли до сервера. Их придерживать нельзя: у них нет постоянного номера, и остаться
     * в базе они могут только мусором.
     */
    public static ArrayList<Integer> keep(int account, long dialogId, ArrayList<Integer> messages, int mode) {
        if (messages == null || messages.isEmpty()) return messages;
        if (!enabledFor(account, dialogId, mode)) return messages;

        final ArrayList<Integer> temporary = new ArrayList<>();
        final ArrayList<Integer> real = new ArrayList<>();
        for (Integer id : messages) {
            if (id == null) continue;
            if (id > 0) {
                real.add(id);
            } else {
                temporary.add(id);
            }
        }
        if (real.isEmpty()) return messages;

        FGDeletedStore.remember(account, dialogId, real);
        return temporary;
    }

    /**
     * Надо ли придерживать удалённое в этом чате.
     *
     * Один ответ на два вопроса: что не удалять из базы и что не убирать с экрана. Раньше
     * решение принималось только в хранилище, и открытый чат всё равно прощался с
     * сообщением — оно улетало с анимацией и возвращалось лишь при повторном входе.
     */
    public static boolean enabledFor(int account, long dialogId, int mode) {
        if (!FinegramPrivacyConfig.INSTANCE.getSaveDeleted()) return false;
        if (FGDeletedExceptions.excluded(account, dialogId)) return false;
        // Отложенные, быстрые ответы и приветствия — не переписка, а заготовки. Их удаляет
        // сам человек, и держать их «на всякий случай» незачем.
        if (mode == ChatActivity.MODE_SCHEDULED || mode == ChatActivity.MODE_QUICK_REPLIES
                || mode == ChatActivity.MODE_WELCOME_MESSAGES) {
            return false;
        }
        // Очистка истории и удаление чата проходят тем же путём. Там человек сам попросил
        // убрать всё, и спорить с ним мы не будем.
        if (clearingHistory()) return false;
        return !skipDialog(account, dialogId);
    }

    /**
     * Что из удалённого оставить на экране открытого чата.
     *
     * Здесь, в отличие от хранилища, чат известен точно — его знает сам экран. Поэтому
     * пометку ставим сразу, не дожидаясь, пока до записи доберётся очередь хранилища:
     * иначе сообщение успевает пропасть.
     */
    public static ArrayList<Integer> holdInOpenChat(int account, long dialogId, java.util.List<Integer> messages, int mode) {
        final ArrayList<Integer> held = new ArrayList<>();
        if (messages == null || messages.isEmpty()) return held;
        if (!enabledFor(account, dialogId, mode)) return held;
        for (Integer id : messages) {
            // Временные номера придерживать нельзя: без постоянного номера сообщение
            // останется в базе мусором.
            if (id != null && id > 0) held.add(id);
        }
        if (!held.isEmpty()) {
            FGDeletedStore.markNow(account, dialogId, held);
        }
        return held;
    }

    /**
     * Не идёт ли сейчас очистка истории.
     *
     * Отличить её от обычного удаления можно только по тому, кто позвал: указание приходит
     * то же самое. Заглядываем в стек — приём нехороший, но другого признака здесь нет, а
     * без него «очистить историю» переставало бы работать.
     */
    private static boolean clearingHistory() {
        try {
            for (StackTraceElement element : Thread.currentThread().getStackTrace()) {
                final String method = element.getMethodName();
                if ("deleteMessagesRange".equals(method)
                        || "deleteDialog".equals(method)
                        || "clearUserPhotos".equals(method)) {
                    return true;
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    /**
     * Чаты, в которых придерживать нечего.
     *
     * Свои сообщения человек удаляет сам и обычно нарочно; боты сыплют служебным, и их
     * удалённое никому не интересно. Секретные чаты не трогаем принципиально: там
     * исчезновение сообщения — часть договора между собеседниками.
     */
    private static boolean skipDialog(int account, long dialogId) {
        try {
            // Ноль — это «чат неизвестен». Так приходит удаление в личной переписке:
            // сервер не указывает собеседника. Записывать такое нельзя — в журнале
            // появлялся несуществующий «чат 0», а пометка не доставалась ни одному
            // сообщению. Настоящий чат выясняет хранилище и зовёт нас уже с ним.
            if (dialogId == 0) return true;
            if (DialogObject.isEncryptedDialog(dialogId)) return true;

            if (dialogId == UserConfig.getInstance(account).getClientUserId()) return true;

            if (dialogId > 0) {
                final TLRPC.User user = MessagesController.getInstance(account).getUser(dialogId);
                if (user != null && user.bot && !FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromBots()) {
                    return true;
                }
            } else if (!FinegramPrivacyConfig.INSTANCE.getSaveDeletedFromChannels()) {
                final TLRPC.Chat chat = MessagesController.getInstance(account).getChat(-dialogId);
                if (chat != null && ChatObject.isChannelAndNotMegaGroup(chat)) {
                    return true;
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }
}
