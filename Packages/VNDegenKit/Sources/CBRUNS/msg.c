#include "br.h"

void msg_send(BR *e, Node *call) {}

void msg_tick(BR *e) { e->wait = W_NONE; }

void msg_draw(BR *e, u32 *dst) {}
