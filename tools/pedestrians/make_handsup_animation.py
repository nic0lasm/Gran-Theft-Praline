"""Make the hands-up loop of the pedestrians from their idle, on each skeleton they have (editor script, nothing is downloaded).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/pedestrians/make_handsup_animation.py" -unattended -nosplash -nullrhi

For each job (idle loop -> folder/name) the idle is duplicated and, on every frame, both arms are raised: upper arms up
and a little out, forearms nearly vertical, palms forward (the thumbs point at the head). No Epic pack we have holds a
hands-up animation (fab/manifeste-fab.csv), so the pose is made here; it is a held pose, not motion capture.

The pose is solved from the idle itself, in the space of the body: where the shoulder, the elbow, the wrist and the thumb
are on frame 0, which way each bone has to turn to point where it should. Nothing depends on the axes of a skeleton, so
the same code serves the UEFN mannequin, the MetaHuman skeleton and the one of the City Sample crowd, and the loop is
made on the skeleton that plays it (no retarget of it). The first version wrote fixed local angles with every position
key at zero: the upper arm lost its length, and the hands ended 10 to 19 cm under the head on the crowd's bodies
(-GTARun of candidate 49, "hands NOT over the head").

Default jobs (those whose idle is missing are skipped):
  /Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop -> /Game/Characters/Pedestrians/Anims/A_HandsUp
  /Game/MetaHumans/GTP/Anims/MH_M_Relaxed_Stand_Idle_Loop                   -> /Game/MetaHumans/GTP/Anims/MH_A_HandsUp
  /Game/MetaHumans/GTP/AnimsCrowd/CS_M_Relaxed_Stand_Idle_Loop              -> /Game/MetaHumans/GTP/AnimsCrowd/CS_A_HandsUp
GTP_HANDSUP_JOBS="idle>folder/name,idle>folder/name" replaces them. GTP_HANDSUP_SPREAD (degrees from the vertical of the
upper arm, default 35) and GTP_HANDSUP_FOREARM (of the forearm, default 12) shape the pose.

Lines "HANDSUP ...". The last line of each job is its proof, read back from the saved asset:
"HANDSUP RESULT <path>: OK (hands N and N cm above the head ...)" or "... BAD ...".
GTP_HANDSUP_SELFTEST=1 python3 make_handsup_animation.py checks the solver on a toy skeleton, without the editor.
Everything stays under /Game, not versioned.
"""
import math
import os

try:
    import unreal
except ImportError:  # the self-test runs without the editor
    unreal = None

DEFAULT_JOBS = [
    ("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop", "/Game/Characters/Pedestrians/Anims/A_HandsUp"),
    ("/Game/MetaHumans/GTP/Anims/MH_M_Relaxed_Stand_Idle_Loop", "/Game/MetaHumans/GTP/Anims/MH_A_HandsUp"),
    ("/Game/MetaHumans/GTP/AnimsCrowd/CS_M_Relaxed_Stand_Idle_Loop", "/Game/MetaHumans/GTP/AnimsCrowd/CS_A_HandsUp"),
]
SPREAD = float(os.environ.get("GTP_HANDSUP_SPREAD") or 35.0)
FOREARM = float(os.environ.get("GTP_HANDSUP_FOREARM") or 12.0)
# A hold-up reads when the wrists are clearly higher than the head bone (the test of -GTARun asks for higher at all).
MIN_ABOVE_HEAD = 10.0


# ---------------------------------------------------------------- quaternions (x, y, z, w), as the engine composes them

def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    length = math.sqrt(dot(a, a))
    return mul(a, 1.0 / length) if length > 1e-9 else (0.0, 0.0, 0.0)


def qmul(a, b):
    """Rotation b, then rotation a."""
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qnorm(q):
    length = math.sqrt(sum(c * c for c in q))
    return tuple(c / length for c in q)


def rotate(q, v):
    u = (q[0], q[1], q[2])
    t = mul(cross(u, v), 2.0)
    return add(add(v, mul(t, q[3])), cross(u, t))


def from_to(a, b):
    """Shortest turn that brings direction a on direction b."""
    a, b = norm(a), norm(b)
    w = 1.0 + dot(a, b)
    if w < 1e-6:
        axis = norm(cross(a, (1.0, 0.0, 0.0)))
        if dot(axis, axis) < 0.5:
            axis = norm(cross(a, (0.0, 1.0, 0.0)))
        return (axis[0], axis[1], axis[2], 0.0)
    c = cross(a, b)
    return qnorm((c[0], c[1], c[2], w))


def axis_angle(axis, degrees):
    half = math.radians(degrees) * 0.5
    s = math.sin(half)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


# ---------------------------------------------------------------- the pose

def component(chain, local, overrides=None):
    """Rotation and position, in the space of the body, of the first bone of chain (bone first, root last)."""
    rot, pos = (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)
    for name in reversed(chain):
        q, p = (overrides or {}).get(name) or local(name)
        pos = add(pos, rotate(rot, p))
        rot = qmul(rot, q)
    return qnorm(rot), pos


def solve(chain_of, local, say=lambda text: None):
    """Local (rotation, translation) of upperarm_l/r and lowerarm_l/r for the raised pose; {} when a bone is missing.

    chain_of(bone) -> [bone, parent, ..., root] or []; local(bone) -> ((x, y, z, w), (x, y, z)) on the frame solved.
    """
    up = (0.0, 0.0, 1.0)
    shoulders = {}
    for side in ("l", "r"):
        chain = chain_of("upperarm_" + side)
        if not chain:
            say("bone upperarm_%s not found" % side)
            return {}
        shoulders[side] = component(chain, local)[1]
    out = {}
    for side, other in (("l", "r"), ("r", "l")):
        names = {key: "%s_%s" % (key, side) for key in ("upperarm", "lowerarm", "hand", "thumb_01")}
        chains = {key: chain_of(bone) for key, bone in names.items()}
        if not (chains["lowerarm"] and chains["hand"]):
            say("bones of the %s arm not found" % side)
            return {}
        ru, pu = component(chains["upperarm"], local)
        rl, pl = component(chains["lowerarm"], local)
        _, ph = component(chains["hand"], local)
        rc, _ = component(chains["upperarm"][1:], local) if len(chains["upperarm"]) > 1 else ((0.0, 0.0, 0.0, 1.0), None)
        thumb = sub(component(chains["thumb_01"], local)[1], ph) if chains["thumb_01"] else None
        away = sub(shoulders[side], shoulders[other])
        lateral = norm((away[0], away[1], 0.0))
        spread, forearm = math.radians(SPREAD), math.radians(FOREARM)
        want_upper = norm(add(mul(lateral, math.sin(spread)), mul(up, math.cos(spread))))
        want_lower = norm(add(mul(lateral, math.sin(forearm)), mul(up, math.cos(forearm))))
        raise_arm = from_to(sub(pl, pu), want_upper)
        best = None
        for twist in range(0, 360, 5) if thumb else (0,):
            # The twist is given at the shoulder, as an arm does it; the one kept turns the palm forward.
            turn = qmul(axis_angle(want_upper, twist), raise_arm)
            bend = from_to(rotate(turn, sub(ph, pl)), want_lower)
            whole = qmul(bend, turn)
            score = 0.0
            if thumb:
                carried = rotate(whole, thumb)
                across = norm(sub(carried, mul(want_lower, dot(carried, want_lower))))
                score = dot(across, mul(lateral, -1.0))
            if best is None or score > best[0]:
                best = (score, twist, turn, whole)
        score, twist, turn, whole = best
        new_ru = qnorm(qmul(turn, ru))
        new_rl = qnorm(qmul(whole, rl))
        out[names["upperarm"]] = (qnorm(qmul(qinv(rc), new_ru)), local(names["upperarm"])[1])
        out[names["lowerarm"]] = (qnorm(qmul(qinv(new_ru), new_rl)), local(names["lowerarm"])[1])
        say("plan %s arm: shoulder at %.0f cm, twist %d deg, thumb towards the head %.2f" % (side, pu[2], twist, score))
    return out


def heights(chain_of, local, overrides=None):
    """(left wrist, right wrist) above the head bone, cm; None when a bone is missing."""
    chains = [chain_of(bone) for bone in ("head", "hand_l", "hand_r")]
    if not all(chains):
        return None
    head, left, right = (component(chain, local, overrides)[1][2] for chain in chains)
    return left - head, right - head


# ---------------------------------------------------------------- self-test (no editor)

def selftest():
    import random
    rng = random.Random(7)
    worst = 1e9
    for case in range(200):
        def spin():
            return qnorm(tuple(rng.uniform(-1.0, 1.0) for _ in range(4)))

        # A body with arbitrary bone axes: every bone's own frame is random, the joints are where a person has them.
        joints = {"root": (0, 0, 0), "spine": (0, 0, 100), "head": (0, 2, 160)}
        parent = {"root": None, "spine": "root", "head": "spine"}
        for side, x in (("l", 1.0), ("r", -1.0)):
            hang = rng.uniform(0.0, 25.0)
            fore = rng.uniform(-30.0, 30.0)
            elbow = add((x * 18, 0, 145), (x * 28 * math.sin(math.radians(hang)), 0, -28 * math.cos(math.radians(hang))))
            wrist = add(elbow, (0, 26 * math.sin(math.radians(fore)), -26 * math.cos(math.radians(fore))))
            joints.update({"clavicle_" + side: (x * 3, 0, 148), "upperarm_" + side: (x * 18, 0, 145), "lowerarm_" + side: elbow,
                           "hand_" + side: wrist, "thumb_01_" + side: add(wrist, (-x * 1.0, 3.0, -2.0))})
            parent.update({"clavicle_" + side: "spine", "upperarm_" + side: "clavicle_" + side, "lowerarm_" + side: "upperarm_" + side,
                           "hand_" + side: "lowerarm_" + side, "thumb_01_" + side: "hand_" + side})
        world = {name: spin() for name in joints}
        locals_ = {}
        for name, at in joints.items():
            if parent[name] is None:
                locals_[name] = (world[name], at)
            else:
                inverse = qinv(world[parent[name]])
                locals_[name] = (qnorm(qmul(inverse, world[name])), rotate(inverse, sub(at, joints[parent[name]])))

        def chain_of(bone):
            chain = []
            while bone:
                if bone not in parent:
                    return []
                chain.append(bone)
                bone = parent[bone]
            return chain

        local = lambda bone: locals_[bone]
        # The toy is built right: its joints come back where they were put.
        for name, at in joints.items():
            assert math.dist(component(chain_of(name), local)[1], at) < 1e-6, name
        pose = solve(chain_of, local)
        assert len(pose) == 4, pose.keys()
        left, right = heights(chain_of, local, pose)
        worst = min(worst, left, right)
        for side, x in (("l", 1.0), ("r", -1.0)):
            wrist = component(chain_of("hand_" + side), local, pose)[1]
            elbow = component(chain_of("lowerarm_" + side), local, pose)[1]
            shoulder = component(chain_of("upperarm_" + side), local, pose)[1]
            thumb = sub(component(chain_of("thumb_01_" + side), local, pose)[1], wrist)
            assert math.dist(shoulder, joints["upperarm_" + side]) < 1e-6
            assert abs(math.dist(shoulder, elbow) - 28.0) < 1e-6 and abs(math.dist(elbow, wrist) - 26.0) < 1e-6, "arm lengths kept"
            assert elbow[2] > shoulder[2] + 20.0 and wrist[2] > elbow[2] + 24.0, "arm raised"
            assert x * elbow[0] > x * shoulder[0], "elbow outside the shoulder"
            assert x * thumb[0] < -0.5 * math.sqrt(dot(thumb, thumb)) + 1.5, "thumb towards the head: %s" % (thumb,)
    print("HANDSUP selftest ok: 200 skeletons with random bone axes, lowest wrist %.1f cm above the head" % worst)


# ---------------------------------------------------------------- the editor's side

def log(text):
    unreal.log("HANDSUP " + text)


def make(source, target):
    folder, name = target.rsplit("/", 1)
    if not unreal.EditorAssetLibrary.does_asset_exist(source):
        log("skipped %s: no idle at %s" % (target, source))
        return None
    unreal.EditorAssetLibrary.make_directory(folder)
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        log("replaces %s" % target)
        unreal.EditorAssetLibrary.delete_asset(target)
    anim = unreal.EditorAssetLibrary.duplicate_asset(source, target)
    if not anim:
        log("RESULT %s: BAD (the idle could not be duplicated)" % target)
        return False
    library = unreal.AnimationLibrary
    frames = library.get_num_frames(anim)
    log("%s from %s, %d frames" % (target, source, frames))

    def chain_of(bone):
        chain = [str(n) for n in library.find_bone_path_to_root(anim, bone)]
        if chain and chain[0] != bone:
            chain.insert(0, bone)
        return chain

    def local_at(frame):
        cache = {}

        def local(bone):
            if bone not in cache:
                t = library.get_bone_pose_for_frame(anim, bone, frame, False)
                q, p = t.rotation, t.translation
                cache[bone] = ((q.x, q.y, q.z, q.w), (p.x, p.y, p.z))
            return cache[bone]
        return local

    before = heights(chain_of, local_at(0))
    pose = solve(chain_of, local_at(0), log)
    if not pose or before is None:
        log("RESULT %s: BAD (bones missing: head, upperarm, lowerarm, hand)" % target)
        return False
    planned = heights(chain_of, local_at(0), pose)
    log("idle: wrists %.0f and %.0f cm from the head; planned: %.0f and %.0f" % (before[0], before[1], planned[0], planned[1]))
    controller = anim.controller
    written = 0
    controller.open_bracket("HandsUp")
    for bone, (q, p) in pose.items():
        done = False
        # The model wants one key per sampled key; which count that is differs between engine versions.
        for count in (frames, frames + 1, frames - 1):
            if count <= 0 or done:
                continue
            try:
                answer = controller.set_bone_track_keys(bone, [unreal.Vector(p[0], p[1], p[2])] * count,
                                                        [unreal.Quat(q[0], q[1], q[2], q[3])] * count, [unreal.Vector(1, 1, 1)] * count)
                done = answer is None or bool(answer)
            except Exception as err:
                log("bone %s with %d keys: %s" % (bone, count, err))
        written += 1 if done else 0
    controller.close_bracket()
    log("arm bones written %d of %d" % (written, len(pose)))
    unreal.EditorAssetLibrary.save_asset(target)
    # The proof: the pose read back from the asset as saved, first and middle frame.
    checks = [heights(chain_of, local_at(frame)) for frame in (0, max(0, frames // 2))]
    low = min(min(pair) for pair in checks)
    ok = written == len(pose) and low >= MIN_ABOVE_HEAD
    log("RESULT %s: %s (hands %.0f and %.0f cm above the head on frame 0, %.0f and %.0f on frame %d; at least %.0f asked)" % (
        target, "OK" if ok else "BAD", checks[0][0], checks[0][1], checks[1][0], checks[1][1], frames // 2, MIN_ABOVE_HEAD))
    return ok


def main():
    jobs = [tuple(job.split(">", 1)) for job in (os.environ.get("GTP_HANDSUP_JOBS") or "").split(",") if ">" in job] or DEFAULT_JOBS
    results = [make(source, target) for source, target in jobs]
    log("done: %d made, %d bad, %d skipped" % (results.count(True), results.count(False), results.count(None)))


if os.environ.get("GTP_HANDSUP_SELFTEST") == "1" or unreal is None:
    selftest()
else:
    main()
