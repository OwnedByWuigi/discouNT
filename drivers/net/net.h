#ifndef NET_H
#define NET_H

#include <stdint.h>

void NetInit(void);
void NetPoll(void);
int NetIsReady(void);
int NetPing(const char *ip_text, char *out_text, int out_text_len);

#endif
