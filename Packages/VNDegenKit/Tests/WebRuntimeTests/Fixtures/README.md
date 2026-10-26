`tone.ogg` is one second of a 440 Hz sine in stereo Ogg Vorbis, made with:

    ffmpeg -f lavfi -i "sine=frequency=440:duration=1:sample_rate=44100" -ac 2 -c:a libvorbis -q:a 0 tone.ogg
