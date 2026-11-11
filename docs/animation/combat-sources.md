# Combat animation sources (version 0.7)

*French version: [combat-sources.fr.md](combat-sources.fr.md)* · Inventory of 2026-11-11, free first, nothing bought · ADR-0011

Clips needed for the 0.7 fight, on the GASP character (UEFN / UE5 mannequin): fists (3 blows, heavy third),
guard, block, dodge (4 directions), takedown (attacker and victim), pistol (draw, holster, idle aimed, aim
offset, fire additive, reload, dry click), cover (low and high idle, move along, lean left and right, pop up,
blind fire, enter, leave), hit reactions and deaths (the ragdoll thread's `AM_HitLight_*`, `AM_Stagger_*`),
hands up (done by hand, `tools/animation/car_poses.json`).

**Checked** = read on a page on 2026-11-11 (search result or listing); **to check** = known from memory or
seen without the licence and the clip list; open the page before any download.

| Source | What it would cover | Licence and cost | Status |
| --- | --- | --- | --- |
| **Lyra Starter Game** (Epic, Fab) | pistol, rifle and shotgun on the UE5 mannequin: aim offsets, fire, reload, equip, melee montage per weapon, hit reactions, deaths | free, Epic content: usable in Unreal projects, **never in the public repo** | free and Epic: **checked** (Epic documentation); clip list from a third-party write-up (melee montage, weapon system): **to check** in the project. Same skeleton family as GASP: easiest retarget. **First choice for the pistol** |
| **Quaternius Universal Animation Library 2** | "melee and armed combos", parkour, about 130 clips on a universal humanoid rig | **CC0**, free, direct download (no account) | licence and theme **checked**; exact clips **to check** in the online viewer (quaternius.com/animviewer.html). **First choice for fists and dodges** |
| **Quaternius Universal Animation Library 1** | about 120 clips: deaths, locomotion; a write-up mentions guns | **CC0**, free | licence **checked** (already in docs/animation/sources.md); gun clips **to check** |
| **Animation Starter Pack** (Epic, UE4 mannequin) | rifle stance set: aim, fire, reload, crouch, hit reactions, deaths | free Epic content (from memory) | **to check** (still on Fab or only in the launcher vault) |
| **Paragon characters** (Epic) | stylised pistol heroes, full sets | free Epic content (from memory), heavy downloads | **to check**; stylised, last resort for the pistol |
| Fab "Free Animation Pack" (UE5 mannequin) | idle, walk, turns | free for prototypes and commercial use (listing) | **checked**: no combat clip, not useful here |
| GameDevHero unarmed sample (108 clips) | punches, kicks, dodges | free only after joining a Discord | **rejected**: needs an account |
| **Mixamo** (Adobe) | punches, dodges, pistol, cover, deaths | free with an Adobe account; raw files not redistributable | **to check**; needs an account: only if Nico opens one |
| Rapa Motion strikes, MandB unarmed, Mocap Online pistol starter | complete fist or pistol sets on the UE5 mannequin | paid (Rapa Motion strikes 39.99 $) or price not shown | not bought; only if a gap remains, with a card to Nico |

## Recommendation

1. **Pistol**: Lyra's pistol layer (aim offset, fire, reload, equip) retargeted to the GASP character. Asked to the
   free-downloads thread (no purchase, no account).
2. **Fists, guard, dodge**: Quaternius UAL 2 (CC0), retargeted with an IK Retargeter once for the universal rig
   (the same one serves UAL 1 for the ragdoll thread).
3. **Cover** (low and high idle, lean, pop up, blind fire): no free set found; **hand-keyed poses** with the method of
   `tools/animation/` (keyed bones in JSON, built in Blender, imported as montages), about half a day, done in the cloud
   up to the import.
4. **Takedown**: two hand-keyed clips (attacker, victim), the victim then goes ragdoll (ragdoll thread).
5. Hit reactions and deaths stay with the ragdoll thread (its clips and the ragdoll).

Every clip used goes into `assets/SOURCES.md` with its source and licence; Epic and Mixamo content stays out of Git.
