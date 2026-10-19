/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.chats.gemini

import com.google.gson.annotations.SerializedName

class GeminiErrorDTO {

    data class ErrorResponse(
        val error: ErrorDetails
    )

    data class ErrorDetails(
        val code: Int,
        val message: String,
        val status: String,
        val details: List<ErrorDetail>? = null
    )

    data class ErrorDetail(
        @SerializedName("@type") val type: String,
        val reason: String? = null,
        val domain: String? = null,
        val metadata: Metadata? = null,
        val locale: String? = null,
        val localizedMessage: String? = null
    )

    data class Metadata(
        val service: String
    )

}