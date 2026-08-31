#ifndef SAYRI_RELAY_H
#define SAYRI_RELAY_H

#include <stddef.h>

#define RELAY_PORT 5055
#define RELAY_CODE_LEN 3

/*
    Sayri relay server
    ------------------
    An always-on TCP server that lets a paired Sayri
    phone/tablet stream the AI out of this desktop
    machine. The desktop itself runs the local Ollama
    backend; the phone cannot run one, so it connects
    here and relays its chat through the local engine.

    Wire protocol (UTF-8, newline framed):

      client -> server:
        "PAIR <code>\n"          pair with the current code
        "CHAT <message>\n"       ask the assistant (UTF-8 text)

      server -> client:
        "HELLO <code>\n"         sent on connect (the code to display)
        "PAIR OK\n"              pairing succeeded
        "PAIR ERR <reason>\n"    pairing failed
        "TOK <text>\n"           a chunk of the reply (many of these)
        "DONE OK\n"              reply finished cleanly
        "DONE ERR <reason>\n"    reply failed

    A freshly connected socket must pair before it may
    send CHAT. TOK frames stream the assistant's reply
    in chunks so the phone can render it progressively.
*/

/*
    Start the relay. Always listens on RELAY_PORT.
    Returns 0 on success, -1 on failure.
*/
int relay_start(void);

/*
    Poll and serve any pending relay work. Call every
    frame from the main loop. Drives background accept +
    per-connection chat threads.
*/
void relay_poll(void);

/*
    Snapshot of the current pairing code so the UI
    can display it. Copies the numeric code into out.
*/
void relay_code(char *out, size_t out_size);

/*
    Generate a fresh pairing code.
*/
void relay_rotate_code(void);

/*
    Stop the listener and join all worker threads.
*/
void relay_stop(void);

#endif /* SAYRI_RELAY_H */
