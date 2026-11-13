/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.display

import android.os.Bundle
import android.util.Log
import androidx.preference.Preference
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment
import com.xiaomi.settings.R

class DisplayEnhanceSettingsFragment :
    SettingsBasePreferenceFragment(), Preference.OnPreferenceChangeListener {

    companion object {
        private const val TAG = "DisplayEnhanceFragment"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)
    }

    private val switchBar by lazy {
        findPreference<MainSwitchPreference>(DisplayEnhanceController.SETTING_KEY)!!
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        if (DEBUG) Log.d(TAG, "onCreatePreferences")
        setPreferencesFromResource(R.xml.settings_display_enhance, rootKey)

        switchBar.isChecked = DisplayEnhanceController.isEnabled(requireContext())
        switchBar.onPreferenceChangeListener = this
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        if (preference == switchBar) {
            val isChecked = newValue as Boolean
            if (DEBUG) Log.d(TAG, "onPreferenceChange: $isChecked")
            DisplayEnhanceController.setEnabled(requireContext(), isChecked)
        }
        return true
    }
}
