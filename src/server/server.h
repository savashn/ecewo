#ifndef SERVER_H
#define SERVER_H

void ecewo(unsigned short PORT);
void shutdown_hook(void (*hook)(void));
void init_router(void);
void reset_router(void);

#endif
