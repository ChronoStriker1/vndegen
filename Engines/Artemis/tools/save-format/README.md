# Save format tools

Python scripts that dump the PC Artemis engine's save files, used to work out the format in `docs/artemis-save-format.md`.

- `bow.py <file>` lists the sections of a `system.dat`, `saveg.dat`, or numbered save.
- `tree.py <file> [field...]` dumps a numbered save's fields as objects, tags, and lists.
- `pluto.py <save> <variable>` decodes the Pluto data stored in a variable, such as `lua.scr`.

They read Shift_JIS strings. Run them with Python 3 on saves copied out of a game's save folder.
