/* /usr/src/servers/po/po.c
 *
 * Post Office server for MINIX 3.2.1.
 * Key points for signal-handling test:
 * - retrieve_wait blocks by returning SUSPEND (no reply)
 * - PM notifies us with PM_UNPAUSE (sent with asynsend3)
 * - we must wake the blocked caller with a NONBLOCKING reply (sendnb)
 */

#include <minix/drivers.h>
#include <minix/sef.h>
#include <minix/syslib.h>
#include <minix/endpoint.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <minix/callnr.h>

#include <lib.h>            /* _syscall */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <po.h>

#define PM_REGISTER_PO_NR 44   /* zostawiamy na sztywno 44, jak chcesz */

/* ---------------- mailbox state ---------------- */

typedef struct {
    int full;
    int is_bomb;
    endpoint_t owner_ep;
    pid_t owner_pid;
    pid_t sender_pid;        /* NIE zmieniamy przy forward/send_back */
    package pkg;
    clock_t bomb_deadline;   /* uptime ticks */
} mailbox_t;

typedef struct {
    int waiting;
    endpoint_t waiter_ep;
    vir_bytes pkg_ptr;       /* user's package* */
    vir_bytes pid_ptr;       /* user's pid_t* */
} waiter_t;

static mailbox_t mbox[NR_PROCS];
static waiter_t  waiters[NR_PROCS];
static int g_hz = 60;

/* ---------------- helpers ---------------- */

static clock_t ticks_now(void)
{
    clock_t t = 0;
    getuptime(&t);
    return t;
}

static clock_t us_to_ticks(int us)
{
    uint64_t num;
    clock_t t;

    if (us <= 0) return 0;

    num = (uint64_t)us * (uint64_t)g_hz;
    t = (clock_t)((num + 999999ULL) / 1000000ULL);
    if (t <= 0) t = 1;
    return t;
}

static int ep_slot(endpoint_t ep)
{
    int s = _ENDPOINT_P(ep);
    if (s < 0 || s >= NR_PROCS) return -1;
    return s;
}

static void clr_box(int s)
{
    mbox[s].full = 0;
    mbox[s].is_bomb = 0;
    mbox[s].sender_pid = 0;
    memset(&mbox[s].pkg, 0, sizeof(mbox[s].pkg));
    mbox[s].bomb_deadline = 0;
}

static void clr_slot(int s, endpoint_t ep, pid_t pid)
{
    clr_box(s);
    mbox[s].owner_ep = ep;
    mbox[s].owner_pid = pid;

    waiters[s].waiting = 0;
    waiters[s].waiter_ep = NONE;
    waiters[s].pkg_ptr = 0;
    waiters[s].pid_ptr = 0;
}

static int sync_owner(endpoint_t ep)
{
    int s = ep_slot(ep);
    pid_t pid;

    if (s < 0) return -1;

    pid = getnpid(ep);
    if (mbox[s].owner_pid != pid) {
        clr_slot(s, ep, pid);
        return s;
    }

    mbox[s].owner_ep = ep;
    mbox[s].owner_pid = pid;
    return s;
}

static int pid_to_ep(pid_t pid, endpoint_t *ep_out)
{
    endpoint_t ep;

    if (pid <= 0) return ESRCH;

    ep = getnprocnr(pid);
    if (ep < 0) return ESRCH;

    *ep_out = ep;
    return OK;
}

/* NONBLOCKING wakeup (kluczowe!) */
static void wake_waiter(endpoint_t ep, int err)
{
    message rep;
    memset(&rep, 0, sizeof(rep));
    rep.m_type = -err; /* MINIX: -errno */
    (void)sendnb(ep, &rep);
}

static void set_alarm(void)
{
    clock_t soon = 0;
    int i;

    for (i = 0; i < NR_PROCS; i++) {
        if (!mbox[i].full || !mbox[i].is_bomb) continue;
        if (mbox[i].bomb_deadline <= 0) continue;

        if (soon == 0 || mbox[i].bomb_deadline < soon)
            soon = mbox[i].bomb_deadline;
    }

    if (soon == 0) {
        sys_setalarm(0, 1);
        return;
    }

    if (soon <= ticks_now())
        soon = ticks_now() + 1;

    sys_setalarm(soon, 1);
}

static void handle_bomb_timeouts(void)
{
    clock_t now = ticks_now();
    int i;

    for (i = 0; i < NR_PROCS; i++) {
        if (!mbox[i].full || !mbox[i].is_bomb) continue;
        if (mbox[i].bomb_deadline <= 0) continue;
        if (mbox[i].bomb_deadline > now) continue;

        endpoint_t victim_ep = mbox[i].owner_ep;
        clr_box(i);

        if (victim_ep != NONE)
            sys_kill(victim_ep, SIGTERM);
    }

    set_alarm();
}

/* ---------------- operations ---------------- */

static int do_po_post(message *m)
{
    if (m->m1_p1 == NULL) return EINVAL;

    pid_t target_pid = (pid_t)m->m1_i1;
    endpoint_t target_ep;
    int r = pid_to_ep(target_pid, &target_ep);
    if (r != OK) return r;

    int ts = sync_owner(target_ep);
    if (ts < 0) return EINVAL;

    if (mbox[ts].full) return EBUSY;

    package p;
    if (sys_datacopy(m->m_source, (vir_bytes)m->m1_p1,
                     SELF, (vir_bytes)&p,
                     (phys_bytes)sizeof(p)) != OK)
        return EINVAL;

    pid_t sender_pid = getnpid(m->m_source);

    if (waiters[ts].waiting) {
        endpoint_t wep = waiters[ts].waiter_ep;
        vir_bytes pkg_ptr = waiters[ts].pkg_ptr;
        vir_bytes pid_ptr = waiters[ts].pid_ptr;

        waiters[ts].waiting = 0;
        waiters[ts].waiter_ep = NONE;
        waiters[ts].pkg_ptr = 0;
        waiters[ts].pid_ptr = 0;

        if (sys_datacopy(SELF, (vir_bytes)&p, wep, pkg_ptr,
                         (phys_bytes)sizeof(package)) != OK ||
            sys_datacopy(SELF, (vir_bytes)&sender_pid, wep, pid_ptr,
                         (phys_bytes)sizeof(pid_t)) != OK) {
            wake_waiter(wep, EINVAL);
        } else {
            wake_waiter(wep, OK);
        }
        return OK;
    }

    mbox[ts].full = 1;
    mbox[ts].is_bomb = 0;
    mbox[ts].sender_pid = sender_pid;
    mbox[ts].pkg = p;
    mbox[ts].bomb_deadline = 0;

    return OK;
}

static int do_po_retrieve_common(message *m, int blocking)
{
    endpoint_t ep = m->m_source;
    int s = sync_owner(ep);
    if (s < 0) return EINVAL;

    if (m->m1_p1 == NULL || m->m1_p2 == NULL) return EINVAL;

    if (!mbox[s].full) {
        if (!blocking) return ENOMSG;
        if (waiters[s].waiting) return EBUSY;

        /* validate both pointers */
        {
            char tmp;
            pid_t tmp_pid;
            if (sys_datacopy(ep, (vir_bytes)m->m1_p1, SELF, (vir_bytes)&tmp, 1) != OK)
                return EINVAL;
            if (sys_datacopy(ep, (vir_bytes)m->m1_p2, SELF, (vir_bytes)&tmp_pid, (phys_bytes)sizeof(pid_t)) != OK)
                return EINVAL;
        }

        waiters[s].waiting = 1;
        waiters[s].waiter_ep = ep;
        waiters[s].pkg_ptr = (vir_bytes)m->m1_p1;
        waiters[s].pid_ptr = (vir_bytes)m->m1_p2;

        return SUSPEND; /* brak reply */
    }

    if (mbox[s].is_bomb) {
        endpoint_t victim_ep = mbox[s].owner_ep;

        clr_box(s);
        set_alarm();

        if (victim_ep != NONE)
            sys_kill(victim_ep, SIGTERM);

        return EINTR;
    }

    if (sys_datacopy(SELF, (vir_bytes)&mbox[s].pkg, ep, (vir_bytes)m->m1_p1,
                     (phys_bytes)sizeof(package)) != OK ||
        sys_datacopy(SELF, (vir_bytes)&mbox[s].sender_pid, ep, (vir_bytes)m->m1_p2,
                     (phys_bytes)sizeof(pid_t)) != OK)
        return EINVAL;

    clr_box(s);
    return OK;
}

static int do_po_retrieve(message *m)      { return do_po_retrieve_common(m, 0); }
static int do_po_retrieve_wait(message *m) { return do_po_retrieve_common(m, 1); }

static int do_po_check(message *m)
{
    if (m->m1_p1 == NULL) return EINVAL;

    int s = sync_owner(m->m_source);
    if (s < 0) return EINVAL;

    if (!mbox[s].full) return ENOMSG;

    if (sys_datacopy(SELF, (vir_bytes)&mbox[s].sender_pid,
                     m->m_source, (vir_bytes)m->m1_p1,
                     (phys_bytes)sizeof(pid_t)) != OK)
        return EINVAL;

    return OK;
}

static void deliver_or_store(int from_s, endpoint_t to_ep, int to_s)
{
    mailbox_t msg = mbox[from_s];

    if (waiters[to_s].waiting) {
        endpoint_t wep = waiters[to_s].waiter_ep;
        vir_bytes pkg_ptr = waiters[to_s].pkg_ptr;
        vir_bytes pid_ptr = waiters[to_s].pid_ptr;

        waiters[to_s].waiting = 0;
        waiters[to_s].waiter_ep = NONE;
        waiters[to_s].pkg_ptr = 0;
        waiters[to_s].pid_ptr = 0;

        if (msg.is_bomb) {
            sys_kill(wep, SIGTERM);
            wake_waiter(wep, EINTR);
            return;
        }

        if (sys_datacopy(SELF, (vir_bytes)&msg.pkg, wep, pkg_ptr,
                         (phys_bytes)sizeof(package)) != OK ||
            sys_datacopy(SELF, (vir_bytes)&msg.sender_pid, wep, pid_ptr,
                         (phys_bytes)sizeof(pid_t)) != OK) {
            wake_waiter(wep, EINVAL);
        } else {
            wake_waiter(wep, OK);
        }
        return;
    }

    pid_t to_pid = getnpid(to_ep);
    mbox[to_s] = msg;
    mbox[to_s].owner_ep = to_ep;
    mbox[to_s].owner_pid = to_pid;

    if (mbox[to_s].is_bomb)
        set_alarm();
}

static int do_po_send_back(message *m)
{
    int s = sync_owner(m->m_source);
    if (s < 0) return EINVAL;
    if (!mbox[s].full) return ENOMSG;

    pid_t target_pid = mbox[s].sender_pid;

    endpoint_t target_ep;
    int r = pid_to_ep(target_pid, &target_ep);
    if (r != OK) return r;

    int ts = sync_owner(target_ep);
    if (ts < 0) return EINVAL;
    if (mbox[ts].full) return EBUSY;

    deliver_or_store(s, target_ep, ts);
    clr_box(s);
    set_alarm();
    return OK;
}

static int do_po_forward(message *m)
{
    int s = sync_owner(m->m_source);
    if (s < 0) return EINVAL;
    if (!mbox[s].full) return ENOMSG;

    pid_t target_pid = (pid_t)m->m1_i1;

    endpoint_t target_ep;
    int r = pid_to_ep(target_pid, &target_ep);
    if (r != OK) return r;

    int ts = sync_owner(target_ep);
    if (ts < 0) return EINVAL;
    if (mbox[ts].full) return EBUSY;

    deliver_or_store(s, target_ep, ts);
    clr_box(s);
    set_alarm();
    return OK;
}

static int do_po_send_bomb(message *m)
{
    pid_t target_pid = (pid_t)m->m1_i1;
    int timer_us = m->m1_i2;

    endpoint_t sender_ep = m->m_source;
    pid_t sender_pid = getnpid(sender_ep);

    int sender_s = sync_owner(sender_ep);
    if (sender_s < 0) return EINVAL;

    if (timer_us <= 0) {
        sys_kill(sender_ep, SIGTERM);
        return OK;
    }

    clock_t deadline = ticks_now() + us_to_ticks(timer_us);

    endpoint_t target_ep;
    int r = pid_to_ep(target_pid, &target_ep);
    if (r != OK) {
        if (!mbox[sender_s].full) {
            mbox[sender_s].full = 1;
            mbox[sender_s].is_bomb = 1;
            mbox[sender_s].sender_pid = sender_pid;
            mbox[sender_s].bomb_deadline = deadline;
            set_alarm();
            return OK;
        }
        sys_kill(sender_ep, SIGTERM);
        return OK;
    }

    int ts = sync_owner(target_ep);
    if (ts < 0) return EINVAL;

    if (!mbox[ts].full && waiters[ts].waiting) {
        endpoint_t wep = waiters[ts].waiter_ep;
        waiters[ts].waiting = 0;
        waiters[ts].waiter_ep = NONE;
        waiters[ts].pkg_ptr = 0;
        waiters[ts].pid_ptr = 0;

        sys_kill(target_ep, SIGTERM);
        wake_waiter(wep, EINTR);
        return OK;
    }

    if (!mbox[ts].full) {
        mbox[ts].full = 1;
        mbox[ts].is_bomb = 1;
        mbox[ts].sender_pid = sender_pid;
        mbox[ts].bomb_deadline = deadline;
        set_alarm();
        return OK;
    }

    if (!mbox[sender_s].full) {
        mbox[sender_s].full = 1;
        mbox[sender_s].is_bomb = 1;
        mbox[sender_s].sender_pid = sender_pid;
        mbox[sender_s].bomb_deadline = deadline;
        set_alarm();
        return OK;
    }

    sys_kill(sender_ep, SIGTERM);
    return OK;
}

static int do_po_unpause(message *m)
{
    endpoint_t victim_ep = m->PM_PROC;
    int s = ep_slot(victim_ep);
    if (s < 0) return OK;

    (void)sync_owner(victim_ep);

    if (waiters[s].waiting && waiters[s].waiter_ep == victim_ep) {
        endpoint_t wep = waiters[s].waiter_ep;

        waiters[s].waiting = 0;
        waiters[s].waiter_ep = NONE;
        waiters[s].pkg_ptr = 0;
        waiters[s].pid_ptr = 0;

        wake_waiter(wep, EINTR);
    }

    return OK;
}

/* ---------------- SEF ---------------- */

static int sef_cb_init(int type, sef_init_info_t *info)
{
    int i;
    g_hz = sys_hz();

    for (i = 0; i < NR_PROCS; i++)
        clr_slot(i, NONE, 0);

    sys_setalarm(0, 1);

    /* rejestracja endpointu w PM (call nr 44) */
    {
    message m;
    memset(&m, 0, sizeof(m));

    //int r = _syscall(PM_PROC_NR, PM_REGISTER_PO_NR, &m);
    int rr = _syscall(PM_PROC_NR, 44, &m);
if (rr != OK) {
    printf("PO: PM_REGISTER_PO failed rr=%d errno=%d\n", rr, errno);
}
}

    return OK;
}

static void sef_local_startup(void)
{
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);
    sef_startup();
}

int main(int argc, char **argv)
{
    message m;

    env_setargs(argc, argv);
    sef_local_startup();

    while (1) {
        int r = sef_receive(ANY, &m);
        if (r != OK) continue;

        if (m.m_type == NOTIFY_MESSAGE && m.m_source == CLOCK) {
            handle_bomb_timeouts();
            continue;
        }

        int result;
        switch (m.m_type) {
            case PO_POST:          result = do_po_post(&m); break;
            case PO_RETRIEVE:      result = do_po_retrieve(&m); break;
            case PO_RETRIEVE_WAIT: result = do_po_retrieve_wait(&m); break;
            case PO_CHECK:         result = do_po_check(&m); break;
            case PO_SEND_BACK:     result = do_po_send_back(&m); break;
            case PO_FORWARD:       result = do_po_forward(&m); break;
            case PO_SEND_BOMB:     result = do_po_send_bomb(&m); break;
            case PM_UNPAUSE:       (void)do_po_unpause(&m); continue; /* no reply */
            default:               result = ENOSYS; break;
        }

        if (result == SUSPEND) continue;

        m.m_type = -result; /* 0 => 0, errno => -errno */
        (void)sendnb(m.m_source, &m);
    }
    return 0;
}