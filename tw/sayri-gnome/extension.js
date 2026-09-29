/*
 * Sayri Assistant — GNOME Shell extension
 * ---------------------------------------
 * Adds a Sayri button to the top panel. Clicking it opens a
 * popup that mirrors the Sayri app's design language:
 *
 *   • the animated orb (orb.js, a port of orb.c)
 *   • a conversation area with glassy chat bubbles
 *   • a text field
 *   • the circular send button
 *
 * It talks to the *running* Sayri app over its IPC unix
 * socket ($XDG_RUNTIME_DIR/sayri.sock), so the Sayri app
 * must be open for the extension to answer.
 */

import St from 'gi://St';
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Cogl from 'gi://Cogl';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';

import {RES, renderOrb, makeAudioDrift, makeBuffer} from './orb.js';

const IPC_FILE = 'sayri.sock';
const MAX_MSGS = 40;

/*
 * Subclassing a GObject-backed type (PanelMenu.Button) in GJS requires
 * GObject.registerClass() so a concrete GType is generated. Without it
 * constructing the button fails with:
 *   "Tried to construct an object without a GType"
 */
const SayriIndicator = GObject.registerClass(
class SayriIndicator extends PanelMenu.Button {
    constructor(glyphPath) {
        super(0.0, 'Sayri Assistant', false);

        const icon = new St.Icon({
            style_class: 'system-status-icon',
            icon_size: 18,
        });
        if (glyphPath && GLib.file_test(glyphPath, GLib.FileTest.EXISTS))
            icon.set_gicon(Gio.icon_new_for_string(glyphPath));
        else
            icon.icon_name = 'face-smile-symbolic';
        this.add_child(icon);

        this._pending = false;
        this._lastTime = 0;
        this._frameSource = null;
        this._orb = makeBuffer();

        this._buildMenu();
    }

    /* ------------------------------------------------------------------ UI */
    _buildMenu() {
        /* PanelMenu.Button already built this.menu + this.menuManager. */
        const menu = this.menu;

        const box = new St.BoxLayout({vertical: true, style_class: 'sayri-panel'});

        /* ---- header: title ---- */
        const header = new St.BoxLayout({style_class: 'sayri-header'});
        header.add_child(new St.Label({
            text: 'Sayri',
            style_class: 'sayri-header-title',
        }));
        box.add_child(header);

        /* ---- orb actor (small, centered) ---- */
        const ORB_SIZE = 96;

        this._orbActor = new St.Widget({
            width: ORB_SIZE,
            height: ORB_SIZE,
            reactive: false,
        });
        this._imageContent = St.ImageContent.new_with_preferred_size(ORB_SIZE, ORB_SIZE);
        this._orbActor.set_content(this._imageContent);

        box.add_child(new St.Bin({
            style_class: 'sayri-orb-wrap',
            x_align: Clutter.ActorAlign.CENTER,
            child: this._orbActor,
        }));

        /* ---- transcript ---- */
        this._bubbles = new St.BoxLayout({vertical: true, style_class: 'sayri-bubbles'});

        this._scroll = new St.ScrollView({
            style_class: 'sayri-scroll',
            hscrollbar_policy: St.PolicyType.NEVER,
            vscrollbar_policy: St.PolicyType.AUTOMATIC,
        });
        this._scroll.set_child(this._bubbles);
        box.add_child(this._scroll);

        this._addBubble('Hi, I’m Sayri. How can I help?', false);

        /* ---- input row: text field + circular send ---- */
        this._entry = new St.Entry({
            style_class: 'sayri-entry',
            hint_text: 'Ask Sayri…',
            can_focus: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.STRETCH,
        });
        this._entry.set_can_focus(true);
        this._entry.connect('key-press-event', this._onKey.bind(this));

        this._send = new St.Button({style_class: 'sayri-send', label: '↑', reactive: true});
        this._send.connect('clicked', this._onSend.bind(this));

        const inputRow = new St.BoxLayout({style_class: 'sayri-input-row'});
        inputRow.add_child(this._entry);
        inputRow.add_child(this._send);
        box.add_child(inputRow);

        menu.box.add_child(box);
        menu.connect('open-state-changed', this._onOpen.bind(this));
    }

    /* ------------------------------------------------------ message UI */
    _addBubble(text, isUser) {
        if (this._bubbles.get_n_children() >= MAX_MSGS) {
            const first = this._bubbles.get_first_child();
            if (first)
                this._bubbles.remove_child(first);
        }

        const label = new St.Label({
            text,
            style_class: isUser ? 'sayri-bubble sayri-bubble-user'
                                : 'sayri-bubble sayri-bubble-ai',
            x_align: Clutter.ActorAlign.START,
        });
        label.get_clutter_text().line_wrap = true;

        let row;
        if (isUser) {
            row = new St.Bin({child: label});
            row.style_class = 'sayri-bubble-user-row';
        } else {
            row = new St.Bin({child: label});
            row.style_class = 'sayri-bubble-ai-row';
        }
        this._bubbles.add_child(row);

        const vadj = this._scroll.get_vadjustment();
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            const after = vadj.get_upper() - vadj.get_page_size();
            if (after > vadj.get_value())
                vadj.set_value(after);
            return GLib.SOURCE_REMOVE;
        });
    }

    _onKey(actor, event) {
        if (event.keyval === Clutter.KEY_Return ||
            event.keyval === Clutter.KEY_KP_Enter) {
            this._sendMessage();
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    _onSend() {
        this._sendMessage();
    }

    _sendMessage() {
        const text = this._entry.get_text().trim();
        if (!text || this._pending)
            return;

        this._entry.set_text('');
        this._addBubble(text, true);

        this._pending = true;
        this._setBusy(true);

        this._ipcSend(text)
            .then((answer) => {
                this._addBubble(answer || '(no response)', false);
            })
            .catch((err) => {
                log(`Sayri IPC error: ${err && err.message ? err.message : err}`);
                this._addBubble(
                    '(Cannot reach Sayri — is the app running and its IPC socket up?)',
                    false);
            })
            .finally(() => {
                this._pending = false;
                this._setBusy(false);
            });
    }

    _setBusy(busy) {
        this._send.reactive = !busy;
        this._send.set_opacity(busy ? 140 : 255);

        if (busy) {
            if (!this._typing) {
                this._typing = new St.Label({
                    text: 'Sayri is thinking…',
                    style_class: 'sayri-typing-bubble',
                    x_align: Clutter.ActorAlign.START,
                });
                this._bubbles.add_child(this._typing);
            }
        } else {
            if (this._typing) {
                this._bubbles.remove_child(this._typing);
                this._typing = null;
            }
        }

        const vadj = this._scroll.get_vadjustment();
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            const after = vadj.get_upper() - vadj.get_page_size();
            if (after > vadj.get_value())
                vadj.set_value(after);
            return GLib.SOURCE_REMOVE;
        });
    }

    /* ------------------------------------------------------------ IPC */
    _ipcSend(message) {
        return new Promise((resolve, reject) => {
            const dir = GLib.getenv('XDG_RUNTIME_DIR') || '/tmp';
            const path = `${dir}/${IPC_FILE}`;
            let cancelled = false;
            const cancel = () => { cancelled = true; };

            const client = new Gio.SocketClient();
            const addr = new Gio.UnixSocketAddress({path});

            client.connect_async(addr, null, (source, result) => {
                if (cancelled || this._disposed) {
                    reject(new Error('cancelled'));
                    return;
                }

                let connection;
                try {
                    connection = source.connect_finish(result);
                } catch (e) {
                    reject(e);
                    return;
                }

                const encoder = new TextEncoder();
                const frameBytes = encoder.encode(message + '\n');

                try {
                    /* GJS wraps a Uint8Array as GBytes for WRITE ALL. */
                    connection.get_output_stream().write_all_async(
                        frameBytes,
                        GLib.PRIORITY_DEFAULT, null, (st, r) => {
                            if (this._disposed) {
                                reject(new Error('cancelled'));
                                return;
                            }
                            try {
                                st.write_all_finish(r);
                            } catch (e) {
                                reject(e);
                                return;
                            }
                            this._readReply(connection, [], resolve, reject);
                        });
                } catch (e) {
                    reject(e);
                }
            });
        });
    }

    _readReply(connection, parts, resolve, reject) {
        connection.get_input_stream().read_bytes_async(
            4096, GLib.PRIORITY_DEFAULT, null, (stream, result) => {
                try {
                    const bytes = stream.read_bytes_finish(result);
                    if (bytes.get_size() === 0) {
                        resolve(parts.join(''));
                        return;
                    }
                    const data = bytes.get_data();
                    const s = new TextDecoder('utf-8', {fatal: false}).decode(data);
                    parts.push(s);
                    this._readReply(connection, parts, resolve, reject);
                } catch (e) {
                    if (parts.length)
                        resolve(parts.join(''));
                    else
                        reject(e);
                }
            });
    }

    /* ---------------------------------------------------- orb rendering */
    _renderOrbFrame() {
        const time = this._lastTime;
        const audio = makeAudioDrift(time);
        renderOrb(this._orb, time, audio);

        const bytes = new GLib.Bytes(this._orb);
        const backend = global.stage.context.get_backend();
        const coglCtx = backend ? backend.get_cogl_context() : null;

        if (coglCtx) {
            this._imageContent.set_bytes(
                coglCtx, bytes,
                Cogl.PixelFormat.RGBA_8888_PRE, RES, RES, RES * 4);
        } else {
            this._imageContent.set_bytes(
                bytes,
                Cogl.PixelFormat.RGBA_8888_PRE, RES, RES, RES * 4);
        }
        this._orbActor.queue_redraw();
    }

    /* ------------------------------------------------------- lifecycle */
    _onOpen(menu, open) {
        if (open) {
            if (!this._frameSource) {
                this._lastTime = 0;
                const self = this;
                this._frameSource = GLib.timeout_add(
                    GLib.PRIORITY_DEFAULT, 66, () => {
                        if (self._disposed)
                            return GLib.SOURCE_REMOVE;
                        self._lastTime += 0.066;
                        self._renderOrbFrame();
                        return GLib.SOURCE_CONTINUE;
                    });
            }
        } else {
            if (this._frameSource) {
                GLib.source_remove(this._frameSource);
                this._frameSource = null;
            }
        }
    }

    destroy() {
        this._disposed = true;
        if (this._frameSource) {
            GLib.source_remove(this._frameSource);
            this._frameSource = null;
        }
        super.destroy(); // PanelMenu.Button tears down its own menu/manager
    }
});

export default class SayriAssistantExtension extends Extension {
    enable() {
        this._indicator = new SayriIndicator(`${this.path}/glyph.png`);
        Main.panel.addToStatusArea('sayri-assistant', this._indicator, 0, 'right');
    }

    disable() {
        if (this._indicator) {
            this._indicator.destroy();
            this._indicator = null;
        }
    }
}