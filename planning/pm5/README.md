# Concept2 PM5 — planning

Fork-only working documents for upstream issues
[#4854](https://github.com/cagnulein/qdomyos-zwift/issues/4854),
[#4855](https://github.com/cagnulein/qdomyos-zwift/issues/4855),
[#4856](https://github.com/cagnulein/qdomyos-zwift/issues/4856) and
[#4857](https://github.com/cagnulein/qdomyos-zwift/issues/4857).

| Doc | What it covers |
|---|---|
| [`00-current-state.md`](00-current-state.md) | What the tree actually does today, verified against `upstream/master` — including the parts of these issues that are already built |
| [`01-decisions.md`](01-decisions.md) | The four decisions that gate implementation (D1 units, **D2 Peloton pace notation**, D3 `start_time`, D4 shared parser) |
| [`02-capture-strategy.md`](02-capture-strategy.md) | How we get ground truth with the nRF sniffers / ESP32 / PM5, and where captures live |
| [`03-delivery-plan.md`](03-delivery-plan.md) | Phases, six upstream PRs, gates, risks |

**Read `01-decisions.md` first.** D2 — whether Peloton's `fast_pace` is `MM.SS` or decimal minutes —
is worth ~30 s/500m on every target and blocks three of the four issues. It needs one API call and
one screenshot, no hardware.
