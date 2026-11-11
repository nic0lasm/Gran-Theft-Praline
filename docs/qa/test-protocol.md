# Test protocol of the 0.9 quarter build

*French version: [test-protocol.fr.md](test-protocol.fr.md).*

Audience: Nico and a few close people, on macOS (Apple silicon) and Windows.
The build is a work in progress: the goal is to find what blocks the game, not
to judge the final art.

## Before the session

1. Open the build following [test-build-instructions.md](test-build-instructions.md).
2. Note your machine: system, chip or processor, graphics card, memory.
3. Launch once and let the first-launch shaders finish.

## Session (about 1 hour)

| Time | What | Notes to take |
| ---- | ---- | ---- |
| 0-5 min | Free walk, look around, try the controls and remap one key | Anything confusing, anything missing |
| 5-15 min | Scenario A, then B | See below |
| 15-30 min | Scenario C and D | |
| 30-50 min | Free play: go anywhere in the quarter, by foot and by car, by day, night, rain | Stutters, falls, stuck places, odd visuals |
| 50-60 min | Quit through the menu; press F9 once before quitting | |

## Scenarios

Each scenario has a **pass** line. Write down pass, fail or blocked, and the
time on the in-game clock.

| # | Scenario | Pass when |
| ---- | ---- | ---- |
| A | **Find your way.** Open the full-screen map, read a real street name, set a destination, follow the GPS to it on foot | You arrive without leaving the GPS line by more than a few steps |
| B | **Rob the shop.** Start the activity, enter the shop without a loading screen, threaten the clerk, take the money, get out | Money added to your total, wanted level reacts, the game stays smooth |
| C | **Steal a car to order.** Start the activity, steal the named car, deliver it to the fictional garage | Reward paid; failing (car destroyed) lets you retry |
| D | **Lose the police.** Start the activity, cause trouble to reach 2 stars, reach the safe zone unseen | Stars fall to 0; if arrested, you respawn and can retry |
| E | **Save and load.** Save at the safehouse, quit, relaunch, choose Continue | Position, money and time of day restored |
| F | **Day, night, rain.** Let the clock run (or use the menu) through a night and a rain | Lights come on, no crash, no long freeze |
| G | **Stress.** Drive fast across the whole quarter, then turn around and cross it on foot | No loading screen, no frame stall over 1 s |

## What counts as a blocker

Crash, freeze over 5 s, a scenario that cannot be finished, a save that does
not load, falling under the world, getting stuck for good. Report within the
day. Everything else goes in the feedback form.

## After the session

1. Fill the [feedback form](feedback-form.md) (5 minutes).
2. File one bug per problem with the [bug template](bug-template.md), and
   attach the folder zipped as explained in the instructions (section 4).
3. Send the form and the bugs to Nico.
