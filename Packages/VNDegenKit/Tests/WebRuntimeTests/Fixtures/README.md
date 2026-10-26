`tone.ogg` is one second of a 440 Hz sine in stereo Ogg Vorbis, made with:

    ffmpeg -f lavfi -i "sine=frequency=440:duration=1:sample_rate=44100" -ac 2 -c:a libvorbis -q:a 0 tone.ogg

`mono.ogg` is one second of a 440 Hz sine in mono at 22.05 kHz, made with:

    ffmpeg -f lavfi -i "sine=frequency=440:duration=1:sample_rate=22050" -ac 1 -c:a libvorbis -q:a 0 -map_metadata -1 -fflags +bitexact -flags:a +bitexact mono.ogg
