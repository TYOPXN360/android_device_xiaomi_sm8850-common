/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.touch

import android.os.Bundle
import com.xiaomi.settings.PartsActivity
import com.android.settingslib.collapsingtoolbar.R

class TouchSettingsActivity : PartsActivity() {

    companion object {
        private const val TAG = "TouchSettingsActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportFragmentManager
            .beginTransaction()
            .replace(R.id.content_frame, TouchSettingsFragment(), TAG)
            .commit()
    }
}
