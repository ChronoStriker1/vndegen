# Spike probe. Copied into a scratch copy of a game, never into the user's library.
init 999 python:
    import time, os

    class _VNDProbe(renpy.Displayable):
        def __init__(self):
            super(_VNDProbe, self).__init__()
            self.last = None
            self.frames = 0

        def render(self, width, height, st, at):
            now = time.time()
            if self.last is None:
                self.last = now
            self.frames += 1
            if now - self.last >= 2.0:
                times = renpy.display.interface.frame_times
                drawn = (len(times) - 1) / (times[-1] - times[0]) if len(times) > 1 else 0
                renpy.write_log("VNDPROBE render_calls_per_second %.1f drawn_frames_per_second %.1f" % (self.frames / (now - self.last), drawn))
                self.frames = 0
                self.last = now
            renpy.redraw(self, 0)
            return renpy.Render(1, 1)

    _vnd_probe = _VNDProbe()
    _vnd_out = os.environ.get("VND_PROBE_OUT", "/tmp")
    _vnd_t0 = time.time()
    _vnd_delay = float(os.environ.get("VND_PROBE_DELAY", "0"))
    _vnd_steps = [(t + _vnd_delay, n) for t, n in [(6.0, "menu"), (7.0, "start"), (14.0, "ingame"), (15.0, "click"), (19.0, "ingame2"), (20.0, "quit")]]

    def _vnd_periodic():
        global _vnd_steps
        if not _vnd_steps or time.time() - _vnd_t0 < _vnd_steps[0][0]:
            return
        name = _vnd_steps.pop(0)[1]
        renpy.write_log("VNDPROBE step " + name)
        if name == "start":
            if renpy.get_screen("main_menu"):
                renpy.run(Start())
            else:
                renpy.write_log("VNDPROBE no main menu, skipping start")
        elif name == "click":
            renpy.queue_event("dismiss")
        elif name == "quit":
            renpy.quit()
        else:
            renpy.screenshot(os.path.join(_vnd_out, "shot-" + name + ".png"))

    config.periodic_callbacks.append(_vnd_periodic)

screen _vnd_probe_screen():
    add _vnd_probe

init 999 python:
    config.overlay_screens.append("_vnd_probe_screen")
