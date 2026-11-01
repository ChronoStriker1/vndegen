# The library

The library lists every game in VNDegen's games folders, with its artwork, and Play starts it. This page covers adding games, the library's layouts and menus, hidden games, and where artwork and metadata come from.

## Adding games

Put each game in its own folder, named as you want it to appear. The folder name is the game's identity, so the same game copied to several devices keeps its artwork and settings on each. The one exception is a game whose folder name a game in another library folder already has, described under [Other folders](#other-folders).

- **iPhone and iPad.** Open the Files app, then On My iPhone (or iPad), VNDegen, Games, and copy the game folders there.
- **Mac.** Copy the game folders into `~/Games/VNDegen`. It sits outside `~/Documents` so iCloud Drive does not upload your games.
- **From an archive.** Add game in the library unpacks a zip, RAR, or 7z into the library. You can also drag an archive from Files or Finder onto VNDegen, or open it in VNDegen from the Files Share menu or with Open With on a Mac.
- **Archive copied into VNDegen's folder.** Copy a zip, RAR, or 7z into On My iPhone (or iPad), VNDegen, or its Games folder. On a Mac, copy it into `~/Games/VNDegen`. The library lists it under Ready to import, and its Import button opens the import with the archive chosen. "Delete archive after import" starts on for these archives. Use this when choosing an archive, dropping it, or opening it in VNDegen does nothing, which happens on some sideloaded installs.

VNDegen rescans every library folder on launch, on pull to refresh, with Rescan in Settings, and when you return to the app.

## Other folders

Settings, Library folders, Add folder adds a folder of game folders from anywhere the system's folder picker reaches: an external drive, a server connected in Files or Finder, or another folder on the Mac. Each game in it is a subfolder, as in VNDegen's own folder. Add game asks which library folder to unpack an archive into when there is more than one.

On iPhone and iPad, VNDegen keeps a bookmark with permission to read and write the folder, since games write their saves into their own folders, and keeps that permission from launch until you remove the folder. On a Mac the bookmark finds the folder again after it is moved or renamed.

A folder that can't be reached, such as an unplugged drive, keeps its games in the library, shown as unavailable with the reason, and they come back on the next rescan once the drive or server is back. Remove takes a folder's games out of the library and leaves its files alone. A removed folder that is added back gets its games' records back.

Two folders can each hold a game of the same name, even when the names differ only in case. The second one's library ID adds its folder's name, so each keeps its own artwork, saves, and settings, and that game's identity is local to the device.

Known limit: on a Mac, if an added folder is moved and another folder is made at its old path, VNDegen finds the new one there, because macOS resolves a bookmark by path first. Remove that folder and add the right one.

## Removing a game

Remove from library, in a game's menu or on its page, deletes the game's folder after asking: on a Mac it goes to the Trash, and on iPhone and iPad it is deleted for good. Saves the game writes inside its own folder go with it. The saves VNDegen keeps in its own data folder for the game stay, and so do its artwork, play time, and settings, so copying the folder back restores it.

## Removing engine files

RPG Maker MV and MZ, TyranoScript, and Ren'Py games ship PC engine files that VNDegen doesn't use. Remove engine files, in a game's menu or on its page, lists them with the space they take and asks before removing anything. An archive you import asks once, when the game it adds has any. The game keeps running in VNDegen, but it won't run on a PC from that folder afterwards.

| Engine | Average saved per game | What goes |
|---|---|---|
| RPG Maker MZ | 295 MB | NW.js |
| RPG Maker MV | 148 MB | NW.js |
| TyranoScript | 183 MB | NW.js or Electron |
| Ren'Py | 88 MB | The Windows, Linux, and Mac libraries in `lib/`, and the `.exe` and `.app` launchers |

The averages come from the maintainer's library. Each plugin names exactly what it may remove, and anything else stays, including saves, the game's own data, and whatever a mod or a translation added, such as a picture or a settings file left in `lib/`. The exception is a program or Python file a mod put in one of Ren'Py's `lib/` folders, which VNDegen can't tell from the engine's own. VNDegen loads nothing from those folders, so the game runs as before, but the mod's file is gone. An `.exe` under another name goes only when it's the one file of 30 MB or more beside the runtime's files. A game packed entirely inside its `.exe` isn't detected as these engines and offers nothing. Nothing is offered without the game's data on disk, and no `.exe` of 250 MB or more is offered. Known limit: a smaller `.exe` that holds more of the game than its loose files is treated as the runtime. No library game is laid out that way. The other engines list nothing, because VNDegen reads their `.exe` and `.dll` files or names to recognize or run a game, or because there is little to gain.

Where the files go depends on where the game is:

- **Mac.** They go to the Trash. The space comes back when you empty it.
- **iPhone and iPad.** They are deleted for good.
- **iCloud Drive.** The removal reaches every device on that iCloud account. iCloud Drive's Recently Deleted should keep the files for about 30 days. Files not downloaded yet are measured and removed without downloading them.

VNDegen shows the reason and removes nothing for a folder that can't be reached, one it can't write to, a network share, or a cloud service other than iCloud Drive. On iPhone and iPad that also covers folders you added from On My iPhone or iPad outside VNDegen's own folder, because VNDegen can't tell those from another provider's. VNDegen's own Games folder isn't affected. If some files can't be removed, it lists them with what it did remove. A game whose engine you assigned by hand offers nothing.

## Layouts

The view button in the top bar switches the library between three layouts:

- **Header list** stacks each game's wide header art, in as many columns as the screen allows.
- **Capsule rows** shows shelves of large portrait capsules.
- **Featured** puts the focused game's hero art, logo, and Play button above the shelves.

iPhone starts on Header list, iPad on Capsule rows, and Mac on Featured, and each device remembers its choice.

The hub adapts to iPhone portrait and landscape, both iPhone Duo displays, iPad, and Mac windows of any size. Folding the Duo or rotating the iPad keeps your place. On iPad, VNDegen also runs in a window of its own, at any size iPadOS allows. The library, game pages, the in-game menu, the touch gamepad, and every engine fit the window and keep clear of its window controls.

## The game menu

A long press on a game in any library layout, a right click on a Mac, or X on a controller opens a menu with Play, Open game page, Change artwork, Find metadata, Hide or Unhide, Show in Finder or Open in Files, Remove engine files for the engines that list any, and Remove from library.

<p align="center">
  <img src="screenshots/game-menu.jpg" width="260" alt="The menu a long press opens on a library game on iPhone: Play, Open game page, Change artwork, Find metadata, Hide, Open in Files, and Remove from library">
</p>

## Hidden games

Games marked Hide disappear from the library until it is unlocked with Face ID, Touch ID, or an app passcode. The library locks again when the app goes to the background.

## In-game orientation

On iPhone, games lock to landscape by default. On iPad the lock applies only when VNDegen is full screen. In a window, tiled or not, a game follows the window's shape. You can change the setting globally or per game: Landscape, Portrait, or Follow device.

## Artwork and metadata

VNDegen fetches capsule, hero, logo, header, and icon art plus descriptions, developers, release dates, genres, and screenshots from the Steam store, SteamGridDB, IGDB, and VNDB. Every artwork slot can be replaced with another source's image or one imported from Photos or Files.

Steam and VNDB work without setup. SteamGridDB and IGDB need keys, entered in Settings, Metadata Sources. The keys are stored in the Keychain.

- **SteamGridDB.** An API key from your SteamGridDB profile preferences.
- **IGDB.** The client ID and secret of a Twitch developer application.

A game page's About section takes its description, genres, and screenshots from Steam, or from IGDB when Steam has none. The match you confirm in Find metadata supplies the other details, and the remaining sources fill any gaps.

The screenshots fill a grid whose column count follows the window: two on an iPhone held upright, and more on an iPad or a wide Mac window, where tiles drawn wider than Steam's thumbnails load the full-size image. A long list shows two rows and a Show all button. A tap, a click, or A on a screenshot opens it full screen. Swipes, the arrow buttons and keys, and a controller's left and right move between screenshots, and B, Escape, or the close button go back to the page.
