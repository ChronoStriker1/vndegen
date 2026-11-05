#!/usr/bin/env python3
"""Describe one unpacked Windows Unity game as JSON, from metadata and assemblies only.

Reads the Unity version, scripting backend, assembly references, the UnityEngine
members the game's own code calls, P/Invoke targets, native plugins, and per-class
object counts from serialized files. It decodes no textures, audio or meshes and
runs nothing from the game.

Usage: uv run --with dnfile --with UnityPy python -I scan_game.py GAME_DIR > game.json
"""
import collections
import gc
import hashlib
import json
import os
import re
import struct
import sys
import warnings

import dnfile
import pefile
import UnityPy

warnings.filterwarnings("ignore")

# Unity class IDs worth counting. They tell 2D from 3D and show which engine subsystems a game needs.
CLASS_IDS = {
    20: "Camera", 21: "Material", 23: "MeshRenderer", 28: "Texture2D", 33: "MeshFilter", 43: "Mesh",
    48: "Shader", 50: "Rigidbody2D", 54: "Rigidbody", 58: "CircleCollider2D", 60: "PolygonCollider2D",
    61: "BoxCollider2D", 64: "MeshCollider", 65: "BoxCollider", 74: "AnimationClip", 82: "AudioSource",
    83: "AudioClip", 89: "Cubemap", 91: "AnimatorController", 95: "Animator", 108: "Light",
    111: "Animation", 114: "MonoBehaviour", 137: "SkinnedMeshRenderer", 195: "NavMeshAgent",
    198: "ParticleSystem", 212: "SpriteRenderer", 213: "Sprite", 218: "Terrain", 223: "Canvas",
    224: "RectTransform", 328: "VideoPlayer", 329: "VideoClip", 331: "SpriteMask", 483: "TilemapRenderer",
}
FRAMEWORK_MARKERS = {
    "Naninovel": rb"Naninovel", "Utage": rb"Utage\.AdvEngine|AdvEngine",
    "AdventureCreator": rb"AC\.KickStarter|KickStarter", "PixelCrushersDialogue": rb"PixelCrushers", "YarnSpinner": rb"Yarn\.Unity",
    "Ink": rb"Ink\.Runtime", "PlayMaker": rb"PlayMaker", "Spine": rb"Spine\.Unity|SkeletonAnimation|spine-unity",
    "Live2D": rb"Live2D", "TextMeshPro": rb"TMPro|TextMeshPro", "CRIWare": rb"CriWare|CriAtom|cri_ware", "FMOD": rb"FMODUnity|fmodstudio",
    "Wwise": rb"AkSoundEngine", "DOTween": rb"DOTween|DG\.Tweening", "Addressables": rb"UnityEngine\.AddressableAssets",
    "Cinemachine": rb"Cinemachine", "Rewired": rb"Rewired", "Steamworks": rb"Steamworks", "VRM": rb"UniVRM|VRM\.", "SteamVR": rb"SteamVR|Valve\.VR",
    "BehaviorDesigner": rb"BehaviorDesigner", "TopDownEngine": rb"MoreMountains", "Odin": rb"Sirenix",
}
# Engine assemblies wrap native code. Package assemblies (UnityEngine.UI, Unity.TextMeshPro) are plain IL the game ships.
ENGINE_ASM = re.compile(r"^UnityEngine(\.\w+Module)?(\.dll)?$", re.I)
BCL_ASM = re.compile(r"^(mscorlib|netstandard|System|Mono\.|Boo\.|UnityScript|nunit|Microsoft\.)", re.I)
PACKAGE_ASM = re.compile(r"^(UnityEngine\.|Unity\.)", re.I)
UNITY_MESSAGES = {"OnGUI", "OnRenderImage", "OnPostRender", "OnPreRender", "OnRenderObject", "OnWillRenderObject", "OnAudioFilterRead",
                  "OnCollisionEnter", "OnCollisionEnter2D", "OnTriggerEnter", "OnTriggerEnter2D", "OnAnimatorIK", "OnApplicationQuit", "OnApplicationFocus"}
VERSION_RE = re.compile(rb"\b(\d{1,4}\.\d{1,2}\.\d{1,3}[abfpx]\d{1,3}(?:c\d{1,2})?)\b")
MEDIA_EXTS = {".mp4", ".webm", ".ogv", ".mov", ".avi", ".usm", ".acb", ".awb", ".bank", ".bnk", ".mkv", ".wmv", ".bundle", ".assetbundle", ".unity3d"}
SHADER_PLATFORMS = {
    # ShaderCompilerPlatform. D3D12 shares the D3D11 (DXBC) blobs, so it has no id of its own.
    1: "d3d9", 4: "d3d11", 5: "gles2", 6: "d3d11_9x", 9: "gles3", 14: "metal", 15: "glcore", 18: "vulkan",
}


def walk(root):
    for d, _, fs in os.walk(root):
        for f in fs:
            yield os.path.join(d, f)


def find_data_dirs(root):
    found = []
    for d, ds, fs in os.walk(root):
        lower = {f.lower() for f in fs}
        if os.path.basename(d).lower().endswith("_data") and lower & {"globalgamemanagers", "maindata", "data.unity3d"}:
            found.append(d)
    return found


def dir_size(path):
    return sum(os.path.getsize(p) for p in walk(path) if not os.path.islink(p))


def unity_version(data_dir):
    for name in ("globalgamemanagers", "mainData", "data.unity3d"):
        path = os.path.join(data_dir, name)
        if os.path.exists(path):
            with open(path, "rb") as f:
                head = f.read(256)
            m = VERSION_RE.search(head)
            if m:
                return m.group(1).decode()
    return None


def pe_machine(path):
    try:
        with open(path, "rb") as f:
            head = f.read(4096)
        off = struct.unpack_from("<I", head, 0x3C)[0]
        return {0x14C: "x86", 0x8664: "x64", 0xAA64: "arm64"}.get(struct.unpack_from("<H", head, off + 4)[0])
    except Exception:
        return None


def describe_dll(path):
    """Size, hash and exports of a DLL beside the exe, to tell loaders and DRM from ordinary plugins."""
    with open(path, "rb") as f:
        blob = f.read()
    info = {"bytes": len(blob), "sha256": hashlib.sha256(blob).hexdigest()}
    try:
        pe = pefile.PE(data=blob, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        info["exports"] = [e.name.decode(errors="replace") for e in getattr(getattr(pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", []) if e.name][:40]
    except Exception:
        info["exports"] = None
    return info


def scan_assembly(path):
    """Member references from one assembly, grouped by the assembly that defines the target type."""
    out = {"refs": [], "members": collections.Counter(), "types": collections.Counter(), "pinvoke": collections.Counter(),
           "bcl": collections.Counter(), "messages": collections.Counter(), "methods": 0, "corlib": None, "error": None}
    try:
        pe = dnfile.dnPE(path, fast_load=False)
        md = pe.net.mdtables
    except Exception as e:
        out["error"] = f"{type(e).__name__}: {e}"
        return out
    if md is None:
        out["error"] = "no CLR metadata"
        return out
    try:
        asm_refs = [str(r.Name) for r in (md.AssemblyRef.rows if md.AssemblyRef else [])]
        out["refs"] = asm_refs
        for r in md.AssemblyRef.rows if md.AssemblyRef else []:
            if str(r.Name) in ("mscorlib", "netstandard"):
                out["corlib"] = f"{r.Name} {r.MajorVersion}.{r.MinorVersion}"
        type_scope = {}
        for i, tr in enumerate(md.TypeRef.rows if md.TypeRef else [], 1):
            scope = tr.ResolutionScope
            scope_name = None
            parent = None
            if scope is not None and scope.table is not None:
                row = scope.row
                if scope.table.name == "AssemblyRef":
                    scope_name = str(row.Name)
                elif scope.table.name == "TypeRef":
                    parent = row
            ns = str(tr.TypeNamespace) if tr.TypeNamespace else ""
            full = f"{ns}.{tr.TypeName}" if ns else str(tr.TypeName)
            if parent is not None:
                # Nested type: walk up to the assembly and keep the outer name.
                p = parent
                names = [str(tr.TypeName)]
                while p is not None:
                    names.insert(0, str(p.TypeName))
                    s = p.ResolutionScope
                    if s is not None and s.table is not None and s.table.name == "AssemblyRef":
                        scope_name = str(s.row.Name)
                        pns = str(p.TypeNamespace) if p.TypeNamespace else ""
                        full = (pns + "." if pns else "") + "/".join(names)
                        p = None
                    elif s is not None and s.table is not None and s.table.name == "TypeRef":
                        p = s.row
                    else:
                        p = None
            type_scope[i] = (scope_name, full)
        unity = lambda scope: bool(scope) and bool(ENGINE_ASM.match(scope))
        for scope, full in type_scope.values():
            if unity(scope):
                out["types"][f"{scope}|{full}"] += 1
        for mr in md.MemberRef.rows if md.MemberRef else []:
            cls = mr.Class
            if cls is None or cls.table is None:
                continue
            if cls.table.name == "TypeRef":
                scope, full = type_scope.get(cls.row_index, (None, None))
            elif cls.table.name == "TypeSpec":
                continue  # generic instantiations; the open type's TypeRef is already counted.
            else:
                continue
            if unity(scope):
                out["members"][f"{scope}|{full}::{mr.Name}"] += 1
            elif scope and scope.split(",")[0] in ("mscorlib", "System", "System.Core", "netstandard"):
                if re.match(r"System\.(IO\.|Runtime\.Serialization\.Formatters|Reflection\.Emit|Threading\.Thread$|Net\.|Diagnostics\.Process|Windows\.|Environment$)|Microsoft\.Win32", full or ""):
                    out["bcl"][f"{full}::{mr.Name}"] += 1
        for im in md.ImplMap.rows if md.ImplMap else []:
            if im.ImportScope is not None and im.ImportScope.row is not None:
                out["pinvoke"][str(im.ImportScope.row.Name).lower()] += 1
        for m in md.MethodDef.rows if md.MethodDef else []:
            out["methods"] += 1
            if str(m.Name) in UNITY_MESSAGES:
                out["messages"][str(m.Name)] += 1
    except Exception as e:
        out["error"] = f"{type(e).__name__}: {e}"
    finally:
        try:
            pe.close()
        except Exception:
            pass
    return out


def count_internal_calls(path):
    """InternalCall methods in a shipped UnityEngine assembly: the native surface beneath the managed API."""
    try:
        pe = dnfile.dnPE(path, fast_load=False)
        md = pe.net.mdtables
        n = 0
        for m in md.MethodDef.rows if md and md.MethodDef else []:
            if m.ImplFlags.miInternalCall:
                n += 1
        pe.close()
        return n
    except Exception:
        return None


def scan_serialized(data_dir):
    """Object counts per class across the game's serialized files, without decoding any object."""
    counts = collections.Counter()
    shader_platforms = collections.Counter()
    shader_names = collections.Counter()
    files, errors, target = 0, 0, None
    candidates = []
    for p in walk(data_dir):
        rel = os.path.relpath(p, data_dir)
        low = os.path.basename(p).lower()
        if rel.lower().startswith(("managed", "il2cpp_data", "mono", "plugins")):
            continue
        ext = os.path.splitext(low)[1]
        if ext in (".assets", ".bundle", ".assetbundle", ".unity3d") or re.fullmatch(r"(level\d+|globalgamemanagers|maindata|sharedassets\d+\.assets|unity_builtin_extra|unity default resources|unity_default_resources)", low):
            candidates.append(p)
    # Addressables games can hold thousands of bundles, and UnityPy reads a file into memory.
    # Bounds keep one game from taking hours or the machine's memory; the JSON records what they skipped.
    limit = int(os.environ.get("UNITY_SCAN_MAX_FILE_BYTES", 1 << 30))
    skipped = [p for p in candidates if os.path.getsize(p) > limit]
    candidates = sorted((p for p in candidates if os.path.getsize(p) <= limit), key=os.path.getsize)
    for p in candidates[:600]:
        try:
            env = UnityPy.load(p)
        except Exception:
            errors += 1
            continue
        for name, sf in env.files.items():
            for af in ([sf] if hasattr(sf, "objects") else list(getattr(sf, "files", {}).values())):
                if not hasattr(af, "objects"):
                    continue
                files += 1
                target = target or str(getattr(af, "target_platform", "") or "")
                for obj in af.objects.values():
                    cid = int(getattr(obj, "class_id", -1))
                    counts[CLASS_IDS.get(cid, "other")] += 1
                    if cid == 48 and sum(shader_names.values()) < 3000:
                        # Shaders are code, not art: name and compiled platforms answer what a port must translate.
                        try:
                            tree = obj.read()
                            parsed = getattr(tree, "m_ParsedForm", None)
                            shader_names[(getattr(parsed, "m_Name", None) or getattr(tree, "m_Name", "") or "?")] += 1
                            for plat in getattr(tree, "platforms", []) or []:
                                shader_platforms[SHADER_PLATFORMS.get(int(plat), str(int(plat)))] += 1
                        except Exception:
                            shader_names["<unreadable>"] += 1
        del env
        gc.collect()
    return {"objects": dict(counts), "serialized_files": files, "errors": errors, "bounded": len(candidates) > 600, "skipped_large": len(skipped),
            "candidates": len(candidates), "target_platform": target, "shader_platforms": dict(shader_platforms), "shaders": dict(shader_names)}


def il2cpp_metadata(path):
    with open(path, "rb") as f:
        blob = f.read()
    magic, version = struct.unpack_from("<Ii", blob, 0)
    info = {"magic_ok": magic == 0xFAB11BAF, "version": version if magic == 0xFAB11BAF else None, "size": len(blob)}
    info["images"] = sorted({m.decode() for m in re.findall(rb"[A-Za-z0-9_.\-]+\.dll", blob)})[:400]
    return info, blob


def scan_game(root):
    result = {"root": os.path.basename(os.path.abspath(root)), "data_dirs": []}
    data_dirs = find_data_dirs(root)
    result["data_dir_count"] = len(data_dirs)
    if not data_dirs:
        return result
    data_dirs.sort(key=dir_size, reverse=True)
    data = data_dirs[0]
    game = os.path.dirname(data)
    result["data_dirs"] = [os.path.basename(d) for d in data_dirs]
    result["version"] = unity_version(data)
    names = {f.lower(): f for f in os.listdir(game)}
    managed = os.path.join(data, "Managed")
    has_managed = os.path.isdir(managed) and any(f.lower().endswith(".dll") for f in os.listdir(managed))
    meta = os.path.join(data, "il2cpp_data", "Metadata", "global-metadata.dat")
    result["backend"] = "IL2CPP" if "gameassembly.dll" in names else "Mono" if has_managed else "unknown"
    exes = [f for f in os.listdir(game) if f.lower().endswith(".exe") and "crash" not in f.lower() and "unins" not in f.lower()]
    result["exe_arch"] = next((pe_machine(os.path.join(game, e)) for e in exes if pe_machine(os.path.join(game, e))), None)
    result["player_arch"] = pe_machine(os.path.join(game, names["unityplayer.dll"])) if "unityplayer.dll" in names else None
    result["total_bytes"] = dir_size(game)
    result["beside_exe"] = sorted(f for f in os.listdir(game) if f.lower().endswith(".dll"))
    result["extra_dlls"] = {f: describe_dll(os.path.join(game, f)) for f in result["beside_exe"]
                            if f.lower() not in ("unityplayer.dll", "gameassembly.dll", "baselib.dll")}
    plugins = []
    plugin_dir = os.path.join(data, "Plugins")
    if os.path.isdir(plugin_dir):
        plugins = sorted({os.path.basename(p) for p in walk(plugin_dir)})
    result["plugins"] = plugins
    try:
        with open(os.path.join(data, "boot.config"), errors="replace") as f:
            result["boot_config"] = f.read()[:1500]
    except OSError:
        result["boot_config"] = None
    result["d3d12_folder"] = os.path.isdir(os.path.join(game, "D3D12"))
    # File types beside the assets: movies, middleware audio, separate bundles.
    media = collections.Counter()
    for p in walk(game):
        ext = os.path.splitext(p)[1].lower()
        if ext in MEDIA_EXTS:
            media[ext] += 1
    result["media"] = dict(media)
    result["addressables"] = os.path.isdir(os.path.join(data, "StreamingAssets", "aa"))
    streaming = os.path.join(data, "StreamingAssets")
    result["streaming_assets_bytes"] = dir_size(streaming) if os.path.isdir(streaming) else 0
    result["streaming_assets_top"] = sorted(os.listdir(streaming))[:40] if os.path.isdir(streaming) else []

    corpus = b""
    if result["backend"] == "Mono":
        modules = sorted(f for f in os.listdir(managed) if f.lower().endswith(".dll"))
        result["managed"] = modules
        result["engine_modules"] = [m for m in modules if ENGINE_ASM.match(m)]
        result["package_assemblies"] = [m for m in modules if PACKAGE_ASM.match(m) and not ENGINE_ASM.match(m)]
        game_asms = [m for m in modules if not (ENGINE_ASM.match(m) or BCL_ASM.match(m) or PACKAGE_ASM.match(m))]
        result["game_assemblies"] = game_asms
        members, pkg_members, types, pinvoke, bcl, messages = (collections.Counter() for _ in range(6))
        methods, errs, corlib = 0, {}, None
        refs = collections.Counter()
        for m in game_asms + result["package_assemblies"]:
            r = scan_assembly(os.path.join(managed, m))
            (members if m in game_asms else pkg_members).update(r["members"])
            types.update(r["types"]); pinvoke.update(r["pinvoke"]); bcl.update(r["bcl"]); messages.update(r["messages"])
            refs.update(r["refs"])
            methods += r["methods"] if m in game_asms else 0
            corlib = corlib or r["corlib"]
            if r["error"]:
                errs[m] = r["error"]
        result["members"] = dict(members)
        result["package_members"] = dict(pkg_members)
        result["types"] = dict(types)
        result["pinvoke"] = dict(pinvoke)
        result["bcl"] = dict(bcl)
        result["messages"] = dict(messages)
        result["asm_refs"] = dict(refs)
        result["game_methods"] = methods
        result["corlib"] = corlib
        result["assembly_errors"] = errs
        icalls, total = {}, 0
        for m in result["engine_modules"]:
            n = count_internal_calls(os.path.join(managed, m))
            if n:
                icalls[m] = n
                total += n
        result["engine_internal_calls"] = total
        result["engine_internal_calls_by_module"] = icalls
        ac = os.path.join(managed, "Assembly-CSharp.dll")
        result["assembly_csharp_bytes"] = os.path.getsize(ac) if os.path.exists(ac) else None
        corpus = b" ".join(m.encode() for m in modules)
        for m in game_asms + result["package_assemblies"]:
            with open(os.path.join(managed, m), "rb") as f:
                corpus += f.read()
    elif result["backend"] == "IL2CPP":
        ga = os.path.join(game, names["gameassembly.dll"])
        result["game_assembly_bytes"] = os.path.getsize(ga)
        result["game_assembly_arch"] = pe_machine(ga)
        with open(ga, "rb") as f:
            # IL2CPP resolves engine internal calls by name, so these strings are the native surface this build uses.
            result["il2cpp_icalls"] = sorted({m.decode() for m in re.findall(rb"UnityEngine\.[\w.]+::[\w]+(?:\([^\x00]{0,200}\))?", f.read())})
        if os.path.exists(meta):
            info, blob = il2cpp_metadata(meta)
            result["il2cpp"] = info
            corpus = blob
            result["engine_modules"] = [m for m in info["images"] if ENGINE_ASM.match(m)]
            result["managed"] = info["images"]
        else:
            result["il2cpp"] = {"missing": True}
    result["markers"] = sorted(k for k, pat in FRAMEWORK_MARKERS.items() if re.search(pat, corpus))
    # "Fungus" alone is an ordinary word for a game to use, so ask for its namespace string and its main class.
    if re.search(rb"\x00Fungus\x00", corpus) and b"Flowchart" in corpus:
        result["markers"] = sorted(result["markers"] + ["Fungus"])
    # Go by the pipeline's assembly: its name also turns up as a string in code that merely supports it.
    assemblies = " ".join(result.get("managed", []))
    result["render_pipeline"] = ("HDRP" if "RenderPipelines.HighDefinition.Runtime" in assemblies else
                                 "URP" if "RenderPipelines.Universal.Runtime" in assemblies else
                                 "LWRP" if "RenderPipelines.Lightweight.Runtime" in assemblies else "Built-in")
    result["serialized"] = scan_serialized(data)
    return result


if __name__ == "__main__":
    json.dump(scan_game(sys.argv[1]), sys.stdout, ensure_ascii=False, indent=0)
