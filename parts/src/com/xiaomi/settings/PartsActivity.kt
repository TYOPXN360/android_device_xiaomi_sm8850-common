/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings

import android.media.AudioManager
import android.view.KeyEvent
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity

/**
 * XiaomiParts is persistent and starts before the media session service, so
 * its cached MediaSessionManager has no service and the default volume key
 * handling in PhoneWindow crashes. Adjust the music volume directly.
 */
abstract class PartsActivity : CollapsingToolbarBaseActivity() {

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val direction = when (keyCode) {
            KeyEvent.KEYCODE_VOLUME_UP -> AudioManager.ADJUST_RAISE
            KeyEvent.KEYCODE_VOLUME_DOWN -> AudioManager.ADJUST_LOWER
            KeyEvent.KEYCODE_VOLUME_MUTE -> AudioManager.ADJUST_TOGGLE_MUTE
            else -> return super.onKeyDown(keyCode, event)
        }
        // adjustSuggestedStreamVolume() goes through the media session service too
        getSystemService(AudioManager::class.java).adjustStreamVolume(
            AudioManager.STREAM_MUSIC,
            direction,
            AudioManager.FLAG_SHOW_UI,
        )
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean = when (keyCode) {
        KeyEvent.KEYCODE_VOLUME_UP, KeyEvent.KEYCODE_VOLUME_DOWN,
        KeyEvent.KEYCODE_VOLUME_MUTE -> true
        else -> super.onKeyUp(keyCode, event)
    }
}
