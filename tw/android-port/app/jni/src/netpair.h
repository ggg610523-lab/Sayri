#ifndef SAYRI_NETPAIR_H
#define SAYRI_NETPAIR_H

#include <stddef.h>
#include <stdbool.h>

#define NETPAIR_PORT 5055
#define NETPAIR_CODE_LEN 3
#define NETPAIR_MAX_TEXT 4096
#define NETPAIR_FIELD_LEN 128

/*
    Sayri network-pairing client
    ----------------------------
    Lets a Sayri phone connect to a desktop running the
    Sayri relay (see relay.h), pair with a 4-digit code,
    and stream the AI through the desktop's local Ollama.
    The phone itself cannot run Ollama, so all chat is
    relayed.

    Wire protocol is the same as the desktop relay:
      <-- "HELLO <code>"
      --> "PAIR <code>"   and "<-- PAIR OK"
      --> "CHAT <msg>"    then "<-- TOK ...", "<-- DONE OK|ERR ..."

    Status:
      NETPAIR_IDLE     no connection attempt
      NETPAIR_CONNECTING
      NETPAIR_NEED_CODE  connected, waiting for pairing code
      NETPAIR_PAIRED     paired, can send chat
      NETPAIR_STREAMING  waiting for TOK/DONE

    The assistant's reply is delivered incrementally as
    TOK frames; netpair_poll() returns whether new text
    landed in the shared buffer this frame.
*/

typedef enum {
    NETPAIR_IDLE = 0,
    NETPAIR_CONNECTING,
    NETPAIR_NEED_CODE,
    NETPAIR_PAIRED,
    NETPAIR_STREAMING
} NetpairStatus;

/*
    Configure the pairing endpoint and code before
    netpair_connect() is called.
*/
void netpair_set_endpoint(const char *host, const char *port);
void netpair_set_code(const char *code);

/*
    Returns the current status.
*/
NetpairStatus netpair_status(void);

/*
    Last human-readable status/error text.
*/
const char *netpair_status_text(void);

/*
    Begin connecting + pairing in the background.
    Persistent until netpair_disconnect() or the app ends.
*/
void netpair_connect(void);

/*
    Ask the paired desktop to answer `msg`. The reply is
    streamed into a shared buffer. Returns true if the
    request was accepted (i.e. we were paired).
*/
bool netpair_send_msg(const char *msg);

/*
    Copies the accumulated assistant text into out. If a
    new TOK chunk arrived since the last poll, *new_data
    is set to true. Returns true when a reply just
    finished (DONE received) — call to obtain the final
    text and to reset the streaming buffer.
*/
bool netpair_poll(char *out, size_t out_size, bool *new_data);

/*
    Reset the streaming text buffer (called after the UI
    has consumed a finished reply).
*/
void netpair_clear_text(void);

/*
    Tear down the connection and join worker threads.
*/
void netpair_disconnect(void);

#endif /* SAYRI_NETPAIR_H */
