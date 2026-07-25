# SB20 handlebar-buttons — bench-test run-sheet (feat/sb20-buttons)

Goal: build this branch on Linux, connect to a **real Stages SB20**, and verify the SB20's
handlebar buttons control the app. This is a **human + AI** job — the AI builds/observes/fixes,
the human presses the physical buttons and pedals. Tracks upstream issue
[cagnulein/qdomyos-zwift#4785](https://github.com/cagnulein/qdomyos-zwift/issues/4785).

> These two files (`build-linux-desktop.sh`, `SB20-BENCH.md`) are **fork-only bench aids** — strip
> them from the eventual upstream PR (the PR is just the `ftmsbike` + `qzsettings` code change).

## What the feature does

The SB20 exposes its 6 handlebar buttons on a BLE **vendor characteristic**
`0c46be60-9c22-48ff-ae0e-c6eae1a2f4e5` (under vendor service `0c46be5f-…`). qz already binds the
SB20 to the **`ftmsbike`** driver and auto-subscribes every Notify characteristic, so those button
notifications already arrive in `ftmsbike::characteristicChanged`.

- **Payload:** `<type:u8> 00 <bitmask:u16 LE>`. The per-press **commit** frame is `type = 0x03`.
  (The `0x01` frames stream while a button is *held*; `0x04`/`0x08` are burst terminators.)
- **Bitmask (one-hot, which button):** bit0 LEFT-up, bit1 LEFT-down, bit2 LEFT-3rd,
  bit3 RIGHT-up, bit4 RIGHT-down, bit5 RIGHT-3rd.
- **Default mapping** (`src/devices/ftmsbike/ftmsbike.cpp`, search `0c46be60`):

  | Button | bitmask | Action |
  |---|---|---|
  | LEFT up   | `0x0001` | target power **+10 W** |
  | LEFT down | `0x0002` | target power **−10 W** |
  | LEFT 3rd  | `0x0004` | **gear down** (virtual shifting) |
  | RIGHT up  | `0x0008` | peloton offset **+1 s** |
  | RIGHT down| `0x0010` | peloton offset **−1 s** |
  | RIGHT 3rd | `0x0020` | **gear up** (virtual shifting) |

  Gears (`gearUp/gearDown`, `ftmsbike` is-a `bike`): a **no-op in erg/Peloton mode**; in **sim** mode
  they add an inclination offset (harder/easier climb); with **`gears_zwift_ratio=true`** they send a
  Zwift Play gear command (true Zwift virtual shifting). So test gears on a **sim/Zwift-style** ride,
  and power/offset on a **Peloton PZ** ride.

  Routed through the existing dispatcher: `homeform::singleton()->keyboardPlus/Minus("target_power"|"peloton_offset")`.
  150 ms debounce (`lastSb20ButtonPress`). Gated by setting `sb20_buttons_enabled` (**default ON**).

## 1 · Build

```bash
bash build-linux-desktop.sh      # apt deps + submodules + qthttpserver + qmake/make (~15 min)
```
If `qmake` errors on a missing Qt module, `apt install` the matching `-dev` / `qml-module-*` and retry.

## 2 · Bring up BLE + pair

- `bluetoothctl list` → a controller must appear. If not, the USB BT dongle needs re-passing-through
  to the VM (tell the human).
- Run with debug capture: `./src/qdomyos-zwift 2>&1 | tee /tmp/qz.log`
  (enable **Settings → debug log** in qz if the console is quiet).
- Human confirms the SB20 (`Stages Bike ####`) connects as an **FTMS bike** and power/cadence stream.

## 3 · Verify the buttons

Ask the human to pedal, then press each button while you watch `/tmp/qz.log`:

```bash
grep --line-buffered -i '0c46be60' /tmp/qz.log        # shifter notifications
```
- Each press should show a `… 0c46be60 … << 03 00 <bit> <bit>` line (plus a run of `01 00 <bit>`).
- **LEFT up/down** → the app's `T.Power` tile changes ±10 W and resistance steps (human confirms feel).
- **RIGHT up/down** → the `peloton_offset` moves ±1 s — start a **Peloton Power Zone** workout first.

Record, per button: the exact log bytes, whether the action fired, and what the human saw/felt.

## 4 · If something's off — you may edit the branch, rebuild, and (with human OK) push

- **Only `01` frames, no `03`:** switch the trigger to debounce the `01` held-stream instead of keying
  on `0x03` — collapse a run of identical `01 00 <bit>` into one action (reuse the 150 ms
  `lastSb20ButtonPress` guard). Edit the `characteristicChanged` block in `ftmsbike.cpp`.
- **Wrong control / backwards offset:** adjust the `switch (buttonMask)` cases (swap
  `keyboardPlus`↔`keyboardMinus`, or remap bits).
- **Nothing on `0c46be60` at all:** confirm the vendor service is discovered — grep the log for
  `0c46be5f` / `char uuid`. If qz never subscribes it, check the notify-subscription loop in
  `ftmsbike::stateChanged`.
- After any change: `qmake && make -j"$(nproc)"` and re-test. To push: `git push origin feat/sb20-buttons`
  (triggers the fork's hosted CI). Keep a terse changelog of what you changed and why.

## Boundaries

- Coordinate with the human for anything physical (button presses, pedalling, felt resistance).
- Stay on **this fork's `feat/sb20-buttons`** branch — don't touch other branches or upstream.
- The full Peloton **ride** test is the human's, after this bench check passes.
