\# Rocket League Tennis



Custom RL game mode: scaled-down arena, net across the midfield line,

floor ends the point on the ball's first touch (dropshot-style), ball

color changes by side, two variants (open / car-blocking wall).



\## Architecture

\- Map (UDK, built by hand by the user): arena, net, wall variant.

\- Plugin (BakkesMod C++, built here): bounce detection, scoring,

&#x20; ball color by side, serve, haymaker.

\- Hosting through Rocket Plugin. Both players have haymaker.



\## Rules

\- Floor touch by the ball ends the point; the touched half's owner loses.

\- Only the ball counts, not cars. Wall bounces never score.

\- Net runs along Y=0; side = sign of ball Y.



\## Working rules

\- Work phase by phase. Do not start a phase until the previous gate passes.

\- Mark anything uncertain about the BakkesMod SDK or UDK as \[verify]

&#x20; and write a small test for it instead of assuming.

\- Log every spike result in docs/spikes.md (pass/fail + notes).

\- Commit after each passed gate.



\## Phases

0 Setup · 1 Feasibility spikes · 2 Arena scale · 3 Net + variants ·

4 Floor + scoring · 5 Ball color · 6 Rules/serve/haymaker ·

7 Hosting test · 8 Packaging

