# PM5 work — decisions to settle before writing code

**Fork-only.** Four decisions gate the implementation. Three of them are cheap to get wrong and
expensive to unpick, because #4855 and #4856 both encode them.

---

## D1 · What unit do rowing targets carry internally?

**#4857 proposes** making sec/500m the canonical unit and redefining `trainrow.speed` to mean pace
when `deviceType() == ROWING`.

**What the tree already does:** `peloton::rowerpaceToSpeed(double pace)` (`peloton.cpp:2409`)
already converts a Peloton pace into **km/h**, which is what `trainrow.speed` means everywhere else
and what every existing consumer of `trainrow` assumes.

```cpp
double peloton::rowerpaceToSpeed(double pace) {
    fractional = std::modf(pace, &whole);
    double seconds = whole * 60.0;
    seconds += (fractional * 100.0);
    seconds *= 2.0;              // per-500m -> per-1000m
    return 3600.0 / seconds;     // -> km/h
}
```

**Recommendation: keep km/h as the internal unit; convert to sec/500m only at the two edges that
need it — the target-pace tile and the CSAFE `SET_TARGETPACETIME` command.**

Reasoning: redefining the meaning of a shared struct field for one device type is a latent trap for
every current and future consumer of `trainrow` — `trainprogram`, the FIT writer, the UI, and
anything that averages or compares rows. It cannot be enforced by the type system and will be
silently wrong the first time someone forgets. A conversion helper at the boundary is one function
and cannot be misread.

This does not weaken #4857's actual user-facing goal: the rower still sees sec/500m everywhere, and
the CSAFE command still gets the exact units it wants.

**Cost if we choose wrong:** every consumer of `trainrow.speed` needs auditing. Do this first.

---

## D2 · ⚠️ What notation is Peloton's `fast_pace` / `slow_pace` actually in?

**This is the highest-risk unknown in the whole plan, and it is not resolvable from the code.**

Issue #4855 says these are *"minutes per 500 m"* and gives examples like `2.80`.

The existing helper reads them as **MM.SS**, i.e. whole part = minutes, fractional × 100 = seconds:

- `2.80` → `120 + 80` = **200 s** (3:20/500m)

But read as **decimal minutes**, the same value is:

- `2.80` → `2.80 × 60` = **168 s** (2:48/500m)

That is a **32-second-per-500m difference** — the gap between a recovery pace and a hard effort. If
we get it backwards, every target QZ shows and every pace pushed to the PM5 is wrong, and wrong in a
way that looks plausible.

Note also that MM.SS notation admits `2.80` = 2:80, i.e. 80 seconds, which is not a valid clock
value — mildly suggestive that decimal minutes is correct and the existing helper is wrong. **But
that is inference, not evidence**, and the existing helper is shipped code that presumably someone
tested.

**Resolution: capture a real Peloton rowing class's `target_metrics_data` JSON and compare a known
`pace_intensities` entry against what the Peloton app displays for the same class.** Do not write
the conversion until this is settled. See `02-capture-strategy.md`.

---

## D3 · How does a `trainrow` carry Peloton's `offsets.start`?

`trainrow` has **no `start_time` field**; rows are sequenced purely by `duration`. Peloton rowing
classes give absolute `offsets.start` / `offsets.end` in seconds from the video start, and #4856
needs the PM5's intervals to line up with that same timeline.

Options:

1. **Filler rows for gaps.** Emit a no-target row covering any gap between one interval's `end` and
   the next's `start`. No struct change; works with everything today. Downside: gaps become
   indistinguishable from genuine zero-target intervals, and any drift accumulates.
2. **Add `start_time` to `trainrow`.** Explicit and drift-free, and #4856 can map intervals
   directly. Downside: touches a shared struct, and needs a decision about what it means for the
   non-Peloton programs that will not set it.

**Recommendation: option 1 for #4855, and revisit before #4856.** It is the smaller change, it is
enough to ship Peloton rowing targets in QZ's own UI, and it avoids committing to a struct change
before the CSAFE work has told us what it actually needs. Flag it explicitly in the #4855 PR so the
maintainer can object early.

---

## D4 · Share the multiplexed `ce060080` parser, or duplicate it?

`concept2skierg.cpp:146` already parses the multiplexed characteristic, switching on the leading
type byte, and at line 304 deliberately subscribes to *only* `ce060080`.

Options: extract a shared PM5 parser used by both drivers, or write a second copy in `ftmsrower`.

**Recommendation: propose extraction to the maintainer, but do not assume it.** Shared parsing is
obviously better engineering, but it refactors a working, shipped driver that we cannot test — we
have a PM5 rower, not a SkiErg. Rewriting a device we cannot exercise is how you break someone
else's hardware from a distance.

Suggested approach: build it in `ftmsrower` first with the decode logic in a **separately testable
free function** (no device state), so extraction later is a move rather than a rewrite. Ask
cagnulein in the #4854 PR whether he wants the SkiErg switched over; he can test that machine.

---

## Already exists — do not rebuild

Corrections to the issues, verified in `upstream/master` @ `f04ddf82e`:

| Issue proposes | Reality |
|---|---|
| #4855: "add a `peloton_rower_level` setting" | **Already exists** — `qzsettings.h:2064`, `default_peloton_rower_level = 1` |
| #4855: convert Peloton pace to a speed field | **`rowerpaceToSpeed()` already exists** — `peloton.cpp:2409` |
| #4856: "add a CSAFE frame encoder" | **`csafe::write()` already exists** and is transport-agnostic; only `csaferunner` is serial-bound |
| #4856: PM-specific CSAFE opcodes | Genuinely absent — no `SETUSERCFG`, no `PM_SET_*`, no `TARGETPACE` in `csafeutility` |
| #4854: multiplexed `ce060080` parsing | **Already written in `concept2skierg.cpp:146`** |
| #4854: force curve on `CE060041` (scope) vs `0x003D` (table) | Self-contradictory; settle against a capture |
