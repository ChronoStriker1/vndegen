// VNDegen's cheats for RPG Maker MV and MZ, on the game's own objects, so a save made afterwards is an ordinary save.
// Injected at document start after rpgmaker.js. VNDegen puts the game's cheat settings in place of __VNDEGEN_CHEATS__.
(function () {
    'use strict';

    let settings = __VNDEGEN_CHEATS__;
    let installed = false;
    // Whether the game objects belong to a game the player started or loaded. The boot scene sets up a new game for the
    // title screen, and starting or loading one from there replaces it, as returning to the title does again.
    let playing = false;

    // The cheats that stay on wrap engine methods once every plugin has loaded, so they wrap what the plugins left.
    // Both MV and MZ start the boot scene through SceneManager.run after the window's load event, which waits for
    // every plugin script.
    document.addEventListener('load', () => {
        const scenes = window.SceneManager;
        if (!scenes || typeof scenes.run !== 'function' || scenes.run.vndegenCheats) return;
        const run = scenes.run;
        scenes.run = function () {
            install();
            return run.apply(this, arguments);
        };
        scenes.run.vndegenCheats = true;
    }, true);

    // A game whose plugins replaced SceneManager.run without calling the one before it never ran the wrapper above. Once
    // a scene exists, boot has started, so every plugin has loaded and the cheats can wrap what they left.
    function installAfterBoot() {
        if (installed || !window.SceneManager || !SceneManager._scene) return;
        install();
        if (window.Scene_Map && SceneManager._scene instanceof Scene_Map) playing = true;
    }

    function install() {
        if (installed || !window.SceneManager || !window.Game_BattlerBase || !window.Game_Player || !window.Game_Actor) return;
        installed = true;
        wrap(SceneManager, 'onSceneStart', (onSceneStart) => function () {
            const scene = this._scene;
            if (window.Scene_Title && scene instanceof Scene_Title) playing = false;
            if (window.Scene_Map && scene instanceof Scene_Map) {
                playing = true;
                // A party from a new game or a save starts full too.
                if (settings.keepsPartyHealthy) fillParty();
            }
            return onSceneStart.apply(this, arguments);
        });
        const battler = Game_BattlerBase.prototype;
        const keepsFull = (target) => settings.keepsPartyHealthy && target.isActor && target.isActor();
        wrap(battler, 'setHp', (setHp) => function (hp) {
            return setHp.call(this, keepsFull(this) ? Math.max(hp, this.mhp) : hp);
        });
        wrap(battler, 'setMp', (setMp) => function (mp) {
            return setMp.call(this, keepsFull(this) ? Math.max(mp, this.mmp) : mp);
        });
        // Skills take their MP cost from _mp directly, not through setMp.
        wrap(battler, 'paySkillCost', (pay) => function () {
            const mp = this._mp;
            const result = pay.apply(this, arguments);
            if (keepsFull(this)) this._mp = mp;
            return result;
        });
        const player = Game_Player.prototype;
        wrap(player, 'canEncounter', (canEncounter) => function () {
            return !settings.noEncounters && canEncounter.apply(this, arguments);
        });
        // What the editor's playtest does while Ctrl is held, without its side effect of no encounters.
        wrap(player, 'isThrough', (isThrough) => function () {
            return settings.walksThroughWalls || isThrough.apply(this, arguments);
        });
        wrap(Game_Actor.prototype, 'finalExpRate', (rate) => function () {
            return rate.apply(this, arguments) * settings.experienceRate;
        });
        // Game speed runs more engine updates per displayed frame. MZ counts the updates a frame takes in
        // determineRepeatNumber. MV runs an update for each 1/60 s its clock has moved, so its clock runs faster.
        if (typeof SceneManager.determineRepeatNumber === 'function') {
            // MZ counts 2 after slow frames, to catch up. Multiplied by the speed, that made each next frame slow too, so
            // the count stayed at 2: No Socks RPG ran 8 updates a frame at 4x and drew 23 frames a second, not 38. A
            // frame that spent over half its time on its own work is not caught up after. One that did not is on a slow
            // screen, such as at 30 Hz, and still needs its count for the game to run at the chosen speed.
            let timedApp = null, started = 0, spent = 0;
            wrap(SceneManager, 'determineRepeatNumber', (repeat) => function (deltaTime) {
                const count = repeat.apply(this, arguments);
                const app = window.Graphics && Graphics._app;
                if (app && app !== timedApp) {
                    timedApp = app;
                    wrap(app, 'render', (render) => function () {
                        const result = render.apply(this, arguments);
                        spent = performance.now() - started;
                        return result;
                    });
                }
                started = performance.now();
                const selfMade = settings.speed > 1 && count > 1 && spent * 2 > deltaTime * 1000 / 60;
                // A frame that draws nothing leaves no time behind for the next.
                spent = 0;
                return (selfMade ? 1 : count) * settings.speed;
            });
        } else if (typeof SceneManager._getTimeInMsWithoutMobileSafari === 'function') {
            let real = null, virtual = 0;
            wrap(SceneManager, '_getTimeInMsWithoutMobileSafari', (time) => function () {
                const now = time.apply(this, arguments);
                virtual = real === null ? now : virtual + (now - real) * settings.speed;
                real = now;
                return virtual;
            });
        }
    }

    function wrap(owner, name, make) {
        const original = owner[name];
        if (typeof original === 'function') owner[name] = make(original);
    }

    function inGame() {
        return playing && !!(window.$gameParty && window.$gameVariables && window.$dataSystem);
    }

    function party() {
        return window.$gameParty ? $gameParty.allMembers() : [];
    }

    function fillParty() {
        for (const actor of party()) {
            actor.setHp(actor.mhp);
            actor.setMp(actor.mmp);
        }
    }

    // A value as the page shows it: a whole number, text, or a description of anything else, which it doesn't edit.
    function value(raw) {
        if (typeof raw === 'number' && Number.isSafeInteger(raw)) return raw;
        if (typeof raw === 'string') return { s: raw };
        let text;
        try {
            text = JSON.stringify(raw);
        } catch (e) {
            text = undefined;
        }
        return { o: text === undefined ? String(raw) : text.slice(0, 200) };
    }

    const databases = { item: () => window.$dataItems, weapon: () => window.$dataWeapons, armor: () => window.$dataArmors };

    window.__vndegen.cheats = {
        apply(next) {
            const fill = next.keepsPartyHealthy && !settings.keepsPartyHealthy;
            settings = next;
            installAfterBoot();
            if (fill && inGame()) fillParty();
        },

        // JSON, so the app decodes one string rather than WebKit's conversion of every value.
        state() {
            installAfterBoot();
            if (!inGame()) return JSON.stringify({ inGame: false });
            const system = $dataSystem;
            const names = (list) => list || [];
            const items = [];
            for (const kind of Object.keys(databases)) {
                for (const entry of databases[kind]() || []) {
                    if (entry && entry.name) items.push([kind, entry.id, entry.name, $gameParty.numItems(entry)]);
                }
            }
            // The database's lists set how many there are. setValue ignores numbers past them.
            const variables = [];
            for (let id = 1; id < names(system.variables).length; id++) {
                variables.push([id, names(system.variables)[id] || '', value($gameVariables.value(id))]);
            }
            const switches = [];
            for (let id = 1; id < names(system.switches).length; id++) {
                switches.push([id, names(system.switches)[id] || '', !!$gameSwitches.value(id)]);
            }
            return JSON.stringify({
                inGame: true,
                gold: $gameParty.gold(),
                currencyUnit: system.currencyUnit || '',
                party: party().map((actor) => [actor.name(), actor.hp, actor.mhp, actor.mp, actor.mmp]),
                items, variables, switches,
            });
        },

        // Returns false when no game is going, so the change would be lost.
        perform(action) {
            if (!inGame()) return false;
            switch (action.type) {
            case 'gold':
                $gameParty.gainGold(action.amount);
                break;
            case 'restore':
                for (const actor of party()) actor.recoverAll();
                if ($gameParty.inBattle() && window.BattleManager && typeof BattleManager.refreshStatus === 'function') {
                    BattleManager.refreshStatus();
                }
                break;
            case 'item': {
                const entry = (databases[action.kind]() || [])[action.id];
                if (!entry) return false;
                $gameParty.gainItem(entry, action.count);
                break;
            }
            case 'variable':
                $gameVariables.setValue(action.id, action.value);
                break;
            case 'switch':
                $gameSwitches.setValue(action.id, action.on);
                break;
            default:
                return false;
            }
            return true;
        },
    };
})();
