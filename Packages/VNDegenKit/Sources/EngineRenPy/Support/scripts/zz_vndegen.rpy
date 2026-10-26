# VNDegen's hook. VNDegen's launcher adds this file's folder to Ren'Py's search path, so every game it runs loads
# this file. Nothing here changes the game's own files. Valid on Ren'Py 6.99 to 8.

init 999 python in _vndegen:
    import os
    import time
    import pygame

    from store import config, renpy, _preferences, Return

    # The backtick and the controller's View or Share and Home buttons open the VNDegen menu, as in every other
    # VNDegen engine. Ren'Py binds those two buttons to rollback and the game menu, which the game's own Menu and B
    # buttons still reach.
    config.keymap["vndegen_menu"] = ["K_BACKQUOTE"]
    for name in ["pad_back_press", "repeat_pad_back_press", "pad_guide_press"]:
        config.pad_bindings[name] = ["vndegen_menu"]

    menu_open = False

    def open_menu():
        global menu_open
        if menu_open:
            return
        menu_open = True
        try:
            leave = renpy.invoke_in_new_context(renpy.call_screen, "_vndegen_menu")
        finally:
            menu_open = False
        if leave:
            # What Ren'Py's own Quit action does after the player confirms.
            if not renpy.store.main_menu and config.autosave_on_quit:
                renpy.force_autosave()
            renpy.quit(save=True)

    config.underlay.append(renpy.Keymap(vndegen_menu=open_menu))

    # The game window takes the hub window's place: full screen when the hub is, otherwise its exact frame.
    # VNDEGEN_WINDOW is "x,y,width,height" in screen points, with the origin at the top left of the main display.
    _preferences.fullscreen = os.environ.get("VNDEGEN_FULLSCREEN") == "1"
    window = os.environ.get("VNDEGEN_WINDOW")
    if window and not _preferences.fullscreen:
        x, y, width, height = [int(round(float(v))) for v in window.split(",")]
        _preferences.maximized = False
        _preferences.physical_size = (width, height)

        Interface = renpy.display.core.Interface
        set_mode = Interface.set_mode

        def place_window(self):
            # Ren'Py fits its first window to the game's aspect ratio, so the exact frame is set once it exists.
            Interface.set_mode = set_mode
            set_mode(self)
            window = pygame.display.get_window()
            window.resize((width, height), opengl=True)
            window.set_position((x, y))

        Interface.set_mode = place_window

    # VNDEGEN_PROBE is set only by scripts/renpy-compat.sh. It clicks through splash screens until the main menu
    # shows, writes the result and a screenshot to the folder it names, and quits.
    probe = os.environ.get("VNDEGEN_PROBE")
    if probe:
        probe_start = time.time()
        probe_timeout = float(os.environ.get("VNDEGEN_PROBE_TIMEOUT", "45"))
        probe_next = [probe_start + 1.0, probe_start + 5.0]

        def probe_finish(result):
            renpy.screenshot(os.path.join(probe, "screen.png"))
            with open(os.path.join(probe, "result.txt"), "w") as f:
                f.write(result + "\n")
            renpy.quit()

        def probe_tick():
            now = time.time()
            if now < probe_next[0]:
                return
            probe_next[0] = now + 1.0
            if renpy.store.main_menu or renpy.get_screen("main_menu"):
                probe_finish("main-menu")
            elif now - probe_start > probe_timeout:
                try:
                    showing = " ".join(sorted(renpy.get_showing_tags("screens")))
                except Exception:
                    showing = ""
                probe_finish("no-main-menu " + (showing or "-"))
            elif now >= probe_next[1]:
                probe_next[1] = now + 4.0
                renpy.queue_event("dismiss")

        config.periodic_callbacks.append(probe_tick)

style _vndegen_frame:
    xalign 0.5
    yalign 0.5
    xpadding 48
    ypadding 36
    background Solid("#14141ce8")

style _vndegen_text:
    font "DejaVuSans.ttf"
    color "#ffffff"
    outlines []
    xalign 0.5

style _vndegen_button:
    xalign 0.5
    xminimum 360
    xpadding 24
    ypadding 12
    background Solid("#ffffff1a")
    hover_background Solid("#ff4080")

style _vndegen_button_text is _vndegen_text

screen _vndegen_menu():
    modal True
    zorder 10000

    key "vndegen_menu" action Return(False)
    key "game_menu" action Return(False)

    add Solid("#0000008c")

    frame:
        style "_vndegen_frame"
        has vbox
        spacing 16

        text "VNDegen" style "_vndegen_text" size (config.screen_height // 18) bold True
        null height 8
        textbutton "Resume" action Return(False) style "_vndegen_button" text_style "_vndegen_button_text" text_size (config.screen_height // 30)
        textbutton "Quit to library" action Return(True) style "_vndegen_button" text_style "_vndegen_button_text" text_size (config.screen_height // 30)
