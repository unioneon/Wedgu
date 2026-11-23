# Changelog

## 1.1.0 — Wedgu · 2026-11-23

Wireless device batteries, weather and air quality beside your Windows clock.

This is an independent fork of krisgfx9/Wireless-Device-Battery-Monitor v1.0.4,
now branded Wedgu. The original GPL-3.0-or-later attribution is retained.

## Added

- Open-Meteo weather without an API key, refreshed hourly, plus optional AccuWeather with a personal key.
- IQAir air-quality widgets with a personal API key; no empty widget when no key is configured.
- Lock-screen wallpaper widgets with size control, live preview and restoration of the original background.
- Optional system date, disabled by default, after the air-quality indicator.
- Battery polling settings from 1 minute to 1 hour.
- City search and on-demand Windows location detection instead of manual coordinates.
- Redesigned settings, shared color presets, HEX picker and screen eyedropper.
- Unified taskbar/tray menu, outline weather icons and new Wedgu branding.
- 13 UI languages, automatic system-language selection, instant language changes and Arabic right-to-left layout.
- About page linking to https://github.com/unioneon/ and crediting the upstream project.
- Regression tests for localization, edit preservation, slider painting, date and color updates, provider parsing and icon rendering.

## Preserved and fixed

- Windows-style device icons, custom controller icon, Lofree Flow Lite84 protocol support and AJAZZ Bluetooth identity fixes from the previous fork release.
- Missed battery polls do not falsely mark an active mouse asleep.
- Custom colors update taskbar, tray and lock-screen widgets consistently. Auto uses system foreground and a white lock-screen widget.
- Date toggles take effect immediately and update from the local system clock.
- Password fields, menus, toggles and sliders use consistent rendering.

## Packages

Use Setup for a normal Windows installation or extract the complete portable ZIP.
Portable stores settings in the Windows user profile. Source archives and SHA-256
checksums accompany this release. Personal keys and profiles are not included.
SF Pro Display is used only if already installed; no proprietary font is bundled.

## Known limitations

- The tested AJAZZ AK820 Pro receiver supplies no usable percentage; use Bluetooth. Its USB marker is not a measured 100%.
- Some sleeping devices need to be woken before reading their battery.
- Windows permissions/policies can restrict location and lock-screen wallpaper changes.
- Open-Meteo's free public endpoint is for non-commercial use. Provider terms and limits apply; IQAir and optional AccuWeather require user-supplied credentials.
- AQI icon thresholds are custom visual cues, not official health categories.

## 1.0.4.4 — Windows Style · 2026-11-20

First public release of this independent fork, based on [upstream v1.0.4](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/releases/tag/v1.0.4).

### Added

- Lofree Flow Lite84 2.4 GHz identification, online and battery protocol, including charging and voltage.
- AJAZZ AK820 Pro receiver handshake/battery queries, USB battery-marker handling and Bluetooth identity normalization.
- A stable keyboard identity across transports, preferring a live Bluetooth battery reading over an unknown receiver entry.
- A custom vector gamepad icon with a four-bar D-pad and one large ring.
- Keyboard and display-state regression tests, hardware diagnostics and a production widget preview renderer.
- Russian/English fork documentation, hardware verification table, upstream attribution and release packaging.

### Changed

- Softer foreground colors in both taskbar themes.
- Supersampled grayscale font masks for smoother small icons and text.
- A missed battery response no longer forces an `asleep` state or dims an otherwise known value.

### Known limitations

- The tested AJAZZ AK820 Pro receiver does not supply a usable numeric battery reading. Use Bluetooth for a percentage on that unit.
- The special AJAZZ USB `FF` marker is shown as `USB`; it is not treated as a measured 100%.
- Sleeping Lofree devices may require a keypress and refresh; cached readings are explicitly marked `last known`.
- Other device protocols and the limitations documented in the archived upstream README remain applicable.
