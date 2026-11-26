# Wireless Battery local revision 4 (based on v1.0.4)

Revision 4 only simplifies the controller's interior per the user's new reference:
four separate D-pad bars on the left and one larger ring on the right. Body geometry,
DPI sizing, colors and all battery/detection behavior remain as in revision 3.

## Keyboard support and hardware results

- Lofree Flow Lite84, receiver 05AC:024F with FF02:0002 and 17-byte reports:
  performs the official Control HUB identification exchange, online query and
  battery query. Checks command, status, keyboard flag, checksum, model IDs,
  percentage and charging range before accepting a reading. Real hardware
  returned 95%, then 100%, not charging, with a voltage reading. Idle receivers
  reporting offline do not replace the UI's last successful battery reading;
  the existing 30-minute cache applies.
- AJAZZ AK820 Pro, receiver 0C45:FDFD, interface 3, FF60:0061, 33-byte reports:
  verifies handshake 02 before requesting battery 20 01. The connected receiver
  returned a full identification reply and zero battery payload. Zero is not
  displayed as an empty battery. This receiver does not currently supply a
  usable percentage; radio battery support is therefore limited on this unit.
- AJAZZ USB, 0C45:8009/800A, FF13, 65-byte feature reports: sends the documented
  battery-only query and reads its reply. The user's unit returned FF in the
  battery field. That special wired marker is displayed as USB, never as an
  invented measured 100%. Actual numeric battery values from 1 to 100 are accepted.
- AJAZZ Bluetooth: Windows reported AK820 5.1-1 with battery property
  {104EA319-6EE2-4701-BD47-8DDBF425BBE5}, property 2, BYTE value 61. This is a real
  successful reading. The app normalizes the name and icon, reuses one stable
  keyboard key across USB/radio/Bluetooth, and prefers the live Bluetooth reading
  over an unknown receiver/USB entry. Flow Lite84 Bluetooth naming is normalized too.

The app does not change keyboard settings, pairing, lighting, firmware or keymaps.
Only identification, link status and battery queries are sent.

References:
- https://lofree.tech/home/ (official Control HUB bundle index-BTVblIUr.js, ha/v2/oi)
- https://github.com/gha-projects/ak820-mac/blob/main/docs/PROTOCOL.md
- https://github.com/Twyx22/ak820-center/blob/main/tools/ak820ctl.c

## Display changes

- Missed battery polls do not invent an asleep state. Cached values retain their
  normal appearance and are marked last known in the menu. A driver-reported sleep
  state still dims the device. Existing cache expiry remains in place.
- Taskbar icons/text use soft light/dark foreground shades and grayscale
  supersampling for smooth edges.
- The gamepad glyph has been replaced in the actual taskbar renderer with the
  user's requested outline: rounded body, four D-pad bars and a single large ring.
  It is rendered from vector geometry at the current DPI.
- Mouse, headset and controller battery protocols are unchanged.

## Validation

`make test`: production display/cache logic, both keyboard transports and protocols,
identity and checksum rejection, unknown values, USB/radio precedence, offline
handling, plus the original Rapoo protocol and driver tests. All pass.

`tests/keyboard_probe.cpp` performs only documented diagnostic queries;
`tests/keyboard_status.cpp` runs the production keyboard and Bluetooth pollers;
`tests/widget_preview.cpp` renders the production taskbar layout without attaching
to Explorer. Hardware output and preview PNGs are kept locally in build/.

## Local build

Built with w64devkit x64 2.11.0 and build.bat. Some MinGW headers duplicate the BYTE
and boolean IReference specialization. xinput.cpp now suppresses that unused duplicate
on MinGW, so no toolchain-header edits are needed. The application uses integer battery
references; controller polling behavior is unchanged. Public package version: 1.0.4.4.

The application and modifications remain GPL-3.0-or-later; see LICENSE.

## WLMouse support

- Ported the GPL-3.0-or-later WLMouse driver from upstream snapshot
  `notjiccu/wireless-device-battery-monitor@79705baa15412ad1f96bd7c9d3798b5d981c0d2a`.
- Added the contributor-enumerated Beast X 1K receiver `36A7:A882`, interface 2,
  usage `FFFF:0000`, 65-byte feature reports. Status query `02 02 00 83`
  returned a real battery percentage of 93, not charging.
- The driver validates command, status and percentage, retries echoed requests,
  preserves explicit sleep and gives receivers a 30-minute last-known grace period.
- Cable `A884` also claims the alternative 1K receiver `A882`. All Beast X
  transports (`A882` / `A883` / `A884`) share key `wlmouse:A882` and name
  `WLMouse Beast X`, so the UI updates the existing cached entry when charging
  instead of retaining a second, stale receiver entry. Driver and display tests
  cover receiver -> cable -> receiver transitions.
- Both build paths include the driver. `make test` includes a mocked-transport
  driver test; `tests/wlmouse_status.cpp` is the hardware diagnostic.
- Other models in the inherited table are not locally hardware-tested.
  Original Beast X 4K (`A887` / `A888`) uses a different protocol.
