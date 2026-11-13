/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.display

import android.os.Bundle
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity
import com.android.settingslib.collapsingtoolbar.R

class DisplayEnhanceSettingsActivity : CollapsingToolbarBaseActivity() {

    companion object {
        private const val TAG = "DisplayEnhanceActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportFragmentManager
            .beginTransaction()
            .replace(R.id.content_frame, DisplayEnhanceSettingsFragment(), TAG)
            .commit()
    }
}
