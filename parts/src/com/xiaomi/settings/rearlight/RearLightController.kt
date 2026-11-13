/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.rearlight

import android.os.SystemClock
import android.util.Log

/**
 * The rear light is a single AW21024 driven LED shared by the battery, the
 * notification pulse and the camera/microphone indicators. The light HAL decides
 * which of them wins, and it cannot read Settings.System, so the switches are
 * carried over as properties and the HAL blocks on a generation counter rather
 * than polling.
 *
 * Keys mirror the ones stock uses for the same page (battery_light and
 * notification_pulse), plus a master switch and the colour and breathing period
 * overrides.
 */
object RearLightController {

    private const val TAG = "RearLight"

    private const val PROP_MASTER = "persist.sys.rear_light.enable"
    private const val PROP_BATTERY = "persist.sys.rear_light.battery"
    private const val PROP_NOTIFY = "persist.sys.rear_light.notify"
    private const val PROP_COLOR = "persist.sys.rear_light.color"
    private const val PROP_PERIOD = "persist.sys.rear_light.period"

    /**
     * Bumped after every change. The light HAL blocks on this property with
     * __system_property_wait, so it is woken by the kernel instead of polling.
     */
    private const val PROP_GENERATION = "persist.sys.rear_light.gen"

    /** Boolean switches, in the order they appear on the page. */
    val SWITCH_KEYS = listOf(
        "rear_light_enable",
        "rear_light_battery",
        "rear_light_notify",
    )

    /** Drop-down preferences. */
    val LIST_KEYS = listOf("rear_light_color", "rear_light_period")

    private fun propFor(key: String): String = when (key) {
        "rear_light_enable" -> PROP_MASTER
        "rear_light_battery" -> PROP_BATTERY
        "rear_light_notify" -> PROP_NOTIFY
        "rear_light_color" -> PROP_COLOR
        "rear_light_period" -> PROP_PERIOD
        else -> throw IllegalArgumentException(key)
    }

    fun isEnabled(key: String): Boolean =
        getProp(propFor(key), "1") == "1"

    fun getValue(key: String, default: String): String =
        getProp(propFor(key), default)

    fun setEnabled(key: String, enabled: Boolean) {
        setValue(key, if (enabled) "1" else "0")
    }

    fun setValue(key: String, value: String) {
        val prop = propFor(key)
        try {
            setProp(prop, value)
            // Wake the HAL. It waits on this counter, so every change has to
            // move it.
            setProp(PROP_GENERATION, SystemClock.elapsedRealtime().toString())
            Log.i(TAG, "applied $prop=$value")
        } catch (t: Throwable) {
            Log.e(TAG, "failed to set $prop=$value", t)
        }
    }

    private fun setProp(prop: String, value: String) {
        val cls = Class.forName("android.os.SystemProperties")
        cls.getMethod("set", String::class.java, String::class.java).invoke(null, prop, value)
    }

    private fun getProp(prop: String, default: String): String = try {
        val cls = Class.forName("android.os.SystemProperties")
        cls.getMethod("get", String::class.java, String::class.java)
            .invoke(null, prop, default) as String
    } catch (t: Throwable) {
        Log.e(TAG, "failed to read $prop", t)
        default
    }
}
