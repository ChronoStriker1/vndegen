#!/usr/bin/env python3
"""Turn scan_game.py results into docs/unity-games.tsv and print the numbers in docs/unity.md.

Usage: summarize.py SCAN_DIR OUT_TSV. SCAN_DIR holds one JSON per game: {"name", "how", "scan": <scan_game.py output>}.
"""
import collections as C
import json
import os
import re
import statistics as st
import sys

scan_dir, out_tsv = sys.argv[1:3]
records = [json.load(open(os.path.join(scan_dir, f))) for f in sorted(os.listdir(scan_dir)) if f.endswith(".json")]
read = [r for r in records if r.get("scan", {}).get("backend") in ("Mono", "IL2CPP")]
# Same rule as scan_game.py, so results from before that rule changed still read correctly.
for r in read:
    names = " ".join(r["scan"].get("managed", []))
    r["scan"]["render_pipeline"] = ("HDRP" if "RenderPipelines.HighDefinition.Runtime" in names else "URP" if "RenderPipelines.Universal.Runtime" in names
                                    else "LWRP" if "RenderPipelines.Lightweight.Runtime" in names else "Built-in")
mono = [r for r in read if r["scan"]["backend"] == "Mono"]
il2cpp = [r for r in read if r["scan"]["backend"] == "IL2CPP"]

# Shaders that come with Unity or its common packages, by name. The rest are the game's own or from an asset pack.
STOCK = re.compile(r"^(Hidden/|Sprites/|UI/|GUI/|Standard|Legacy Shaders/|Mobile/|Skybox/|Particles/|Unlit/|TextMeshPro/|Universal Render Pipeline/"
                   r"|Nature/|VR/|FX/|Sprite|Spine/|Naninovel/|Live2D|Cubism|Internal-|Normal-|Camera-|Font$|Unlit - )")
ROUTINE_PLUGIN = re.compile(r"^(lib_burst_generated|steam_api|Galaxy|sdkencryptedappticket|SOVEREIGN|winmm)", re.I)


def objects(r):
    return r["scan"].get("serialized", {}).get("objects", {})


def kind(r):
    o, marks = objects(r), set(r["scan"].get("markers", []))
    three_d = (o.get("SkinnedMeshRenderer", 0) >= 10 or (o.get("Mesh", 0) >= 60 and (o.get("Light", 0) >= 10 or o.get("Rigidbody", 0) >= 10))
               or o.get("Light", 0) >= 20 or o.get("Rigidbody", 0) >= 50)
    # 2D games lean on sprites even when lights or mesh-based sprite kits make them look 3D by the counts above.
    if o.get("SkinnedMeshRenderer", 0) < 10 and o.get("SpriteRenderer", 0) >= 5 * max(o.get("MeshRenderer", 0), 1):
        three_d = False
    if marks & {"SteamVR", "VRM"}:
        return "3D or VR"
    if three_d:
        return "3D"
    for framework in ("Naninovel", "Utage", "Fungus", "AdventureCreator"):
        if framework in marks:
            return "VN: " + framework
    return "2D"


def norm(key):
    return key.split("|", 1)[1]


def surface(r, with_packages=False):
    s = r["scan"]
    keys = set(map(norm, s.get("members", {})))
    return keys | set(map(norm, s.get("package_members", {}))) if with_packages else keys


def custom_shaders(r):
    return sum(1 for n in r["scan"].get("serialized", {}).get("shaders", {}) if not STOCK.match(n))


def other_plugins(r):
    s = r["scan"]
    names = [p for p in s.get("plugins", []) if p.lower().endswith((".dll", ".so")) and not ROUTINE_PLUGIN.match(p)]
    return sorted(set(names))


def line(*values):
    return "\t".join(str(v) for v in values)


# Titles the leave-out rule excludes aren't listed here. docs/unity.md gives their count.
rows = [line("folder", "unity", "backend", "arch", "pipeline", "kind", "frameworks", "other_native_plugins", "engine_members_game_code",
             "engine_members_with_packages", "shaders", "custom_shaders", "shaders_with_metal", "gb", "read_from")]
for r in sorted(records, key=lambda r: r["name"].lower()):
    if r.get("how") == "left out":
        continue
    if r not in read:
        rows.append(line(r["name"], "", "", "", "", "not read", "", "", "", "", "", "", "", "", r.get("how", "")))
        continue
    s = r["scan"]
    sh = s.get("serialized", {})
    fw = [m for m in s.get("markers", []) if m in ("Naninovel", "Utage", "Fungus", "AdventureCreator", "YarnSpinner", "Ink", "PixelCrushersDialogue", "PlayMaker", "Spine", "Live2D", "CRIWare", "FMOD", "SteamVR", "VRM")]
    rows.append(line(r["name"], s.get("version"), s["backend"], s.get("exe_arch"), s["render_pipeline"], kind(r), ",".join(fw), ",".join(other_plugins(r)),
                     len(surface(r)) if s["backend"] == "Mono" else "", len(surface(r, True)) if s["backend"] == "Mono" else "",
                     sum(sh.get("shaders", {}).values()), custom_shaders(r), sh.get("shader_platforms", {}).get("metal", 0),
                     round(s.get("total_bytes", 0) / 2**30, 1), r.get("how", "")))
open(out_tsv, "w").write("\n".join(rows) + "\n")


def era(v):
    major = int(v.split(".")[0])
    return "4.x" if major == 4 else "5.x" if major == 5 else "2017-2018" if major <= 2018 else str(major) if major < 6000 else "6000 (Unity 6)"


print("records", len(records), C.Counter(r.get("how") for r in records))
print("read", len(read), "mono", len(mono), "il2cpp", len(il2cpp), "GB", round(sum(r["scan"].get("total_bytes", 0) for r in read) / 2**30))
print("not read", [(r["name"], r.get("how")) for r in records if r not in read and r.get("how") != "left out"])
print("distinct versions", len({r["scan"].get("version") for r in read}), "None", sum(1 for r in read if not r["scan"].get("version")))
order = ["4.x", "5.x", "2017-2018", "2019", "2020", "2021", "2022", "6000 (Unity 6)"]
c = C.Counter((era(r["scan"]["version"]), r["scan"]["backend"]) for r in read)
print("era", [(e, c[(e, "Mono")], c[(e, "IL2CPP")]) for e in order])
print("kinds", sorted(C.Counter((kind(r), r["scan"]["backend"]) for r in read).items()))
print("pipeline", C.Counter((r["scan"]["render_pipeline"], r["scan"]["backend"]) for r in read))
print("arch", C.Counter((r["scan"]["backend"], r["scan"].get("exe_arch")) for r in read))
print("markers", C.Counter(m for r in read for m in r["scan"].get("markers", [])).most_common())
print("any naninovel", C.Counter(r["scan"]["backend"] for r in read if "Naninovel" in r["scan"].get("markers", [])), "with live2d", sum(1 for r in read if {"Naninovel", "Live2D"} <= set(r["scan"].get("markers", []))))
print("plugins", C.Counter(p for r in read for p in r["scan"].get("plugins", []) if p.lower().endswith(".dll")).most_common(12))
print("games with other native plugins", sum(1 for r in read if other_plugins(r)))
print("custom shader buckets", C.Counter("0" if custom_shaders(r) == 0 else "1-5" if custom_shaders(r) <= 5 else "6-30" if custom_shaders(r) <= 30 else "31+" for r in read))
metal = [(r["scan"]["serialized"].get("shader_platforms", {}).get("metal", 0), sum(r["scan"]["serialized"].get("shaders", {}).values())) for r in read if r["scan"].get("serialized", {}).get("shader_platforms")]
print("metal shaders per game, distribution", C.Counter(m for m, _ in metal).most_common(), "games", len(metal), "median shaders", st.median(t for _, t in metal))


def report(label, games):
    if not games:
        print(label, 0)
        return
    for desc, flag in (("game code", False), ("with packages", True)):
        sets = [surface(r, flag) for r in games]
        sizes = sorted(map(len, sets))
        union = set().union(*sets)
        freq = C.Counter(m for x in sets for m in x)
        print(f"{label} [{desc}] games {len(games)} median {st.median(sizes)} max {sizes[-1]} union {len(union)} types {len({m.split('::')[0] for m in union})}"
              f" in>=90% {sum(v >= .9 * len(games) for v in freq.values())} in>=50% {sum(v >= .5 * len(games) for v in freq.values())} one game only {sum(v == 1 for v in freq.values())}")


report("all mono", mono)
twod = [r for r in mono if kind(r) == "2D" or kind(r).startswith("VN")]
report("mono 2D or VN", twod)
builtin = [r for r in twod if r["scan"]["render_pipeline"] == "Built-in"]
report("mono 2D/VN built-in", builtin)
nolive = [r for r in builtin if "Live2D" not in r["scan"].get("markers", [])]
report("mono 2D/VN built-in, no Live2D", nolive)
easy = [r for r in nolive if not other_plugins(r) and custom_shaders(r) <= 5]
report("  and no other plugin, <=5 custom shaders", easy)
print("easy", sorted((r["name"][:26], r["scan"].get("version"), custom_shaders(r)) for r in easy))
print("il2cpp no metadata image list", [r["name"][:30] for r in il2cpp if not r["scan"].get("managed")])
print("twod urp", sum(1 for r in twod if r["scan"]["render_pipeline"] != "Built-in"), "x86 in twod", sum(1 for r in twod if r["scan"].get("exe_arch") == "x86"))
nani = [r for r in read if "Naninovel" in r["scan"].get("markers", [])]
print("naninovel versions", sorted(r["scan"]["version"] for r in nani), C.Counter((kind(r), r["scan"]["render_pipeline"], r["scan"]["backend"]) for r in nani))
checks = {"PlayerPrefs": r"\.PlayerPrefs::", "persistentDataPath": r"Application::get_persistentDataPath", "dataPath": r"Application::get_dataPath", "AssetBundle": r"AssetBundle::",
          "Input (legacy)": r"\.Input::", "Physics 3D": r"\.Physics::|\.Rigidbody::|\.Collider::", "Physics 2D": r"\.Physics2D::|Rigidbody2D::|Collider2D::", "VideoPlayer": r"VideoPlayer::",
          "CommandBuffer": r"CommandBuffer::", "RenderTexture": r"RenderTexture::", "GL": r"\.GL::", "Procedural Mesh": r"Mesh::[sS]et", "Animator": r"\.Animator::", "ParticleSystem": r"ParticleSystem::", "AudioSource": r"AudioSource::", "IMGUI": r"\.GUI::|GUILayout::", "Network API": r"UnityWebRequest|\.WWW::"}
for label, group in (("all", mono), ("2D/VN", twod)):
    print(label, "of", len(group), {k: sum(any(re.search(p, m) for m in r["scan"].get("members", {})) for r in group) for k, p in checks.items()})
bcl = {"File IO": r"System\.IO\.File", "BinaryFormatter": r"BinaryFormatter", "Registry": r"Microsoft\.Win32", "Reflection.Emit": r"Reflection\.Emit", "Process": r"Diagnostics\.Process", "GetFolderPath": r"Environment::GetFolderPath"}
print("bcl", {k: sum(any(re.search(p, m) for m in r["scan"].get("bcl", {})) for r in mono) for k, p in bcl.items()})
print("corlib", C.Counter(r["scan"].get("corlib") for r in mono).most_common())
ic = sorted(r["scan"].get("engine_internal_calls", 0) for r in mono)
print("internal calls in shipped engine", ic[0], st.median(ic), ic[-1])
print("game assembly MB", sorted(round(r["scan"].get("game_assembly_bytes", 0) / 2**20) for r in il2cpp))
print("three d games", sorted((r["name"][:28], objects(r).get("SkinnedMeshRenderer", 0), objects(r).get("MeshRenderer", 0), objects(r).get("SpriteRenderer", 0)) for r in read if kind(r).startswith("3D")))
print("il2cpp icall strings", sorted(len(r["scan"].get("il2cpp_icalls", [])) for r in il2cpp))
print("il2cpp metadata versions", C.Counter(r["scan"].get("il2cpp", {}).get("version") for r in il2cpp).most_common())
print("pinvoke steam", sum(1 for r in mono if {"steam_api", "steam_api64"} & set(r["scan"].get("pinvoke", {}))), "windows api", sum(1 for r in mono if any(k.startswith(("user32", "kernel32", "gdi32", "shell32", "comdlg32", "advapi32")) for k in r["scan"].get("pinvoke", {}))))
print("sovereign", sum(1 for r in read if any(p.lower().startswith("sovereign") for p in r["scan"].get("beside_exe", []) + r["scan"].get("plugins", []))))
print("skipped large", sum(1 for r in read if r["scan"].get("serialized", {}).get("skipped_large")), "bounded", sum(1 for r in read if r["scan"].get("serialized", {}).get("bounded")))
