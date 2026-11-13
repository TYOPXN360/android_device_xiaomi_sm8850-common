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
 * carried over as persistent properties and the HAL polls them.
 *
 * Keys mirror the ones stock uses for the same page (battery_light and
 * notification_pulse), plus a master switch that stock does not have.
 */
object RearLightController {

    private const val TAG = "RearLight"

    private const val PROP_MASTER = "persist.sys.rear_light.enable"
    private const val PROP_BATTERY = "persist.sys.rear_light.battery"
    private const val PROP_NOTIFY = "persist.sys.rear_light.notify"

    /**
     * Bumped after every toggle. The light HAL blocks on this property with
     * __system_property_wait, so it is woken by the kernel instead of polling.
     */
    private const val PROP_GENERATION = "persist.sys.rear_light.gen"

    val KEYS = listOf("rear_light_enable", "rear_light_battery", "rear_light_notify")

    private fun propFor(key: String): String = when (key) {
        "rear_light_enable" -> PROP_MASTER
        "rear_light_battery" -> PROP_BATTERY
        "rear_light_notify" -> PROP_NOTIFY
        else -> throw IllegalArgumentException(key)
    }

    fun isEnabled(key: String): Boolean = getBool(propFor(key))

    fun setEnabled(key: String, enabled: Boolean) {
        val prop = propFor(key)
        val value = if (enabled) "1" else "0"
        try {
            val cls = Class.forName("android.os.SystemProperties")
            cls.getMethod("set", String::class.java, String::class.java)
                .invoke(null, prop, value)
            // Wake the HAL. It waits on this counter rather than polling, so
            // every toggle has to move it.
            cls.getMethod("set", String::class.java, String::class.java)
                .invoke(null, PROP_GENERATION, SystemClock.elapsedRealtime().toString())
            Log.i(TAG, "applied $prop=$value")
        } catch (t: Throwable) {
            Log.e(TAG, "failed to set $prop=$value", t)
        }
    }

    private fun getBool(prop: String): Boolean = try {
        val cls = Class.forName("android.os.SystemProperties")
        cls.getMethod("getBoolean", String::class.java, Boolean::class.javaPrimitiveType)
            .invoke(null, prop, true) as Boolean
    } catch (t: Throwable) {
        Log.e(TAG, "failed to read $prop", t)
        true
    }
}
