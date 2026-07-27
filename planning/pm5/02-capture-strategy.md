# PM5 work — capture and validation strategy

**Fork-only.** How we get ground truth, using hardware already on hand.

The current PM5 decode in `ftmsrower` is byte-offset magic numbers credited to *"go-row
implementation"* — not to the official Concept2 spec. Some of those offsets may be right, some may
be coincidentally right for one firmware, and `CE060033` is subscribed and hex-dumped without being
parsed at all. **We should not extend that by adding more offsets from a PDF.** Capture first,
decode against evidence — the same discipline the SB20 work runs on.

## Available kit

nRF52840 USB sniffer dongles · nRF52840 boards · ANT+ sticks · ESP32 boards · the X270 (BlueZ,
Wireshark, the `sb20-power-proxy` capture tooling) · **a real PM5 in the pain cave**.

## Two capture paths, and when each is right

### A · Passive BLE sniff — nRF52840 dongle + Wireshark extcap

The primary tool. Captures the PM5 ↔ central conversation without either side knowing, which is the
only way to see what a *known-good* app does.

`sb20-power-proxy` already has this working end to end: `code/scripts/sniff_ble.py` drives Nordic's
SnifferAPI over the dongle, and `code/findings/nrf-sniffer.md` documents the path. **Reuse it
rather than rebuild it** — session 9 of that project lost hours to exactly that mistake.

The highest-value capture is **PM5 ↔ Concept2's own ErgData app**, especially for #4856: it shows a
known-correct CSAFE workout download, frame for frame. That is far better evidence than the CSAFE
PDF, because it shows what the machine actually accepts, including any ordering or timing the spec
glosses over.

### B · Active probe — QZ itself, or a small BLE central

For enumerating what the PM5 exposes and what actually notifies, a central that connects and
subscribes is more direct than sniffing. Options in increasing order of effort: `bleak` from the
X270 (fastest, already in the `sb20-power-proxy` venv), QZ with debug logging, or an ESP32/nRF
board if we need something standalone in the pain cave.

**Ordering caution learned the hard way:** a BLE peripheral stops advertising once connected, and
two centrals cannot both own the PM5. Decide before each run whether QZ or the probe holds the
connection. This is exactly the deadlock that blocked G2 in `sb20-power-proxy` session 13.

## What each issue needs, and which path gets it

| Need | Path | Blocks |
|---|---|---|
| Full characteristic/service enumeration; which actually notify | B | #4854 |
| PM5 firmware + hardware version (does it support the CSAFE workout commands?) | B — C2 Device Info service `0x0010` | #4856 |
| Whether `ce060080` is offered alongside or instead of the individual chars | B | #4854 D4 |
| Ground-truth frames for `0x0031` (drag factor) and `0x0035` (drive metrics) | A or B | #4854 |
| Real force-curve frames — settles `0x003D` vs `CE060041` | A or B | #4854 |
| **A known-good CSAFE workout download (ErgData → PM5)** | **A — this is the big one** | #4856 |
| Whether the PM5 accepts our CSAFE frames, and its responses | A, while QZ writes | #4856 |

## Validating the existing decode

Worth doing early and cheap: capture a rowing piece, then check the shipped offsets against the
spec-derived layout for the same frames. Any offset that disagrees is either a bug we should fix in
#4854 or a firmware difference we should know about. Either way it is a finding, and it tells us how
much to trust the rest of the existing decode.

## D2 — the Peloton pace notation question

This one is **not** a BLE problem and needs no hardware. It is answered from the Peloton API plus
the app's own display:

1. Fetch `target_metrics_data` for a real rowing class (QZ already makes this call — the JSON can be
   dumped from the existing ROWING branch in `peloton.cpp:1392`).
2. Take one `pace_intensities[].levels[].fast_pace` value.
3. Compare against what the Peloton app shows as the target for that level in that class.

If the app shows `3:20` for `2.80`, the existing MM.SS reading is right. If it shows `2:48`, decimal
minutes is right and `rowerpaceToSpeed()` has a latent bug worth reporting separately.

**Do this before writing any pace conversion.** It is a single API call and one screenshot, and it
determines whether every target in #4855, #4857 and #4856 is right or 30-odd seconds out.

## Where captures live

Captures that ground a decode are evidence and should be committed, per the same rule
`sb20-power-proxy` uses: raw capture is canonical and never edited, findings derived from it.

Because upstream will not want our capture files, they belong in **`sb20-power-proxy`** (which
already has `code/findings/captures/` and the tooling), not in the qz fork. The qz fork's
`planning/pm5/` holds the *conclusions* and links to them.

## Safety note for #4856

#4856 is the only work that **writes** to the machine. Before the first write:

- Know how to return the PM5 to a known state (menu reset / power cycle) and write it down.
- Capture the full exchange, including the PM5's CSAFE responses — errors are as informative as
  successes.
- Keep the setting default-off, as the issue already proposes.
