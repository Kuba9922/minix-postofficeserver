/* /usr/src/lib/libc/sys-minix/po.c */

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <minix/rs.h>
#include <minix/endpoint.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <po.h>

static int get_po_endpt(endpoint_t *pt)
{
    return minix_rs_lookup("po", pt);
}

/* działa dla obu konwencji:
 * - server odsyła m_type = -errno  => _syscall zwraca -1 i ustawia errno
 * - server odsyła m_type = +errno  => _syscall może zwrócić errno
 */
static int po_call(int callnr, message *m)
{
    endpoint_t po_ep;
    int r;

    r = get_po_endpt(&po_ep);
    if (r != 0) { errno = ENOSYS; return -1; }

    r = _syscall(po_ep, callnr, m);

    if (r == 0) return 0;
    if (r == -1) return -1; /* errno już ustawione */

    if (r < 0) r = -r;
    errno = r;
    return -1;
}

int post(package *pp, pid_t pid)
{
    message m;
    if (!pp) { errno = EINVAL; return -1; }

    memset(&m, 0, sizeof(m));
    m.m1_i1 = (int)pid;
    m.m1_p1 = (char*)pp;
    return po_call(PO_POST, &m);
}

int retrieve(package *pp, pid_t *pidp)
{
    message m;
    if (!pp || !pidp) { errno = EINVAL; return -1; }

    memset(&m, 0, sizeof(m));
    m.m1_p1 = (char*)pp;
    m.m1_p2 = (char*)pidp;
    return po_call(PO_RETRIEVE, &m);
}

int retrieve_wait(package *pp, pid_t *pidp)
{
    message m;
    if (!pp || !pidp) { errno = EINVAL; return -1; }

    memset(&m, 0, sizeof(m));
    m.m1_p1 = (char*)pp;
    m.m1_p2 = (char*)pidp;
    return po_call(PO_RETRIEVE_WAIT, &m);
}

int check(pid_t *pidp)
{
    message m;
    if (!pidp) { errno = EINVAL; return -1; }

    memset(&m, 0, sizeof(m));
    m.m1_p1 = (char*)pidp;
    return po_call(PO_CHECK, &m);
}

int send_back(void)
{
    message m;
    memset(&m, 0, sizeof(m));
    return po_call(PO_SEND_BACK, &m);
}

int forward(pid_t pid)
{
    message m;
    memset(&m, 0, sizeof(m));
    m.m1_i1 = (int)pid;
    return po_call(PO_FORWARD, &m);
}

int send_bomb(pid_t pid, int timer)
{
    message m;
    memset(&m, 0, sizeof(m));
    m.m1_i1 = (int)pid;
    m.m1_i2 = timer;
    return po_call(PO_SEND_BOMB, &m);
}