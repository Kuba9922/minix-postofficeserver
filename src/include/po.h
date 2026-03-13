#ifndef POSTOFFICE_H
#define POSTOFFICE_H
#define PO_POST          1
#define PO_RETRIEVE      2
#define PO_RETRIEVE_WAIT 3
#define PO_CHECK         4
#define PO_SEND_BACK     5
#define PO_FORWARD       6
#define PO_SEND_BOMB     7
#define PO_CANCEL        8

#include <sys/types.h>

typedef struct {
    char bytes[128];
} package;

int post(package *pp, pid_t dst);
int retrieve(package *pp, pid_t *pidp);
int retrieve_wait(package *pp, pid_t *pidp);
int check(pid_t *pidp);
int send_back(void);
int forward(pid_t dst);
int send_bomb(pid_t dst, int timer);

#endif /* POSTOFFICE_H */