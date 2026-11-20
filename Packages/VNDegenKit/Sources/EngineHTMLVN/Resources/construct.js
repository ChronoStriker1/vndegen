(() => {
    'use strict';

    delete window.OffscreenCanvas;

    let c3;
    Object.defineProperty(window, 'C3', {
        configurable: true,
        get: () => c3,
        set: (value) => {
            c3 = value;
            queueMicrotask(() => {
                if (c3 && c3.AssetManager) c3.AssetManager.prototype.GetAudioToPreload = () => [];
            });
        },
    });

    let runtimeInterface;
    Object.defineProperty(window, 'RuntimeInterface', {
        configurable: true,
        get: () => runtimeInterface,
        set: (value) => {
            runtimeInterface = typeof value !== 'function' ? value : class extends value {
                constructor(options) {
                    super(options && options.exportType === 'nwjs' ? { ...options, exportType: 'html5' } : options);
                }
            };
        },
    });

    const listeners = new Set();
    const deliver = (data) => setTimeout(() => { for (const listener of listeners) listener({ data: data }); }, 0);
    const webview = {
        postMessage: (json) => {
            const message = JSON.parse(json);
            if (message.type === 'wrapper-init') deliver({ type: 'wrapper-init-response', registeredComponentIds: [] });
            else if (message.type === 'set-fullscreen') deliver(message.fullscreen ? 'entered-fullscreen' : 'exited-fullscreen');
        },
        addEventListener: (type, listener) => { if (type === 'message') listeners.add(listener); },
        removeEventListener: (type, listener) => { if (type === 'message') listeners.delete(listener); },
    };
    window.chrome = window.chrome || {};
    window.chrome.webview = webview;

    class KVStorageContainer {
        constructor(name) {
            if (typeof name !== 'string') throw new TypeError('expected string');
            this.name = name;
        }

        storageKey(key) {
            if (typeof key !== 'string') throw new TypeError('expected string');
            return this.name + '/' + key;
        }

        storedKeys() {
            const prefix = this.name + '/';
            const keys = [];
            for (let i = 0; i < localStorage.length; i++) {
                const key = localStorage.key(i);
                if (key !== null && key.startsWith(prefix)) keys.push(key.slice(prefix.length));
            }
            return keys;
        }

        async ready() {}

        async set(key, value) {
            localStorage.setItem(this.storageKey(key), JSON.stringify(value === undefined ? null : value));
        }

        async get(key) {
            const value = localStorage.getItem(this.storageKey(key));
            return value === null ? undefined : JSON.parse(value);
        }

        async delete(key) {
            localStorage.removeItem(this.storageKey(key));
        }

        async clear() {
            for (const key of this.storedKeys()) localStorage.removeItem(this.storageKey(key));
        }

        async keys() {
            return this.storedKeys();
        }

        async values() {
            return Promise.all(this.storedKeys().map((key) => this.get(key)));
        }

        async entries() {
            return Promise.all(this.storedKeys().map(async (key) => [key, await this.get(key)]));
        }
    }
    Object.defineProperty(window, 'KVStorageContainer', { get: () => KVStorageContainer, set: () => {}, configurable: true });

    let touching = false;
    for (const type of ['pointerdown', 'pointermove', 'pointerup', 'pointercancel']) {
        window.addEventListener(type, (event) => {
            if (!event.isTrusted || event.pointerType === 'mouse' || !event.isPrimary) return;
            touching = type === 'pointerdown' || (touching && type === 'pointermove');
            Object.defineProperty(event, 'pointerType', { value: 'mouse' });
        }, true);
    }

    const pointerTypes = { mousedown: 'pointerdown', mousemove: 'pointermove', mouseup: 'pointerup' };
    for (const type of Object.keys(pointerTypes)) {
        window.addEventListener(type, (event) => {
            if (event.isTrusted || touching) return;
            event.target.dispatchEvent(new PointerEvent(pointerTypes[type], {
                pointerId: 1, pointerType: 'mouse', isPrimary: true,
                clientX: event.clientX, clientY: event.clientY, screenX: event.screenX, screenY: event.screenY,
                button: type === 'mousemove' ? -1 : event.button, buttons: event.buttons,
                bubbles: true, cancelable: true, composed: true, view: window,
            }));
        }, true);
    }
})();
