# Ludusavi saves

VNDegen backs up and restores [Ludusavi](https://github.com/mtkennerly/ludusavi) backups on your SMB share, the one Ludusavi's cloud sync uses, so saves move between VNDegen and the PC copies Ludusavi manages. It matches games by itself, offers newer saves before a game starts, and restores can be undone.

It has rclone's own SMB code built in, the code Ludusavi's rclone runs on the PCs, so it works the same on iPhone, iPad, and Mac with nothing to install.

## Setting it up

Set it up once in Settings, under Ludusavi backups:

1. Enter what Ludusavi's cloud settings show for its SMB remote: the host, the port (445 unless you changed it), and the username and password. Guest is on for a new setup and matches Ludusavi's `guest` username, which logs in with no password.
2. Enter the path from Ludusavi's cloud settings. It is the share, then the folder inside it, such as `games/Ludusavi`.
3. Tap Test Connection. VNDegen lists the folder and shows how many games it found. The test only reads.
4. Turn on Match my Ludusavi settings and enter the retention limits from Ludusavi's Other screen. On a Mac with Ludusavi installed, VNDegen reads them from Ludusavi's config.

The password stays on the device and out of VNDegen's settings file. On iPhone and iPad it is in the Keychain, for this device only. On a Mac it is in a file only your user can read, in VNDegen's Application Support folder, so app updates don't ask for your password. rclone gets it obscured, in a config that lives in memory and never touches an rclone.conf.

VNDegen never uploads with a sync. Ludusavi's upload is `rclone sync`, which deletes whatever the PC lacks. VNDegen only copies its new backup folder and then its mapping.yaml, so it never removes another device's backups.

VNDegen 0.4.0 connected over SSH and rsync. Its settings turn save sync off after the update, with a note in Settings, until you enter the share. VNDegen deletes the SSH key it made then.

## Using it

Each game then finds its Ludusavi game by itself: by the Steam app ID in its metadata through Ludusavi's manifest, else by title. You can pick another on the game's Saves page. That page lists the game's backups with their date, source device, and kind, says whether the saves here or on the server are newer, and previews what a restore changes.

- **Restore** brings a backup down and copies it into the game's save folder, after keeping a copy of the current saves for Undo.
- **Back up now** writes a full or differential backup in Ludusavi's format, as Ludusavi would, with the same file paths as your existing backups, so Ludusavi restores it on the PC and Steam Deck. A game with no backup yet goes where the PC keeps it, under the Steam library the other backups use. With retention limits set, backups beyond them leave the backup list, as Ludusavi drops them. Their folders stay on the server until Ludusavi on a PC deletes them at its next backup of that game, because another device may still list them. Each device names its backup folders with an ID of its own, and VNDegen checks the server's backup list again right before it adds to it, so a backup from another device is not overwritten.
- **Before a game starts**, VNDegen asks when another device backed up newer saves, or when the saves changed both here and on the server.
- **Automatic backup** when a game closes is available and off by default. It skips a game whose saves changed in both places and leaves the choice to you.

Ludusavi on a PC that syncs its backup folder with rclone sees VNDegen's backups as changes in the cloud. When Ludusavi asks, choose to download them.

Where each engine keeps its saves, and which saves Ludusavi's manifest covers, is in [engines.md](engines.md).
