# AGENTS.md

## How to use this file
* Skim once at the start of a session to align on global policies.
* Ask the user when instructions conflict or feel unsafe.

## Models
* Prefer older models. If unavailable, state active model and limitations.

## Agent conduct
* Verify assumptions before executing commands; call out uncertainties first.
* Ask for clarification when the request is ambiguous, destructive, or risky.
* Summarize intent before performing multi-step fixes so the user can redirect early.
* Cite the source when using documentation; quote exact lines instead of paraphrasing from memory.
* Break work into incremental steps and confirm each step with the smallest relevant check before moving on.

## Project Overview
This application will be built using PlatformIO.
Proto board is M5Stack Core2.
Framework is Arduino.
Temperature probe (Dallas DS18B20) is connected to Port A (on M5Stack Core2) as a 1-wire sensor. Data line is GPIO 32 (G32).
Every 5min:
- Temperature is measured.
- Device shall connect to wifi (credentials defined in secrets file).
- Once device is connected to wifi, device shall publish temperature measurements via MQTT (credentials defined in secrets file).
- Device shall be recognized as MQTT device by Home Assistant (using discovery feature).
- Temperature shall be measured even if wifi and/or MQTT connection was not successful.
- Display is inactive to preserve as much battery as possible.
- After publishing the data the device shall go into deep sleep to preserve as much battery as possible.
If user touches the screen or device is rebooted:
- Measured temperature is displayed on the screen using LVGL in format XX.X°C.
- Displayed value is centered on the screen using biggest available font.
- If temperature probe is not connected or any issue occurred N/A is displayed on the screen.
- After 30s the display goes to inactivity to preserve as much battery as possible.
Device shall log all the states into console and this option should be configurable as a build option.
If wifi connection was successful small green circle is displayed in the top left corner. Otherwise the small red circle is displayed.

## Environment
* Platform = espressif32
* Board = m5stack-core2
* Framework = arduino

## Project Structure
Use main.cpp to host the app.

## Code Style
Use Arduino project code style.

## State & living docs
Maintain:
* `README.md` - stable overview.

Refresh triggers: contradictions, omissions, flaky tests, or version uncertainty.

Refresh includes:
* `README.md`: purpose, architecture, stack with versions, run instructions, changelog-lite.
