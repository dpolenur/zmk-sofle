# Right display lock indicators

The left nice!view screen keeps its existing battery, Bluetooth profiles, WPM,
and layer layout. The right screen shows its battery and split connection status
above **CAPS** and **NUM** panels. Each panel displays **ON** (filled) or **OFF**
(outlined). **--** means the right half is disconnected or waiting for a status
report from the left half.

The indicators use the computer's HID output reports, rather than guessing state
from key presses. They follow the active host's Caps Lock and Num Lock state.
The host must send LED reports for this to work.

## Build and flash

Build with the existing GitHub Actions workflow and download its `firmware`
artifact. Flash **both halves** from the same build: the left needs HID reporting
and split forwarding enabled, while the right needs the receiver and display.
The keymap and left screen layout are unchanged.

## Implementation

`CONFIG_SOFLE_LOCK_STATUS` enables ZMK's host HID indicators and native peripheral
forwarding on both halves. The right half requests the current state after every
split reconnect, retrying once per second until it receives a reply. Requests
stop after synchronization. Ordinary lock changes use ZMK's native forwarding.
This also handles booting the right half after the host has already sent its
initial lock report.

Only the right build enables `CONFIG_SOFLE_RIGHT_LOCK_DISPLAY` and disables
`CONFIG_NICE_VIEW_WIDGET_STATUS`. The module reuses the nice!view rotation and
battery drawing helpers from the configured Cormoran ZMK fork. The settings-reset
build does not enable this module's features.

## Hardware checks

1. Confirm the left screen and all key bindings still behave as before.
2. Toggle Caps Lock and Num Lock independently; check all four combinations.
3. Change lock states using another keyboard on the same computer; verify these
   indicators follow the host state too.
4. With a lock enabled, power-cycle only the right half. It should show `--`
   until it receives the current state, then recover without pressing a lock key.
5. Disconnect the halves; the right screen should show `--`, not stale ON/OFF.
6. Repeat over USB and Bluetooth, including switching Bluetooth profiles.
