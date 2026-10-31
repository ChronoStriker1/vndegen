`alpha-loop.webm` is a 64 by 64 VP9 clip with an alpha channel: four frames, 0.14 s each, of a red, green, blue, then
yellow square (pixels 16 to 47) on a transparent background whose color plane is white, as GIFs converted to WebM have
it. It was made from four PNGs (`f0.png` to `f3.png`, each the square on a transparent 64 by 64 canvas) with:

    ffmpeg -framerate 50/7 -i f%d.png -c:v libvpx-vp9 -pix_fmt yuva420p -auto-alt-ref 0 -b:v 0 -crf 10 alpha-loop.webm

It is 64 by 64 because WebKit's VP9 decoder on macOS fails on much smaller frames.
