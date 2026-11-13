/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.rearlight

import android.os.Bundle
import android.util.Log
import androidx.preference.DropDownPreference
import androidx.preference.ListPreference
import androidx.preference.Preference
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment
import com.xiaomi.settings.R

class RearLightSettingsFragment :
    SettingsBasePreferenceFragment(), Preference.OnPreferenceChangeListener {

    companion object {
        private const val TAG = "RearLightFragment"
        private val DEBUG = Log.isLoggable(TAG, Log.DEBUG)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        if (DEBUG) Log.d(TAG, "onCreatePreferences")
        setPreferencesFromResource(R.xml.settings_rear_light, rootKey)

        for (key in RearLightController.SWITCH_KEYS) {
            val pref = findPreference<MainSwitchPreference>(key) ?: continue
            pref.isChecked = RearLightController.isEnabled(key)
            pref.onPreferenceChangeListener = this
        }

        for (key in RearLightController.LIST_KEYS) {
            val pref = findPreference<ListPreference>(key) ?: continue
            val current = RearLightController.getValue(key, pref.value ?: "")
            pref.value = current
            pref.summary = entryLabel(pref, current)
            pref.onPreferenceChangeListener = this
        }
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        val key = preference.key ?: return true
        if (key in RearLightController.SWITCH_KEYS) {
            val isChecked = newValue as Boolean
            if (DEBUG) Log.d(TAG, "onPreferenceChange: $key=$isChecked")
            RearLightController.setEnabled(key, isChecked)
            return true
        }
        if (key in RearLightController.LIST_KEYS) {
            val value = newValue?.toString() ?: ""
            if (DEBUG) Log.d(TAG, "onPreferenceChange: $key=$value")
            RearLightController.setValue(key, value)
            (preference as? ListPreference)?.let { it.summary = entryLabel(it, value) }
            return true
        }
        return true
    }

    private fun entryLabel(pref: ListPreference, value: String): CharSequence? {
        val index = pref.entryValues.indexOfFirst { it.toString() == value }
        return if (index >= 0) pref.entries.getOrNull(index) else pref.summary
    }
}
