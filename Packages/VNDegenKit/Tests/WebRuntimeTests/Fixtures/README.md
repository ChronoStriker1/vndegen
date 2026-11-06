`tone.ogg` is one second of a 440 Hz sine in stereo Ogg Vorbis, made with:

    ffmpeg -f lavfi -i "sine=frequency=440:duration=1:sample_rate=44100" -ac 2 -c:a libvorbis -q:a 0 tone.ogg

`mono.ogg` is one second of a 440 Hz sine in mono at 22.05 kHz, made with:

    ffmpeg -f lavfi -i "sine=frequency=440:duration=1:sample_rate=22050" -ac 1 -c:a libvorbis -q:a 0 -map_metadata -1 -fflags +bitexact -flags:a +bitexact mono.ogg

`alpha-clip.webm` is 0.3 s of half-transparent red, 64 by 64 at 10 fps, in VP9 with an alpha channel, made with:

    ffmpeg -f lavfi -i "color=c=red@0.5:s=64x64:d=0.3:r=10,format=yuva420p" -c:v libvpx-vp9 -pix_fmt yuva420p -auto-alt-ref 0 -b:v 50k alpha-clip.webm

`opaque-clip.webm` is the same length of blue without alpha:

    ffmpeg -f lavfi -i "color=c=blue:s=64x64:d=0.3:r=10" -c:v libvpx-vp9 -pix_fmt yuv420p opaque-clip.webm
