/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.api.dto;

import java.util.Objects;

public final class BadgeDTO {

    private final long documentId;
    private String text;

    public BadgeDTO(long documentId, String text) {
        this.documentId = documentId;
        this.text = text;
    }

    public long getDocumentId() {
        return documentId;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
    }

    public long component1() {
        return documentId;
    }

    public String component2() {
        return text;
    }

    public BadgeDTO copy(long documentId, String text) {
        return new BadgeDTO(documentId, text);
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }
        if (!(other instanceof BadgeDTO)) {
            return false;
        }
        final BadgeDTO badge = (BadgeDTO) other;
        return documentId == badge.documentId && Objects.equals(text, badge.text);
    }

    @Override
    public int hashCode() {
        return Objects.hash(documentId, text);
    }

    @Override
    public String toString() {
        return "BadgeDTO(documentId=" + documentId + ", text=" + text + ")";
    }
}
