# PM5 work — delivery plan

**Fork-only.** How the four issues get built, sequenced and shipped upstream.

## Shape

Six upstream PRs, each independently reviewable and independently useful. cagnulein reviews in
small pieces — #4850/#4851 showed that, and #4852 (one file) merged within the hour while the larger
ones are still in review. **Optimise for reviewability over convenience.**

Nothing in `planning/` ever goes upstream. PR branches are cut fresh from `upstream/master`, never
from `planning/concept2-pm5`.

---

## Phase 0 · Ground truth (no upstream PR)

**Gate for everything else.** Cheap, and it is what stops us extending a decode we have not
validated.

- **P0.1** — Enumerate the PM5: services, characteristics, which notify, firmware/hardware version.
- **P0.2** — Capture a rowing piece; validate the existing shipped offsets against the spec layout.
- **P0.3** — Settle the force curve: `0x003D` or `CE060041`.
- **P0.4** — **Settle D2** (Peloton pace notation). No hardware needed, blocks three issues.
- **P0.5** — If ErgData is available, capture a CSAFE workout download. Blocks #4856 only, so it can
  come later, but it is the single most valuable capture we can take.

Exit: we know what the machine does, and D2 is answered. Captures committed to `sb20-power-proxy`.

---

## Phase 1 · #4854 — read more PM5 data

Independent of the other three. Read-only, so it cannot put the machine in a bad state, and it
builds the hardware loop we will lean on later.

- **PR 1 — decode fixes + drag factor.** Any offset errors found in P0.2, plus drag factor from
  `0x0031` and the drive metrics from `0x0035` (drive time, recovery time, peak/average force).
  Put the decode in **free functions with no device state** so they are unit-testable and so D4's
  extraction stays a move rather than a rewrite. Add tests under `tst/` with captured frames as
  fixtures.
- **PR 2 — multiplexed `ce060080`.** Mirrors `concept2skierg.cpp:146`. Ask in the PR whether
  cagnulein wants the SkiErg switched to the shared parser — he can test that machine; we cannot.
- **PR 3 — force curve.** Chunked reassembly (2–288 bytes/stroke, sequence byte). Only attempt with
  real frames in hand. Expose a per-stroke peak force first; a visualisation is a separate,
  larger conversation.

`0x0037`–`0x003B` (split/interval and summary) are deliberately **deferred** — they are the least
useful without a workout running, and they will be much easier to reason about after #4855 exists.

---

## Phase 2 · #4857 data model + #4855 Peloton rowing

- **PR 4 — Peloton rowing → `trainrow`.** Remove the ROWING exclusions (`peloton.cpp:1240`, `1470`,
  `2019`), walk `target_metrics[]`, use the **existing** `peloton_rower_level` setting and the
  **existing** `rowerpaceToSpeed()`, and fill `lower/average/upper_speed` per D1. Handle
  `offsets.start` per D3 (filler rows), and say so explicitly in the PR body so the maintainer can
  object to the approach early rather than after the code exists.
- **PR 5 — target-pace tile.** Display sec/500m, converted at the boundary per D1. Depends on PR 4
  for something to show.

---

## Phase 3 · #4856 — CSAFE-over-BLE download

Last, because it is the only one that writes to the machine and the only one that can leave the PM5
in a state the rider did not ask for.

- **PR 6 — PM CSAFE opcodes + BLE transport + download.** Add `SETUSERCFG1` / `PM_SET_*` /
  `TARGETPACETIME` to `csafeutility`, reusing `csafe::write()` for framing. Add the BLE path on
  `CE060021` (write) / `CE060022` (notify). Setting default-off.

This may well want splitting again once P0.5 shows what a real download looks like — opcodes and
transport are separable, and a 3-file PR reviews better than a 10-file one.

---

## Gates

Nothing moves to the next phase until:

| Gate | Condition |
|---|---|
| Phase 0 → 1 | Characteristics enumerated; existing offsets validated or corrected; D2 answered |
| Phase 1 → 2 | Decode PRs green in CI, verified against the real PM5, unit tests on captured frames |
| Phase 2 → 3 | Peloton rowing produces correct `trainrow`s for a real class, verified against the app's own displayed targets |
| Phase 3 ship | PM5 accepts the download; intervals match the class timeline; PM5 returns to a known state afterwards |

## Risks

- **D2 wrong** → every pace target is ~30 s/500m out and looks plausible. Mitigated by settling it
  in Phase 0 against the app's own display.
- **Extending an unvalidated decode.** The existing offsets come from a third-party Go
  implementation, not the spec. Mitigated by P0.2.
- **Refactoring the SkiErg blind.** We cannot test that machine. Mitigated by D4 — build in
  `ftmsrower`, offer extraction, let the maintainer decide.
- **PM5 firmware may not support the CSAFE workout commands.** Checked in P0.1 before any of Phase 3
  is built.
- **Scope.** Four issues is a lot; #4854 alone is genuinely useful and shippable. If the Peloton
  work stalls on D2 or on maintainer preference, Phase 1 still lands on its own.

## What is explicitly not in scope

Force-curve *visualisation* (as opposed to peak force), `0x0037`–`0x003B` split/summary parsing, and
any change to the SkiErg driver without the maintainer's agreement.
