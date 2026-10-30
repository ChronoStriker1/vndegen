# Save format tools

Python scripts that dump the PC Artemis engine's save files, used to work out the format in `docs/artemis-save-format.md`.

- `bow.py <file>` lists the sections of a `system.dat`, `saveg.dat`, or numbered save.
- `tree.py <file> [field...]` dumps a numbered save's fields as objects, tags, and lists.
- `pluto.py <save> <variable>` decodes the Pluto data stored in a variable, such as `lua.scr`.
- `splice.py <base> <donor> <out> field...` writes a copy of a numbered save with the given root fields taken from another save. Loading such copies in the PC engine shows which field a difference lives in.

These read version 1000 saves. For version 1002:

- `t1002.py <file> [field...]` lists a save's root fields, with the tables of the objects inside each.
- `frames1002.py <save>...` prints a save's position and call frames, with the tag each frame records.
- `fonts1002.py <save>...` prints each message layer's font.

They read Shift_JIS strings. Run them with Python 3 on saves copied out of a game's save folder.
