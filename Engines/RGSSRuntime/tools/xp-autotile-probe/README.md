# XP autotile probe

How the quarter table for RPG Maker XP's autotiles in `CRGSS/core/tilemap.c` was measured, with RPG Maker XP's own
engine as a black box.

`probe.rb` is a game script: it fills each 16x16 quarter of a 96x128 autotile with its own red value (5 times its
index, reading left to right, top to bottom), puts the 48 autotile shapes on a map in an 8x6 grid, two tiles apart,
and marks the screen's corner with a magenta square. It runs as the only script of an XP game, with the game's
`Game.exe` and `RGSS104J.dll`, under Wine. A screenshot then gives each shape's four quarters: the red value at the
center of each quarter of the tile at (32 + 64 * (shape % 8), 32 + 64 * (shape / 8)) from the marker, divided by 5,
is the source quarter's index. Every sample had green 128 and blue 64 and a red value divisible by 5.
