# Animation sources beyond locomotion

*French version: [sources.fr.md](sources.fr.md)* · Inventory of 2026-11-10, free first, nothing bought.

The Game Animation Sample (GASP) covers walking, running, jumping, crouching and
traversal. Version 0.4 to 0.7 also need: entering and leaving a car, sitting at the
wheel, hit reactions, ragdoll get-ups, and later combat and cover. Status of each
source (**checked** = read on its page on 2026-11-10; **to check** = seen in a
search result or known from memory, the licence and price must be read on the page
before use).

| Source | Content | Licence and cost | Status |
| --- | --- | --- | --- |
| **Game Animation Sample** (Epic, Fab) | locomotion; a 5.8 post says a character can go ragdoll then play motion-matched get-ups | free, Epic content: usable in Unreal projects, even commercial, **never in the public repo** (kept local, as today) | the 5.8 ragdoll and get-up claim comes from a search snippet; the Epic page itself could not be read (403). **To check** by opening the 5.8 sample on the Mac |
| **Quaternius Universal Animation Library 1 and 2** | about 120 and 130 clips on a universal humanoid rig: sitting, death, crawl, push, swim, locomotion | **CC0**, free (a paid Pro tier exists, not needed) | **checked**. No car clips. Needs an IK Retargeter to the UEFN mannequin; CC0 means the retargeted clips could even go in Git, but we keep them out with the other content |
| **Car Interaction Animset Vol 01** (Fab, listing 55c80a94-955e-468b-904f-9df6155be5dd) | car entry and exit, driving, combat in a car, native UE5 Manny skeleton, no animation blueprint | price checked by Nico 2026-11-10: **too expensive, not bought** | rejected; fallback = hand-keyed poses |
| **Open-world civilian pack** (Fab, listing d97667b8-bf0f-43fa-b923-4e7802e4881b) | car interior and exterior interactions, bus, NPC everyday animations, FBX | price and licence **to check** | candidate for pedestrians and car interaction |
| **Mixamo** (Adobe) | thousands of mocap clips including falls, hit reactions, getting up, sitting, some driving poses | free with an Adobe account; community posts say commercial use in a game is allowed but **redistributing the raw files is not**; the licence page is reported removed | **to check**. If used: keep the FBX out of Git, retarget locally, note the source in assets/SOURCES.md |
| **Lyra Starter Game** (Epic) | locomotion, some hit reactions, shared with GASP's mannequin family | free, Epic content, local only | from memory, **to check** |
| **CMU Graphics Lab Motion Capture Database** | mocap of falls, getting up, sitting | free for any use, BVH files, needs conversion and cleanup | from memory, **to check**; heavy cleanup, last resort |

## Recommendation

1. **Ragdoll get-ups:** open the GASP 5.8 sample on the Mac first (10 minutes, no
   Unreal build needed beyond the editor). If the get-ups are in it, wire them to
   `GetUpFaceUp` and `GetUpFaceDown` of `UGTPRagdollComponent`; nothing to buy.
2. **Hit reactions and falls:** Quaternius clips plus the ragdoll for the heavy ones;
   Mixamo only if a clip is missing.
3. **Enter and leave the car:** look at the two Fab listings (price) before anything
   else; the fallback is three hand-keyed poses in Blender (reach, sit, close),
   about half a day, with the montage hooks already in `AGTPVehicle`
   (`EnterMontage`, `ExitMontage`, `CarjackMontage`, `BreakWindowMontage`).
4. **Combat and cover (0.7):** decide after steps 1 to 3, with the same method.

Every clip used goes into `assets/SOURCES.md` with its source and licence;
anything that is not CC0 or ours stays out of the public repo.

## Retargeting

GASP's mannequin is the UEFN one. Clips from another skeleton go through an IK Rig
and IK Retargeter asset (made once per source skeleton, in Content, not in Git
when it depends on third-party meshes), then are baked to new animation sequences.
