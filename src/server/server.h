#ifndef SERVER_H
#define SERVER_H

void ecewo(unsigned short PORT);
void shutdown_hook(void (*hook)(void));
void register_pquv(int (*has_active_ops)(void), int (*get_active_count)(void));
void init_router(void);
void reset_router(void);

#endif
