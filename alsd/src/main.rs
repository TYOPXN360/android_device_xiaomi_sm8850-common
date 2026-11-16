//
// Copyright (C) 2026 Alex Zorzi <info@alexzorzi.it>
// SPDX-License-Identifier: Apache-2.0
//

//! Polls the panel backlight and asks citsensorservice to run its CWB/lux pass.
//!
//! The HAL converts the ALS channels to lux only inside startCalculateLux, which
//! nothing in AOSP ever calls. Stock used to drive it from the HyperOS display
//! service; on this build the only writer that exists is this daemon, so it
//! mirrors the display's brightness into the HAL and triggers a dump on the same
//! cadence the display composer uses.
//!
//! Every call is logged with its binder status. The C++ version returned
//! silently when a transaction failed, which made a dead service look like a
//! working one.

use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::thread::sleep;
use std::time::Duration;

use log::{error, info, warn};

use vendor_xiaomi_sensor_citsensorservice::aidl::vendor::xiaomi::sensor::citsensorservice::ICitSensorService::ICitSensorService;

const SERVICE_NAME: &str =
    "vendor.xiaomi.sensor.citsensorservice.ICitSensorService/default";

const BACKLIGHT_PANEL0: &str = "/sys/class/backlight/panel0-backlight/brightness";
const BACKLIGHT_PANEL1: &str = "/sys/class/backlight/panel1-backlight/brightness";

/// How often to trigger a dump while the screen is lit.
const PERIOD_MS: u64 = 300;

/// Backoff when the screen is off or the HAL is not up yet.
const IDLE_PERIOD_MS: u64 = 1500;

/// Reads the current backlight value, or None when the file cannot be read.
fn read_backlight(file: &mut Option<File>) -> Option<i32> {
    let f = file.as_mut()?;
    let mut buf = [0u8; 16];
    f.seek(SeekFrom::Start(0)).ok()?;
    let n = f.read(&mut buf).ok()?;
    if n == 0 {
        return None;
    }
    std::str::from_utf8(&buf[..n])
        .ok()?
        .trim()
        .parse::<i32>()
        .ok()
}

fn open_backlight(path: &str) -> Option<File> {
    match File::open(path) {
        Ok(f) => Some(f),
        Err(e) => {
            info!("{path} not available: {e}");
            None
        }
    }
}

/// Blocks until the HAL is registered, then returns a proxy to it.
fn connect() -> binder::Strong<dyn ICitSensorService> {
    loop {
        match binder::get_interface(SERVICE_NAME) {
            Ok(svc) => {
                info!("connected to {SERVICE_NAME}");
                return svc;
            }
            Err(e) => {
                warn!("{SERVICE_NAME} not available yet ({e:?}), retrying");
                sleep(Duration::from_millis(IDLE_PERIOD_MS));
            }
        }
    }
}

fn main() {
    android_logger::init_once(android_logger::Config::default().with_tag("alsd"));

    loop {
        let mut panel0 = open_backlight(BACKLIGHT_PANEL0);
        let mut panel1 = open_backlight(BACKLIGHT_PANEL1);

        if panel0.is_none() && panel1.is_none() {
            error!("no backlight node readable, retrying");
            sleep(Duration::from_millis(IDLE_PERIOD_MS));
            continue;
        }

        let service = connect();

        let mut last_dbv = -1i32;
        let mut last_display = -1i32;

        loop {
            // The panel that is actually driving the screen is the one with a
            // non-zero brightness; the other node stays at 0.
            let (display_id, dbv) = match (read_backlight(&mut panel0), read_backlight(&mut panel1)) {
                (Some(v0), _) if v0 > 0 => (0, v0),
                (_, Some(v1)) if v1 > 0 => (1, v1),
                _ => {
                    last_dbv = 0;
                    sleep(Duration::from_millis(IDLE_PERIOD_MS));
                    continue;
                }
            };

            // Brightness only has to be pushed when it changes; the dump has to
            // run every cycle or the lux stays frozen.
            if dbv != last_dbv || display_id != last_display {
                match service.setBrightness(dbv) {
                    Ok(ret) => {
                        info!("setBrightness dbv={dbv} ret={ret}");
                        last_dbv = dbv;
                        last_display = display_id;
                    }
                    Err(e) => {
                        warn!("setBrightness failed ({e:?}), reconnecting");
                        break;
                    }
                }
            }

            match service.triggerCwbDump(display_id, 0, true) {
                Ok(ret) => info!("triggerCwbDump display={display_id} dbv={dbv} ret={ret}"),
                Err(e) => {
                    warn!("triggerCwbDump failed ({e:?}), reconnecting");
                    break;
                }
            }

            sleep(Duration::from_millis(PERIOD_MS));
        }
    }
}
