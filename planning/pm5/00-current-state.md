# PM5 work — verified current state

**Fork-only.** Nothing in `planning/` is ever included in an upstream PR.

Baseline: `upstream/master` @ `f04ddf82e`, verified 2026-07-28. Everything below was read out of
the tree, not inferred from the issue text. Where an issue's premise turned out to be already
satisfied — or already partly built — it is called out, because it changes the cost.

## Issues in scope

| Issue | Title |
|---|---|
| [#4854](https://github.com/cagnulein/qdomyos-zwift/issues/4854) | Enhance PM5 proprietary protocol support in `ftmsrower` |
| [#4855](https://github.com/cagnulein/qdomyos-zwift/issues/4855) | Generate `trainrow` programs from Peloton rowing workouts |
| [#4856](https://github.com/cagnulein/qdomyos-zwift/issues/4856) | Download workouts to PM5 via CSAFE-over-BLE |
| [#4857](https://github.com/cagnulein/qdomyos-zwift/issues/4857) | Target-pace tile, sec/500m as the canonical rowing unit |

## What already exists (and what it means for scope)

### `ftmsrower` already parses five PM5 characteristics

`src/devices/ftmsrower/ftmsrower.cpp` handles `CE060031`, `CE060032`, `CE060033`, `CE060035`,
`CE060036` — subscribed together around line 303. So #4854 is **extending existing decode paths**,
not adding PM5 support from nothing. The remaining gaps are the ones the issue lists:
`0x0037`–`0x003B`, the force curve, and the multiplexed characteristic.

### The multiplexed characteristic is already implemented — in the SkiErg driver

`src/devices/concept2skierg/concept2skierg.cpp:146` parses `ce060080`, switching on the leading
type byte (`0x31`, `0x32`, `0x35`), and at line 304 deliberately subscribes to **only** the
multiplexed characteristic. That is a working reference for #4854 item 4, and the natural question
becomes whether the parsing should be *shared* between skierg and ftmsrower rather than written
twice.

### A CSAFE implementation already exists — and the encoder is transport-agnostic

`src/devices/csafe/` contains `csafe.{cpp,h}`, `csafeutility.{cpp,h}`, `csaferunner.{cpp,h}`,
`netserial.{cpp,h}`, `kalmanfilter.{cpp,h}`.

The important detail for #4856:

- `csafe::write(const QStringList &arguments, bool surround_msg = false)` builds CSAFE frames from
  a command list, and already carries a flag described as *"for wrapping the communication in CSAFE
  non-standard way for some devices like PM3"*. **This is a reusable frame encoder.**
- Only `csaferunner` is bound to transport — it includes `serialhandler.h`.

So #4856 does **not** need a new CSAFE encoder. It needs: PM-specific opcodes added to
`csafeutility` (there are currently none — no `SETUSERCFG`, no `PM_SET_*`, no `TARGETPACE`), and a
BLE write/notify path onto `CE060021`/`CE060022`. That is a meaningfully smaller job than the issue
implies.

### `trainrow` already has the fields #4855/#4857 need

From `src/trainprogram.h`:

- `double speed`, `lower_speed`, `average_speed`, `upper_speed` — the last three annotated
  *"used for peloton"*.
- **`int8_t pace_intensity` already exists**, also annotated *"used for peloton"*.

**There is no `start_time` field.** #4857 asks whether `trainrow` should grow one; the answer from
the tree is that rows are sequenced by `duration`, so honouring Peloton's `offsets.start` means
either adding the field or emitting a filler row for any gap. That is a real design decision, and
it is the one place where #4855 and #4856 could disagree about the timeline.

### Peloton's ROWING branches already exist

`src/peloton.cpp` — ROWING appears at lines 1240, 1392, 1470, 2019, 2299. Lines 1392 and 2299 are
the branches that populate `rower_pace[]`; 1240, 1470 and 2019 are the exclusions that stop
`trainrow` generation. So #4855's premise holds: the data is parsed and then dropped on the floor.

## Dependency order

These are not four independent tasks.

```
#4857 (sec/500m as the canonical unit + data model)
   │
   ├──> #4855 (Peloton rowing -> trainrow)      needs the unit decision
   │        │
   │        └──> #4856 (CSAFE download to PM5)  needs trainrows to exist
   │
#4854 (read more PM5 data)  ── independent, no dependency either way
```

#4857 is not really a UI task — its *data model* half (what unit `speed` carries when
`deviceType() == ROWING`) is a decision that #4855 and #4856 both encode. Deciding it late means
reworking both.

## Recommended sequence

1. **#4854 first.** Independent, read-only, no protocol writes to the machine, and it is the
   fastest way to get a validated PM5 bench/hardware loop running. It also produces the decoded
   telemetry we will want as *evidence* when verifying everything after it.
2. **#4857's data model** — decide and document what `speed`/`lower_speed`/`upper_speed` mean for
   ROWING, and settle the `start_time` question. Cheap, and it unblocks the rest.
3. **#4855** — Peloton rowing → `trainrow`, using that model.
4. **#4857's tile** — display, once there is something to display.
5. **#4856 last** — the only one that *writes* to the machine, and the only one that can put the
   PM5 into a state the rider did not ask for. It should be built on top of everything already
   proven.

## Risk notes

- **#4856 writes to hardware.** Everything else is read-only. It deserves the most caution, an
  explicit opt-in setting (the issue already proposes default-off), and a tested way to get the PM5
  back to a known state.
- **The PM5 cannot control resistance.** #4856 sets a *displayed* target only. Worth keeping
  explicit in the UI so the feature is not oversold.
- **Force curve reassembly (#4854 item 3)** is the one genuinely fiddly decode: 2–288 bytes per
  stroke, chunked, with a sequence byte. It needs real capture data to get right and should not be
  attempted from the spec alone.
- **Issue #4854 mentions `CE060041`** for force curve in its proposed scope while the table lists
  `0x003D`. One of those is wrong; resolve against a real capture before writing the parser.

## Open questions needing the actual PM5

- Firmware version, and whether it supports the CSAFE workout-download commands.
- Whether the machine advertises/exposes the multiplexed characteristic alongside the individual
  ones, or instead of them.
- Which characteristics actually notify in practice versus merely being listed.
- Real force-curve frames, to settle the `0x003D` vs `CE060041` question.
