"""Tell whether the Game Animation Sample already plays footstep sounds (headless).

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=report_gasp_foley.py

GASP animations may carry foley notifies that play their own steps; with our
UGTPFootstepComponent (ADR-0009) that would double every step. This lists the
animation sequences under /Game/Characters and /Game/Blueprints whose notifies
name foley or footsteps, and the sound assets under /Game/Audio, then writes
a JSON report (GTP_AUDIO_REPORT, default Saved/gasp_foley.json) and a line
"[Audio] GASP foley: ..." that the pass script reads. If foley is found, mute
its folder with MutedSoundFolders in [/Script/GranTheftPraline.GTPAudioSettings].
"""

import json
import os

import unreal

ANIM_ROOTS = ["/Game/Characters", "/Game/Blueprints"]
SOUND_ROOT = "/Game/Audio"
WORDS = ("foley", "footstep", "foot_step", "step")


def assets_under(root, class_name):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = registry.get_assets_by_path(root, recursive=True)
    return [a for a in found if str(a.asset_class_path.asset_name) == class_name]


def main():
    sequences_with_foley = []
    notify_classes = {}
    for root in ANIM_ROOTS:
        for data in assets_under(root, "AnimSequence"):
            seq = unreal.load_asset(str(data.package_name))
            if not seq:
                continue
            try:
                events = unreal.AnimationLibrary.get_animation_notify_events(seq)
            except Exception:  # noqa: BLE001 - API differences between engine versions
                events = []
            hits = 0
            for event in events:
                notify = event.get_editor_property("notify")
                name = (notify.get_class().get_name() if notify else str(event.get_editor_property("notify_name"))).lower()
                if any(w in name for w in WORDS):
                    hits += 1
                    notify_classes[name] = notify_classes.get(name, 0) + 1
            if hits:
                sequences_with_foley.append(str(data.package_name))

    sound_folders = {}
    for class_name in ("SoundWave", "SoundCue", "MetaSoundSource"):
        for data in assets_under(SOUND_ROOT, class_name):
            folder = str(data.package_path)
            sound_folders[folder] = sound_folders.get(folder, 0) + 1

    report = {
        "sequences_with_foley": len(sequences_with_foley),
        "examples": sequences_with_foley[:10],
        "notify_classes": notify_classes,
        "sound_folders": sound_folders,
    }
    out = os.environ.get("GTP_AUDIO_REPORT") or os.path.join(unreal.Paths.project_saved_dir(), "gasp_foley.json")
    with open(out, "w") as f:
        json.dump(report, f, indent=2)
    unreal.log_warning("[Audio] GASP foley: %d sequences with foley notifies, %d sound folders under %s; report %s"
                       % (len(sequences_with_foley), len(sound_folders), SOUND_ROOT, out))


main()
