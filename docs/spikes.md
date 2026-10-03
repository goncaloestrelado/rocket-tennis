# Phase 1 — Feasibility spikes

Gate for Phase 1: every spike below is **pass** (or an explicit, written decision to drop/replace it) before Phase 2 starts.
Status values: `pending` · `implemented, unverified` · `pass` · `fail`.

> **Spikes A–D are proposed, not taken from CLAUDE.md** (it only says "1 Feasibility spikes"). I derived them from the
> riskiest assumptions in Phases 2–7. Rename or replace them if your Phase 1 list differs. Spike E is as you specified.

| ID | Question | Needed by | How it is tested | Status | Result / notes |
|----|----------|-----------|------------------|--------|----------------|
| A | Does the CI-built DLL load in Rocket League through BakkesMod (`plugin load RocketLeagueTennis`) and run `onLoad`? | all phases | Push, run the `build-plugin` workflow, download the DLL artifact, copy to `%appdata%\bakkesmod\bakkesmod\plugins\`, load in freeplay. Expect `[SpikeE] loaded` in the F6 console. | pending | CI workflow not yet run (written without access to a Windows runner). |
| B | Can the plugin run on the user's custom UDK map (scaled arena + net), not just stock freeplay maps? | 2, 3 | Load the map, `plugin load`, confirm Spike E rows are written there. | pending | Needs the hand-built map. |
| C | Can the plugin change the ball's color from code, per tick, by side? | 5 | Tiny test notifier that sets the ball color; then drive it from `side`. | pending | `[verify]` which SDK call/event applies to the ball's material. |
| D | Does the plugin run inside a Rocket Plugin–hosted match with two players (plugin type, ball access, tick hook)? | 7 | Host via Rocket Plugin, join with a second client, check Spike E log on the host. | pending | `[verify]` `PLUGINTYPE_FREEPLAY` (current) may not be enough for hosted matches. |
| E | Can the ball be tracked every tick (position, velocity, side) and can a floor bounce be detected from height ≈ radius + vertical-velocity reversal? | 4 | See Spike E below. | implemented, unverified | Detector unit-tested offline (`tests/bounce_test.cpp`); not yet run in the game. |

## Spike E — ball tracking and bounce detection

**What was built** (`plugin/RocketLeagueTennis.cpp`, `plugin/BounceDetector.h`):

- Every tick: read ball location, velocity and radius; write a CSV row. Columns:
  `wall_ms,dt_ms,tick,event,x,y,z,vx,vy,vz,side,height` where `height = z - radius` and `side = sign(Y)` (`+1`, `-1`, or `0` exactly on Y=0, per CLAUDE.md).
- Bounce rule: previous `vz < -min_vz`, current `vz > +min_vz`, and the **lower** of the two samples has `z - radius <= height_tol`.
  The lower sample is used as the contact estimate; the `bounce` row and the console line carry its position and side.
  The console line also prints the side of both samples, so a bounce next to the net is visible.
- Output: `%appdata%\bakkesmod\bakkesmod\data\RocketLeagueTennis\spike_e_<UTC timestamp>.csv` (one file per plugin load), and one console line per bounce.
- Cvars (F6 console): `rlt_spike_e_enabled` (1), `rlt_spike_e_echo_ticks` (0, prints every tick row to the console), `rlt_spike_e_height_tol` (12 uu), `rlt_spike_e_min_vz` (10 uu/s).

**Tested offline** (`tests/bounce_test.cpp`, built and run by CI before the DLL build):
a simulated ball (gravity −650 uu/s², radius 91.25 uu) is detected exactly once per floor hit at 60 and 120 Hz for restitution 0.6 and 0.8 and several drop heights;
side follows the contact sample; apex, mid-air reversals (e.g. a car hit), jitter below `min_vz`, and samples across a >250 ms gap are all rejected.
Micro-bounces whose speed is below `min_vz` (a ball settling with restitution 0.3, after ~5 hits) are deliberately not counted.

**Open items — all `[verify]`, to be settled by the first in-game run:**

1. **Tick source.** The hook is `Function Engine.GameViewportClient.Tick`, a render-frame tick. It may not be the 120 Hz physics tick. Read `dt_ms` in the CSV: ~8.3 means physics rate, ~16.7 or irregular means frame rate. If it is frame rate, switch `kTickEvent` to a physics-rate event (candidate: `Function TAGame.Car_TA.SetVehicleInput`, fires per car).
2. **Radius and floor height.** Confirm `BallWrapper::GetRadius()` ≈ 91 uu and that a ball resting on the floor logs `height ≈ 0` (if not, `height_tol` needs recalibrating).
3. **Tolerance.** Check that real bounces register at `height_tol = 12`; look at the `height` column on bounce rows to tune it.
4. **Car false positives.** A car popping a ball upward near the floor also reverses `vz` and will log as a "bounce". Spike E cannot tell the two apart. Distinguishing them (e.g. via the ball's last-hit time) belongs to Phase 4, not here.
5. **Replays and countdowns.** Goal replays and kickoff countdowns may still produce ticks; not yet checked.
6. **Wall bounces** never match (height check), but a ball rolling up a wall/ceiling transition has not been tested.

**How to run it:** build via CI → copy `RocketLeagueTennis.dll` to the BakkesMod `plugins` folder → in freeplay open F6 and run `plugin load RocketLeagueTennis` → hit the ball around, bounce it on the floor on both sides of Y=0 → inspect the CSV and console. Then fill in the Result column above.

**Pass criteria for E:** (1) a row is written for every tick while the ball exists, with plausible position/velocity; (2) every deliberate floor bounce yields exactly one `bounce` row with the correct `side`; (3) no `bounce` rows from wall hits or mid-air touches; (4) tick rate understood (open item 1).
