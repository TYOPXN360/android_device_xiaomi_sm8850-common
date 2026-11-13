/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.display

import android.content.Context
import android.os.UserHandle
import android.provider.Settings
import android.util.Log

/**
 * Owns the switch that feeds the Novatek visual display processor.
 *
 * The processor is driven by hal_visdisplaysrv_default, which takes its whole
 * configuration from /odm/etc/dqe/ASICScene.xml. The AIDL surface the service
 * exposes is only enableListener, disableListener and setNitsAndBrightness; the
 * scene API that actually selects MEMC, LSR, SDR2HDR and the 3D LUT entries
 * (setSceneMode, setSRMode, setMEMCMode...) is internal and is normally driven
 * by the HyperOS framework service, which this build does not carry.
 *
 * So the switch works one level up and starts or stops the HAL, which decides
 * whether the panel is fed the processed stream at all. With the service
 * stopped the processor keeps its power-on defaults and the panel is driven by
 * the display processor alone.
 */
object DisplayEnhanceController {

    private const val TAG = "DisplayEnhance"

    const val SETTING_KEY = "display_enhance_ai_enable"

    private const val PROP = "persist.sys.disp_enhance"

    fun isEnabled(context: Context): Boolean =
        Settings.System.getIntForUser(
            context.contentResolver,
            SETTING_KEY,
            1,
            UserHandle.USER_CURRENT,
        ) == 1

    fun setEnabled(context: Context, enabled: Boolean) {
        Settings.System.putIntForUser(
            context.contentResolver,
            SETTING_KEY,
            if (enabled) 1 else 0,
            UserHandle.USER_CURRENT,
        )
        apply(enabled)
    }

    /**
     * Writing the property is enough. init reacts to it through the triggers in
     * init.songyuan.rc, which start or stop the service, and because the
     * property persists the choice survives a reboot as well.
     */
    fun apply(enabled: Boolean) {
        val value = if (enabled) "1" else "0"
        try {
            val cls = Class.forName("android.os.SystemProperties")
            cls.getMethod("set", String::class.java, String::class.java)
                .invoke(null, PROP, value)
            Log.i(TAG, "applied $PROP=$value")
        } catch (t: Throwable) {
            Log.e(TAG, "failed to set $PROP=$value", t)
        }
    }
}
