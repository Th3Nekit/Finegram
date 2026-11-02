package com.th3nekit.finegram.chats.ui;

public final class MessageMenuSession {
    private Runnable pending;
    private boolean ready;
    private boolean opened;
    private boolean cancelled;
    private boolean closing;
    private boolean closed;

    public void whenReady(Runnable action, boolean waiting) {
        if (cancelled || opened) {
            return;
        }
        pending = action;
        if (!waiting || ready) {
            ready();
        }
    }

    public void ready() {
        ready = true;
        if (cancelled || opened || pending == null) {
            return;
        }
        Runnable action = pending;
        pending = null;
        opened = true;
        action.run();
    }

    public void cancel() {
        cancelled = true;
        pending = null;
    }

    public boolean beginClose() {
        if (closing || closed) {
            return false;
        }
        cancel();
        closing = true;
        return true;
    }

    public boolean finishClose() {
        if (closed) {
            return false;
        }
        cancel();
        closing = false;
        closed = true;
        return true;
    }

    public boolean isClosing() {
        return closing;
    }

    public boolean isClosed() {
        return closed;
    }

    public boolean isCancelled() {
        return cancelled;
    }

    public boolean isOpened() {
        return opened;
    }
}
