#!/usr/bin/env python3
import hashlib
import os
import struct
import sys


def pack(path, files, major):
    names = sorted(files)
    bodies = [files[n].encode() for n in names]
    paths = [("res://" + n).encode() for n in names]
    if major == 3:
        header = struct.pack("<IIIII", 0x43504447, 1, 3, 6, 3) + b"\0" * 64
        entry = lambda p, offset, body: struct.pack("<I", len(p)) + p + struct.pack("<QQ", offset, len(body)) + hashlib.md5(body).digest()
        size = lambda p: 4 + len(p) + 32
    else:
        header = struct.pack("<IIIIII", 0x43504447, 2, 4, 2, 0, 0) + struct.pack("<Q", 0) + b"\0" * 64
        padded = lambda p: p + b"\0" * (-len(p) % 4)
        entry = lambda p, offset, body: struct.pack("<I", len(padded(p))) + padded(p) + struct.pack("<QQ", offset, len(body)) + hashlib.md5(body).digest() + struct.pack("<I", 0)
        size = lambda p: 4 + len(padded(p)) + 36
    offset = len(header) + 4 + sum(size(p) for p in paths)
    directory = struct.pack("<I", len(names))
    for p, body in zip(paths, bodies):
        directory += entry(p, offset, body)
        offset += len(body)
    with open(path, "wb") as f:
        f.write(header + directory + b"".join(bodies))


def game(major, name, script, settings="", main_scene="res://main.tscn"):
    if major == 3:
        project = 'config_version=4\n[application]\nconfig/name="%s"\nrun/main_scene="%s"\n' % (name, main_scene)
        scene = '[gd_scene load_steps=2 format=2]\n[ext_resource path="res://main.gd" type="Script" id=1]\n[node name="Main" type="Node"]\nscript = ExtResource( 1 )\n'
    else:
        project = 'config_version=5\n[application]\nconfig/name="%s"\nrun/main_scene="%s"\n' % (name, main_scene)
        scene = '[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://main.gd" id="1"]\n[node name="Main" type="Node"]\nscript = ExtResource("1")\n'
    return {"project.godot": project + settings, "main.tscn": scene, "main.gd": script}


QUIT_SOON = "extends Node\nfunc _ready():\n\tprint(\"VNTEST %s ready\")\n"

GAMES4 = {
    "plain": (QUIT_SOON % "plain", ""),
    "capped": (QUIT_SOON % "capped", "[application]\nrun/max_fps=30\n"),
    "delay": (QUIT_SOON % "delay", "[application]\nrun/frame_delay_msec=50\n"),
    "interpolated": ('''extends Node
var n: Node3D
var moved = 0
var frames = 0
func _ready():
	n = Node3D.new()
	add_child(n)
func _physics_process(d):
	n.position.x += 1.0
func _process(d):
	frames += 1
	if abs(n.get_global_transform_interpolated().origin.x - n.global_position.x) > 0.01:
		moved += 1
	if frames == 50:
		print("VNTEST interpolated frames: ", moved, " of ", frames)
''', "[physics]\ncommon/physics_interpolation=true\ncommon/physics_ticks_per_second=10\n"),
    "thread": ('''extends Node
var t: Thread
var count = 0
func _work():
	while true:
		count += 1
		OS.delay_msec(1)
func _ready():
	t = Thread.new()
	t.start(_work)
	print("VNTEST thread ready")
''', ""),
    "joined": ('''extends Node
var t: Thread
var running = true
func _work():
	while running:
		OS.delay_msec(5)
func _ready():
	t = Thread.new()
	t.start(_work)
	print("VNTEST joined ready")
func _exit_tree():
	running = false
	t.wait_to_finish()
	print("VNTEST joined thread stopped")
''', ""),
    "pool": ('''extends Node
func _ready():
	WorkerThreadPool.add_task(func(): OS.delay_msec(10))
	print("VNTEST pool ready")
''', ""),
    "broken": (QUIT_SOON % "broken", ""),
}

GAMES3 = {
    "plain": (QUIT_SOON % "plain", ""),
    "capped": (QUIT_SOON % "capped", "[debug]\nsettings/fps/force_fps=30\n"),
    "delay": (QUIT_SOON % "delay", "[application]\nrun/frame_delay_msec=50\n"),
    "thread": ('''extends Node
var t
var count = 0
func _work(_u):
	while true:
		count += 1
		OS.delay_msec(1)
func _ready():
	t = Thread.new()
	t.start(self, "_work")
	print("VNTEST thread ready")
''', ""),
    "joined": ('''extends Node
var t
var running = true
func _work(_u):
	while running:
		OS.delay_msec(5)
func _ready():
	t = Thread.new()
	t.start(self, "_work")
	print("VNTEST joined ready")
func _exit_tree():
	running = false
	t.wait_to_finish()
	print("VNTEST joined thread stopped")
''', ""),
    "broken": (QUIT_SOON % "broken", ""),
}


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("3", "4"):
        sys.exit("usage: packs.py 3|4 <folder>")
    major = int(sys.argv[1])
    folder = sys.argv[2]
    for name, (script, settings) in (GAMES3 if major == 3 else GAMES4).items():
        main_scene = "res://missing.tscn" if name == "broken" else "res://main.tscn"
        os.makedirs(os.path.join(folder, name), exist_ok=True)
        pack(os.path.join(folder, name, "game.pck"), game(major, name, script, settings, main_scene), major)


main()
