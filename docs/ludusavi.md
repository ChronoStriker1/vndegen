# Ludusavi saves

VNDegen backs up and restores [Ludusavi](https://github.com/mtkennerly/ludusavi) backups where Ludusavi's cloud sync keeps them: an SMB share, an FTP server, or a WebDAV server. Saves move between VNDegen and the PC copies Ludusavi manages. It matches games by itself, offers newer saves before a game starts, and restores can be undone.

It has rclone's own SMB, FTP, and WebDAV code built in, the code Ludusavi's rclone runs on the PCs, so it works the same on iPhone, iPad, and Mac with nothing to install. Ludusavi's other cloud types, the ones that need a sign-in, aren't supported yet.

## Setting it up

Set it up once in Settings, under Ludusavi backups. Pick the type Ludusavi's cloud settings show, then:

1. Enter the same details Ludusavi's cloud settings show for it:
   - **SMB:** the host, the port (445 unless you changed it), and the username and password. Guest is on for a new setup and matches Ludusavi's `guest` username, which logs in with no password.
   - **FTP:** the host, the port (21 unless you changed it), and the username and password.
   - **WebDAV:** the URL, the provider (Other, Nextcloud, Owncloud, Sharepoint, or Sharepoint (NTLM)), and the username and password.
2. Enter the path from Ludusavi's cloud settings. For SMB it is the share, then the folder inside it, such as `games/Ludusavi`. For FTP and WebDAV it is the folder Ludusavi backs up to, such as `ludusavi-backup`, typed exactly as Ludusavi has it: on FTP, `/ludusavi-backup` and `ludusavi-backup` can be different folders.
3. Tap Test Connection. VNDegen lists the folder and shows how many games it found. The test only reads.
4. Turn on Match my Ludusavi settings and enter the retention limits from Ludusavi's Other screen. On a Mac with Ludusavi installed, VNDegen reads them from Ludusavi's config.

FTP sends your password and your saves unencrypted, as Ludusavi's FTP does, so anyone on the network between you and the server can read them. Use it only on a network you trust.

Each type keeps its own password. Trying another type, or switching back, never deletes the password you entered for the one before. Forget Server deletes the password of the server you forget.

The password stays on the device and out of VNDegen's settings file. On iPhone and iPad it is in the Keychain, for this device only. On a Mac it is in a file only your user can read, in VNDegen's Application Support folder, so app updates don't ask for your password. rclone gets it obscured, in a config that lives in memory and never touches an rclone.conf.

VNDegen never uploads with a sync. Ludusavi's upload is `rclone sync`, which deletes whatever the PC lacks. VNDegen only copies its new backup folder and then its mapping.yaml, so it never removes another device's backups.

## When two devices back up at once

Each game's mapping.yaml lists its backups. VNDegen first writes its new list next to the old one under a name of its own, reads the server's list again, and only if nobody changed it since VNDegen last read it renames the new list over the old one. If another device got there first, VNDegen records nothing and asks you to try again. Neither SMB, FTP, nor WebDAV lets VNDegen check and rename in a single step, so there is a short gap between the check and the rename. A backup another device finishes in that gap, a few round trips to the server, can still be overwritten. How each type does the rename:

- **SMB:** the rename replaces the old list in one step, so there is never a moment with no list.
- **FTP:** the same as SMB on servers whose rename replaces an existing file, as vsftpd, ProFTPD, and Pure-FTPd do. Some servers refuse to rename over an existing file. On those, a backup stops with a message saying VNDegen can't back up to that server, and the old list stays as it was. The server must also accept `LIST -a`, which VNDegen uses so it can see files whose names start with a dot, such as Linux saves under `.local`. A server that doesn't is reported as having no such folder when you test the connection.
- **WebDAV:** weaker than SMB. WebDAV servers replace a file by deleting the old one and then moving the new one into its place, so for a moment inside the server there is no list for that game. Another VNDegen that reads the game then just doesn't see it until it next checks. If Ludusavi on a PC downloads at that exact moment, it removes that game's list from the PC until its next download brings it back. The backups themselves are never touched.

If the connection drops while VNDegen is copying a backup, nothing lists the half-copied folder, and VNDegen deletes it when it can. A server that stops answering ends the backup after 30 seconds, and the next one tries again. A slow backup or restore that is still moving runs as long as it needs: VNDegen gives up only when rclone's transfer counters for it stay still for a minute.

VNDegen 0.4.0 connected over SSH and rsync. Its settings turn save sync off after the update, with a note in Settings, until you enter the share. VNDegen deletes the SSH key it made then.

## Using it

Each game then finds its Ludusavi game by itself: by the Steam app ID in its metadata through Ludusavi's manifest, else by title. You can pick another on the game's Saves page. That page lists the game's backups with their date, source device, and kind, says whether the saves here or on the server are newer, and previews what a restore changes.

- **Restore** brings a backup down and copies it into the game's save folder, after keeping a copy of the current saves for Undo.
- **Back up now** writes a full or differential backup in Ludusavi's format, as Ludusavi would, with the same file paths as your existing backups, so Ludusavi restores it on the PC and Steam Deck. A game with no backup yet goes where the PC keeps it, under the Steam library the other backups use. With retention limits set, backups beyond them leave the backup list, as Ludusavi drops them. Their folders stay on the server until Ludusavi on a PC deletes them at its next backup of that game, because another device may still list them. Each device names its backup folders with an ID of its own, and VNDegen checks the server's backup list again right before it adds to it, so a backup from another device is not overwritten.
- **Before a game starts**, VNDegen asks when another device backed up newer saves, or when the saves changed both here and on the server.
- **Automatic backup** when a game closes is available and off by default. It skips a game whose saves changed in both places and leaves the choice to you.

A backup from a Windows PC can hold registry data beside its files. VNDegen's own backups carry it forward, so Ludusavi on the PC still restores it from the latest backup. Unity games on a Mac that run through CrossOver keep their PlayerPrefs, often their settings and sometimes their progress, in the registry under `HKEY_CURRENT_USER\Software\<company>\<product>`. For those games alone, a restore applies that one key's values from the backup, and a backup writes them back into the backup's `registry.reg` as Ludusavi writes it, with every other key it holds unchanged. VNDegen never applies any other registry data.

A game whose `mapping.yaml` lists one file twice, as a hand edit can, or under two names that differ only in Unicode normalization, as a Linux disk can hold, can't be read, and VNDegen leaves that game's backups alone. The other games sync as usual.

Ludusavi on a PC that syncs its backup folder with rclone sees VNDegen's backups as changes in the cloud. When Ludusavi asks, choose to download them.

Where each engine keeps its saves, and which saves Ludusavi's manifest covers, is in [engines.md](engines.md).
