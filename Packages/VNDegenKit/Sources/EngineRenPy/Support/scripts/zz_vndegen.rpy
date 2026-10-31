# VNDegen's hook. VNDegen's launcher adds this file's folder to Ren'Py's search path, so every game it runs loads
# this file. Nothing here changes the game's own files. Valid on Ren'Py 6.99 to 8.
#
# On iOS every game runs in the app's one interpreter, and Ren'Py's reload between games keeps patched classes and
# functions. So each patch below wraps the original it saved in vndegen_original, never the previous game's patch.

init 999 python in _vndegen:
    import os
    import signal
    import sys
    import time
    import pygame

    from store import config, renpy, _preferences, Return

    # The backtick and the controller's Home button open the VNDegen menu, as in every other VNDegen engine. Ren'Py
    # binds Home to the game menu, which the game's own Menu button still reaches. View keeps Ren'Py's own binding.
    config.keymap["vndegen_menu"] = ["K_BACKQUOTE"]
    config.pad_bindings["pad_guide_press"] = ["vndegen_menu"]

    menu_open = False

    def has_quicksave(node):
        import ast
        for name, expression in getattr(node, "keyword", []):
            if name == "action" and any(isinstance(call, ast.Call) and
                    (getattr(call.func, "id", None) == "QuickSave" or
                     getattr(call.func, "attr", None) == "QuickSave")
                    for call in ast.walk(ast.parse(expression, mode="eval"))):
                return True
        return any(has_quicksave(child) for child in getattr(node, "children", [])) or any(
            has_quicksave(block) for condition, block in getattr(node, "entries", []))

    # Use Ren'Py's device variant, once per game. Unknown screens keep the quick-save fallback.
    quick_menu_has_quicksave = False
    if renpy.ios:
        try:
            screen = renpy.display.screen.get_screen_variant("quick_menu")
            quick_menu_has_quicksave = has_quicksave(screen.ast) if screen and screen.ast else False
        except Exception:
            pass
        print("VNDegen quick_menu QuickSave: %s" % quick_menu_has_quicksave)

    def quit_game():
        # What Ren'Py's own Quit action does after the player confirms.
        if not renpy.store.main_menu and config.autosave_on_quit:
            renpy.force_autosave()
        renpy.quit(save=True)

    def open_menu():
        global menu_open
        if menu_open:
            return
        # On iPhone and iPad, Ren'Py shows a game's "touch" screens, and the touch quick menu of many games, such as
        # Servant's Revenge's, drops Q.Save and Q.Load. Offer the game's quick save slot when its menu lacks QuickSave.
        quick = renpy.ios and config.has_quicksave and not quick_menu_has_quicksave
        can_save = quick and not renpy.store.main_menu and not renpy.store._in_replay
        can_load = quick and renpy.store.QuickLoad().get_sensitive()
        menu_open = True
        try:
            choice = renpy.invoke_in_new_context(renpy.call_screen, "_vndegen_menu", quick=quick, can_save=can_save,
                                                 can_load=can_load)
        finally:
            menu_open = False
        # The game's own context runs these, after VNDegen's menu has closed, so the save's screenshot is the game's.
        if choice is True:
            quit_game()
        elif choice == "quicksave":
            renpy.display.behavior.run(renpy.store.QuickSave())
        elif choice == "quickload":
            renpy.display.behavior.run(renpy.store.QuickLoad())

    config.underlay.append(renpy.Keymap(vndegen_menu=open_menu))

    # VNDegen sends SIGTERM when the player quits from the hub or quits VNDegen. SDL would turn it into a quit event,
    # which shows Ren'Py's quit prompt, so the game quits as if the player had confirmed instead. A signal can arrive
    # anywhere, even in the middle of a save, so the handler only sets a flag for the next periodic callback.
    quit_requested = False

    def request_quit(signum, frame):
        global quit_requested
        quit_requested = True

    def quit_if_requested():
        # Cleared first, because the game's own quit label can run interactions that call this again.
        global quit_requested
        if quit_requested:
            quit_requested = False
            quit_game()

    # Tells VNDegen the game booted, for the game page. Ren'Py's traceback.txt beside it tells it when a game failed.
    main_menu_noted = [not os.environ.get("VNDEGEN_DATA")]

    def note_main_menu():
        if not main_menu_noted[0] and (renpy.store.main_menu or renpy.get_screen("main_menu")):
            main_menu_noted[0] = True
            open(os.path.join(os.environ["VNDEGEN_DATA"], "logs", "main-menu"), "w").close()

    config.periodic_callbacks.append(note_main_menu)

    # On iOS VNDegen's launcher answers the hub instead, through quit_game.
    if not renpy.ios:
        signal.signal(signal.SIGTERM, request_quit)
        config.periodic_callbacks.append(quit_if_requested)

    # A text prompt, such as a name entry, swallows the backtick's key press and types it. It passes it on instead, so
    # the backtick still opens the menu there, and never types.
    Input = renpy.display.behavior.Input
    input_event = getattr(Input.event, "vndegen_original", Input.event)

    def input_event_without_menu_key(self, ev, x, y, st):
        if renpy.display.behavior.map_event(ev, "vndegen_menu") or (ev.type == pygame.TEXTINPUT and ev.text == "`"):
            return None
        return input_event(self, ev, x, y, st)

    input_event_without_menu_key.vndegen_original = input_event
    Input.event = input_event_without_menu_key

    # The game window takes the hub window's place: full screen when the hub is, otherwise its exact frame.
    # VNDEGEN_WINDOW is the hub window's frame, "x,y,width,height" in screen points with the origin at the top left of
    # the main display.
    window = os.environ.get("VNDEGEN_WINDOW")
    if not renpy.ios:
        _preferences.fullscreen = os.environ.get("VNDEGEN_FULLSCREEN") == "1"
    if window and not _preferences.fullscreen:
        x, y, width, height = [int(round(float(v))) for v in window.split(",")]
        _preferences.maximized = False
        _preferences.physical_size = (width, height)

        Interface = renpy.display.core.Interface
        set_mode = Interface.set_mode

        def place_window(self):
            # Ren'Py fits its first window to the game's aspect ratio, so the exact frame is set once it exists. The
            # window has no title bar, like the hub window it replaces.
            Interface.set_mode = set_mode
            set_mode(self)
            import ctypes
            sdl = ctypes.CDLL(None)
            sdl.SDL_GL_GetCurrentWindow.restype = ctypes.c_void_p
            sdl.SDL_SetWindowBordered(ctypes.c_void_p(sdl.SDL_GL_GetCurrentWindow()), 0)
            window = pygame.display.get_window()
            window.resize((width, height), opengl=True)
            window.set_position((x, y))

        Interface.set_mode = place_window

    # VNDEGEN_RENPY7_GAME is set for a Ren'Py 7 game on Ren'Py 8. Its persistent data, such as unlocked gallery
    # images, is written so that the game's PC version, on Ren'Py 7 and Python 2, loads it too: in pickle protocol 2,
    # with each class Ren'Py 7.5 moved under its Ren'Py 7.4 module. Every Ren'Py since 7.4 loads the old names. Saves
    # keep Ren'Py 8's format, because older Ren'Py can't load a newer Ren'Py's save state either way.
    if os.environ.get("VNDEGEN_RENPY7_GAME") == "1" and sys.version_info[0] >= 3:
        import io
        import pickle
        import renpy.persistent as persistent_module

        python2_modules = {
            "renpy.revertable": "renpy.python",
            "renpy.rollback": "renpy.python",
            "renpy.display.scenelists": "renpy.display.core",
        }

        class Python2Pickler(pickle._Pickler):
            def save_global(self, obj, name=None):
                old = python2_modules.get(getattr(obj, "__module__", None))
                if old is None:
                    return pickle._Pickler.save_global(self, obj, name)
                self.write(pickle.GLOBAL + (old + "\n" + (name or obj.__qualname__) + "\n").encode("utf-8"))
                self.memoize(obj)

        def python2_dumps(o, highest=False, bad_reduction_name=None):
            f = io.BytesIO()
            Python2Pickler(f, 2).dump(o)
            return f.getvalue()

        persistent_module.dumps = python2_dumps

    # VNDEGEN_BUTTONS carries the game's remapped controller buttons, such as "a=key:K_z,b=click:3,x=pad:a,y=nothing".
    # The game window reads the controller itself, so the remapping happens here: each remapped button gets the Ren'Py
    # bindings of the Xbox button it is mapped to, or posts the key or click it is bound to. Buttons the player left
    # alone keep Ren'Py's own bindings.
    controls = {
        "a": ("pad_a_press", "pad_a_release"),
        "b": ("pad_b_press", "pad_b_release"),
        "x": ("pad_x_press", "pad_x_release"),
        "y": ("pad_y_press", "pad_y_release"),
        "l": ("pad_leftshoulder_press", "pad_leftshoulder_release"),
        "r": ("pad_rightshoulder_press", "pad_rightshoulder_release"),
        "l2": ("pad_lefttrigger_pos", "pad_lefttrigger_zero"),
        "r2": ("pad_righttrigger_pos", "pad_righttrigger_zero"),
        "view": ("pad_back_press", "pad_back_release"),
        "menu": ("pad_start_press", "pad_start_release"),
        "l3": ("pad_leftstick_press", "pad_leftstick_release"),
        "r3": ("pad_rightstick_press", "pad_rightstick_release"),
        "up": ("pad_dpup_press", "pad_dpup_release"),
        "down": ("pad_dpdown_press", "pad_dpdown_release"),
        "left": ("pad_dpleft_press", "pad_dpleft_release"),
        "right": ("pad_dpright_press", "pad_dpright_release"),
        "leftStickUp": ("pad_lefty_neg", "pad_lefty_zero"),
        "leftStickDown": ("pad_lefty_pos", "pad_lefty_zero"),
        "leftStickLeft": ("pad_leftx_neg", "pad_leftx_zero"),
        "leftStickRight": ("pad_leftx_pos", "pad_leftx_zero"),
        "rightStickUp": ("pad_righty_neg", "pad_righty_zero"),
        "rightStickDown": ("pad_righty_pos", "pad_righty_zero"),
        "rightStickLeft": ("pad_rightx_neg", "pad_rightx_zero"),
        "rightStickRight": ("pad_rightx_pos", "pad_rightx_zero"),
    }

    def post_input(kind, value, down):
        if kind == "key":
            key = getattr(pygame, value)
            pygame.event.post(pygame.event.Event(pygame.KEYDOWN if down else pygame.KEYUP, key=key, mod=0, scancode=0,
                                                 unicode=chr(key) if key < 128 else "", repeat=False))
        else:
            pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN if down else pygame.MOUSEBUTTONUP,
                                                 pos=pygame.mouse.get_pos(), button=int(value), which=0, touch=False))

    def remap(buttons):
        defaults = dict(config.pad_bindings)
        # Pad press event name, such as "pad_a_press" or "pad_lefty_neg", to the keys and clicks it holds down.
        holds = {}
        for item in buttons.split(","):
            control, _, action = item.partition("=")
            kind, _, value = action.partition(":")
            if control not in controls:
                continue
            press, release = controls[control]
            if kind == "pad" and value in controls:
                source = controls[value]
                bindings = [defaults.get(source[0], []), defaults.get(source[1], []), defaults.get("repeat_" + source[0], [])]
            else:
                bindings = [[], [], []]
                if kind in ("key", "click"):
                    holds.setdefault(press, []).append((kind, value))
            config.pad_bindings[press], config.pad_bindings[release], config.pad_bindings["repeat_" + press] = bindings

        controller = renpy.display.controller
        post_event = getattr(controller.post_event, "vndegen_original", controller.post_event)
        # The press event each button or stick axis holds down, such as {"lefty": "pad_lefty_neg"}. Ren'Py can report
        # a stick going straight from neg to pos, with no zero between, so a new direction first releases the old one.
        held = {}

        def post_remapped(control, state, repeat):
            if not repeat:
                released = held.pop(control, None)
                for kind, value in holds.get(released, []):
                    post_input(kind, value, False)
                if state not in ("release", "zero") and renpy.display.interface.keyboard_focused:
                    press = "pad_{}_{}".format(control, state)
                    held[control] = press
                    for kind, value in holds.get(press, []):
                        post_input(kind, value, True)
            post_event(control, state, repeat)

        post_remapped.vndegen_original = post_event
        controller.post_event = post_remapped

    if os.environ.get("VNDEGEN_BUTTONS"):
        remap(os.environ["VNDEGEN_BUTTONS"])

    # VNDEGEN_PROBE is set only by test runs, such as scripts/renpy-compat.sh. It runs the steps in VNDEGEN_PROBE_STEPS
    # one at a time, writes screenshots and a result line to the folder VNDEGEN_PROBE names, and quits at the end. The
    # default steps click through splash screens until the main menu shows. Steps, separated by spaces:
    #   menu         dismiss splash screens and press first choices and buttons until the main menu shows, or fail after
    #                VNDEGEN_PROBE_TIMEOUT seconds
    #   start        what the main menu's Start button does
    #   wait:<s>     wait
    #   dismiss      what a click on the dialogue does
    #   shot:<name>  save a screenshot as <name>.png
    #   key:<K_x>    press and release a key, as the keyboard would
    #   pad:<name>   press and release a controller button, by its SDL name such as guide, back, or a
    #   axis:<name>:<state>
    #                move a controller axis, such as axis:lefty:neg for the left stick up; states are neg, pos, and zero
    #   save:<slot>  what a save slot's button does on the save screen; load:<slot> is the same on the load screen
    #   restore:<save>
    #                load a save by its file name, such as 1-1, with no confirmation, as renpy.load does
    #   quit         quit, with the result "main-menu" if a menu step got there; without it the game stays open
    probe = os.environ.get("VNDEGEN_PROBE")
    if probe:
        probe_start = time.time()
        probe_timeout = float(os.environ.get("VNDEGEN_PROBE_TIMEOUT", "45"))
        probe_steps = os.environ.get("VNDEGEN_PROBE_STEPS", "menu shot:screen quit").split()
        probe_state = {"next": probe_start + 1.0, "dismiss": probe_start + 5.0, "action": None, "reached": [], "tries": 0}

        def probe_log(line):
            with open(os.path.join(probe, "steps.txt"), "a") as f:
                f.write("%.1f %s\n" % (time.time() - probe_start, line))

        def probe_finish(result):
            with open(os.path.join(probe, "result.txt"), "w") as f:
                f.write(result + "\n")
            renpy.quit()

        def probe_run():
            # Actions run here, inside an interaction, as a click on a button would run them.
            action, probe_state["action"] = probe_state["action"], None
            if action is not None:
                return renpy.run(action)

        # Registered with no keys, so Ren'Py does not read the name as a key and fail on the next key press.
        config.keymap["_vndegen_probe"] = []
        config.underlay.append(renpy.Keymap(_vndegen_probe=probe_run))

        def probe_step():
            now = time.time()
            if not probe_steps or now < probe_state["next"]:
                return
            step, _, arg = probe_steps[0].partition(":")
            probe_state["next"] = now + 0.5
            if step == "menu":
                if renpy.store.main_menu or renpy.get_screen("main_menu"):
                    probe_log("main-menu")
                    probe_state["reached"].append("main-menu")
                    probe_steps.pop(0)
                elif now - probe_start > probe_timeout:
                    try:
                        showing = " ".join(sorted(renpy.get_showing_tags("screens")))
                    except Exception:
                        showing = ""
                    renpy.screenshot(os.path.join(probe, "screen.png"))
                    probe_finish("no-main-menu " + (showing or "-"))
                elif now >= probe_state["dismiss"]:
                    probe_state["dismiss"] = now + 4.0
                    probe_state["tries"] += 1
                    choice = renpy.get_screen("choice")
                    if choice and choice.scope.get("items"):
                        # Such as a first-run language picker.
                        probe_log("choice " + choice.scope["items"][0].caption)
                        probe_state["action"] = choice.scope["items"][0].action
                        renpy.queue_event("_vndegen_probe")
                    elif not renpy.get_screen("say") and probe_state["tries"] % 2:
                        # Such as a disclaimer's continue button: what Down and then Enter on a keyboard do.
                        renpy.queue_event("focus_down")
                        renpy.queue_event("button_select")
                    else:
                        renpy.queue_event("dismiss")
                return
            probe_steps.pop(0)
            probe_log((step + " " + arg).strip())
            if step == "wait":
                probe_state["next"] = now + float(arg)
            elif step == "shot":
                renpy.screenshot(os.path.join(probe, arg + ".png"))
            elif step == "dismiss":
                renpy.queue_event("dismiss")
            elif step == "key":
                post_input("key", arg, True)
                post_input("key", arg, False)
            elif step == "pad":
                probe_log("keyboard_focused %s" % renpy.display.interface.keyboard_focused)
                renpy.display.controller.controller_event(arg, "press")
                renpy.display.controller.controller_event(arg, "release")
            elif step == "axis":
                renpy.display.controller.controller_event(*arg.split(":"))
            elif step == "restore":
                probe_log("restore " + str(renpy.can_load(arg)))
                renpy.load(arg)
            elif step in ("start", "save", "load"):
                if step == "start":
                    probe_state["action"] = renpy.store.Start()
                elif step == "save":
                    probe_state["action"] = renpy.store.FileSave(arg, confirm=False)
                else:
                    probe_state["action"] = renpy.store.FileLoad(arg, confirm=False)
                renpy.queue_event("_vndegen_probe")
                probe_state["next"] = now + 1.0
            elif step == "quit":
                probe_finish(" ".join(probe_state["reached"]) or "done")

        config.periodic_callbacks.append(probe_step)

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

style _vndegen_button_text is _vndegen_text:
    insensitive_color "#ffffff59"

screen _vndegen_menu(quick=False, can_save=False, can_load=False):
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
        if quick:
            textbutton "Quick save" action If(can_save, Return("quicksave")) style "_vndegen_button" text_style "_vndegen_button_text" text_size (config.screen_height // 30)
            textbutton "Quick load" action If(can_load, Return("quickload")) style "_vndegen_button" text_style "_vndegen_button_text" text_size (config.screen_height // 30)
        textbutton "Quit to library" action Return(True) style "_vndegen_button" text_style "_vndegen_button_text" text_size (config.screen_height // 30)
