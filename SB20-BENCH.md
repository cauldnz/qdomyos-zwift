# SB20 handlebar-buttons — bench-test run-sheet (feat/sb20-obc, built on #4504)

Goal: build this branch on Linux, connect to a **real Stages SB20**, and verify the SB20's handlebar
buttons **(A) control qz internally** and **(B) rebroadcast out via OpenBikeControl to MyWhoosh**. Human +
AI: the AI builds/observes/fixes; the human presses the physical buttons and pedals. Tracks
[cagnulein/qdomyos-zwift#4785](https://github.com/cagnulein/qdomyos-zwift/issues/4785) and #4608.

> This branch is **#4504 (`codex/replace-mywhoosh-link-protocol`) + our SB20 changes**. `SB20-BENCH.md`,
> `build-linux-desktop.sh`, and `.github/workflows/ci-linux-desktop.yml` are fork-only bench aids.

## What the feature does

The SB20 exposes its 6 handlebar buttons on BLE vendor char `0c46be60-9c22-48ff-ae0e-c6eae1a2f4e5`.
`ftmsbike::characteristicChanged` decodes the `0x03` commit frame (150 ms debounce; bitmask one-hot:
bit0 L-up, bit1 L-down, bit2 L-3rd, bit3 R-up, bit4 R-down, bit5 R-3rd) and:

- **(A) drives qz internally** via each button's **configurable** action (`sb20DoAction`).
- **(B) rebroadcasts out via OpenBikeControl** through `MyWhooshLink` (`handleLeftUp/Down`,
  `handleRightY/A`, `handleGearUp/Down`) **when the MyWhoosh/OBC bridge is enabled** — so the SB20 buttons
  become usable in MyWhoosh (+ MQTT). The #4608 fix, for Stages bikes.

**Default internal mapping** (each button is rebindable — see Config):

| Button | bitmask | Default action | Setting key |
|---|---|---|---|
| LEFT up   | `0x0001` | `power_up`   (target power +10 W) | `sb20_button_left_up` |
| LEFT down | `0x0002` | `power_down` (target power −10 W) | `sb20_button_left_down` |
| LEFT 3rd  | `0x0004` | `gear_down` (virtual shifting)    | `sb20_button_left_3rd` |
| RIGHT up  | `0x0008` | `offset_up`   (peloton offset +1 s) | `sb20_button_right_up` |
| RIGHT down| `0x0010` | `offset_down` (peloton offset −1 s) | `sb20_button_right_down` |
| RIGHT 3rd | `0x0020` | `gear_up` (virtual shifting)       | `sb20_button_right_3rd` |

Action tokens: `power_up/down`, `offset_up/down`, `gear_up/down`, `resistance_up/down`, `zone_up/down`,
`lap`, `start_stop`, `none`. (Gears are a no-op in erg mode; an inclination offset in sim; a Zwift Play
gear command when `gears_zwift_ratio=true`.)

## 1 · Build

```bash
bash build-linux-desktop.sh      # apt deps + submodules + qthttpserver + qmake/make (~15 min)
```
If it fails with `bad interpreter: …^M`, run `sed -i 's/\r$//' build-linux-desktop.sh` first.
Then run capturing logs: `./src/qdomyos-zwift 2>&1 | tee /tmp/qz.log`.

## 2 · Test A — internal control (qz)

- Confirm `bluetoothctl list` shows a controller; launch qz; the SB20 (`Stages Bike ####`) connects as an
  **FTMS bike**; power/cadence stream. First run: set **Peloton login + FTP**.
- Human pedals + presses each button; AI watches `grep -i 0c46be60 /tmp/qz.log` for `… << 03 00 <bit> <bit>`.
- Confirm: LEFT up/down → **target power ±10 W** (T.Power tile + felt resistance); RIGHT up/down →
  **peloton offset ±1 s** (start a Peloton Power-Zone workout first); 3rd buttons → **gears** (best seen on
  a sim/Zwift-style ride).

## 3 · Test B — OpenBikeControl rebroadcast → MyWhoosh

- In qz **experimental settings**, enable the **MyWhoosh/OpenBikeControl link** (and, per #4608, you may
  need to disable the `dircon` setting so OBC is used for MyWhoosh).
- Run **MyWhoosh** on the PC (same LAN). It should discover the qz OBC bridge (mDNS
  `_openbikecontrol._tcp.local.`, port 21587).
- Human presses SB20 buttons; confirm they reach MyWhoosh (the shifter/3rd buttons drive
  `handleGearUp/Down` → MyWhoosh virtual shifting; paddles → `handleLeftUp/Down` / `handleRightY/A`).
  Cross-check qz's log / MQTT for the emitted button events.

## 4 · Config — rebind a button

Each button reads a QSettings key (see table). To remap, set e.g. `sb20_button_left_3rd = lap`. On
Linux qz stores settings under `~/.config/` (org "Roberto Viola" / app "QDomyos-Zwift") —
`qz's Settings UI` doesn't expose these yet (provisional; the maintainer will decide the final UI, #4785).

## 5 · If something's off (edit branch, rebuild, push)

- **Only `01` frames, no `03`:** switch the trigger to debounce the `01` held-stream instead of `0x03`.
- **MyWhoosh double-shifts / misses a press:** the OBC rebroadcast sends a momentary click
  (`handle*(true)` then `handle*(false)`) — adjust in the `(b)` block of `characteristicChanged`.
- **Wrong button→OBC mapping:** edit the `(b)` switch (`handleGearUp/Down`, `handleLeftUp/Down`,
  `handleRightY/A`).
- Rebuild `qmake && make -j"$(nproc)"`, re-test; push to `feat/sb20-obc` (triggers the fork CI).

## Boundaries
- Coordinate with the human for anything physical (button presses, pedalling, felt resistance, MyWhoosh).
- Stay on **`feat/sb20-obc`** — don't touch other branches or upstream.
- The full Peloton **ride** test is the human's, after the bench check passes.
