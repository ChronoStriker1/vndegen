`alpha-loop.webm` is a 16 by 16 VP9 clip with an alpha channel: four frames, 0.14 s each, of a red, green, blue, then
yellow square on a transparent background whose color plane is white, as GIFs converted to WebM have it. It was made
from four PNGs (`f0.png` to `f3.png`, each the square on a transparent 16 by 16 canvas) with:

    ffmpeg -framerate 50/7 -i f%d.png -c:v libvpx-vp9 -pix_fmt yuva420p -auto-alt-ref 0 -lossless 1 alpha-loop.webm
