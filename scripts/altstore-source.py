#!/usr/bin/env python3
"""Adds a release to the AltStore and SideStore source in altstore/source.json.

    scripts/altstore-source.py <version> <build number> <path to the IPA> <release notes file>

The newest version goes first, because AltStore treats the first entry as the latest.
"""
import datetime
import hashlib
import json
import pathlib
import sys

REPO = "ChronoStriker1/vndegen"
RAW = f"https://raw.githubusercontent.com/{REPO}/main"
SOURCE = pathlib.Path(__file__).resolve().parent.parent / "altstore" / "source.json"

DESCRIPTION = (
    "VNDegen runs visual novel and RPG Maker games natively. Copy a game's folder into the Games folder in the Files app, "
    "or import a zip, RAR, or 7z archive, and VNDegen detects its engine and shows it in a game hub with artwork from "
    "Steam, SteamGridDB, IGDB, and VNDB. Supports touch, controllers, and keyboard, hidden games behind Face ID or a "
    "passcode, and Ludusavi save backups."
)


def screenshot(name, width, height):
    return {"imageURL": f"{RAW}/docs/screenshots/{name}", "width": width, "height": height}


def skeleton():
    return {
        "name": "VNDegen",
        "identifier": "com.chronostriker1.vndegen.source",
        "subtitle": "Visual novels and RPG Maker games, native on iPhone and iPad.",
        "description": DESCRIPTION,
        "iconURL": f"{RAW}/altstore/icon.png",
        "website": f"https://github.com/{REPO}",
        "tintColor": "#FF4080",
        "apps": [{
            "name": "VNDegen",
            "bundleIdentifier": "com.chronostriker1.vndegen",
            "developerName": "ChronoStriker1",
            "subtitle": "Play PC visual novels and RPG Maker games.",
            "localizedDescription": DESCRIPTION,
            "iconURL": f"{RAW}/altstore/icon.png",
            "tintColor": "#FF4080",
            "category": "games",
            "screenshots": {
                "iphone": [
                    screenshot("hub-iphone.jpg", 644, 1400),
                    screenshot("no-socks-rpg-iphone.jpg", 1600, 736),
                    screenshot("touch-controls.jpg", 1600, 736),
                    screenshot("unlock.jpg", 644, 1400),
                ],
                "ipad": [screenshot("hub-ipad.jpg", 1600, 1200)],
            },
            "versions": [],
            "appPermissions": {
                "entitlements": [],
                "privacy": {"NSFaceIDUsageDescription": "Unlock hidden games."},
            },
        }],
        "news": [],
    }


def main():
    version, build, ipa, notes = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3]), pathlib.Path(sys.argv[4])
    source = json.loads(SOURCE.read_text()) if SOURCE.exists() else skeleton()
    app = source["apps"][0]
    app["versions"] = [v for v in app["versions"] if v["version"] != version]
    app["versions"].insert(0, {
        "version": version,
        "buildVersion": build,
        "date": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "localizedDescription": notes.read_text().strip(),
        "downloadURL": f"https://github.com/{REPO}/releases/download/v{version}/{ipa.name}",
        "size": ipa.stat().st_size,
        "sha256": hashlib.sha256(ipa.read_bytes()).hexdigest(),
        "minOSVersion": "18.0",
    })
    SOURCE.parent.mkdir(exist_ok=True)
    SOURCE.write_text(json.dumps(source, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
