# Sayri Assistant — GNOME Shell extension

Chat with **Sayri** (the local AI assistant for pulsarOS) straight from the
GNOME Shell top panel, using the **same design language** as the Sayri app:

-   the **animated orb** — a JavaScript port of the exact shader from the
    app's `orb.c` / `orb.h`
-   a conversation area with **glassy chat bubbles**
-   a **text field**
-   the circular **↑ send button**

It builds on top of the running Sayri app's **IPC socket**, so the two share
the same local Ollama backend and the same conversation flow.

## How it works

This extension is the thin GNOME client. The heavy lifting happens in the
Sayri app, which now exposes a small Unix-socket server:

```
$XDG_RUNTIME_DIR/sayri.sock        (or /tmp/sayri.sock)
```

Protocol (newline framed):

```
client -> "hello\n"
server -> "Hello! How can I help today?" then closes
```

So **the Sayri app must be running** for the extension to reply.

## Files

```
sayri-gnome/
├── metadata.json       # extension UUID + GNOME 45+ compatibility
├── extension.js        # panel button, popup, orb, text field, send, IPC client
├── orb.js              # JS port of orb.c (RGBA pixel shader)
├── stylesheet.css      # Sayri/Pulsar glassmorphism theme
├── install.sh          # copy + reload + enable
└── README.md
```

## Install

```bash
./install.sh
```

`./install.sh --copy` copies without restarting the shell (useful on
Wayland, which can’t soft-restart; log out/in there).

After installing, enable via `gnome-extensions apps` or the Extensions app →
**Sayri Assistant**. A 🙂 icon appears in the top-right; click it.

## Build/verify the Sayri side

The IPC server lives in the main Sayri repo (`ipc.c`), wired into `main.c`
and the Makefile. Rebuild the app so the socket is exposed:

```bash
make   # in the Sayri repo root
```

Run `./pulsar-assistant`; you’ll see `Sayri IPC socket: /run/user/<uid>/sayri.sock`.

## Design-language port notes

-   `orb.js` is a line-for-line port of `render_orb_line()` from `orb.c`:
    FBM-style noise ribbons on a sphere, animated color ramp, diffuse
    lighting, fresnel rim, soft coverage ramp and center glow. Rendered at
    `RES = 128` and blitted to the cairo surface with premultiplied alpha.
-   The popup uses the app’s palette: deep indigo glass panel, translucent
    user/assistant bubbles, blue ↑ send button, and the mint/blue/pink
    accents already baked into the orb shader.
-   The background follows your spec: a clean gradient **without** the app’s
    extra floating orb blobs.

## Troubleshooting

-   “Cannot reach Sayri…” → launch `./pulsar-assistant` (its IPC socket only
    exists while the app runs). Confirm with `ls $XDG_RUNTIME_DIR/sayri.sock`.
-   Extension not loading → check `journalctl /usr/bin/gnome-shell -f` and the
    extension logs (`LookingGlass` / `lg`).
-   Shell version → `metadata.json` targets GNOME 45+. Adjust
    `shell-version`.