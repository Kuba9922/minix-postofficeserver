# Project – Post Office Server

This project extends the **MINIX** system with a server that enables communication between user processes via messages.  
Each user process has its own mailbox that can hold **exactly one message**.

***

## User Process Interface

The following functions are available in the system library `libc` (e.g., in `lib\libc\sys-minix\`).  
Function declarations and the definition of type `package` are provided in `include\po.h`.

*   **`int post(package *pp, pid_t pid)`**  
    Places message `pp` in the mailbox of process `pid`.  
    **Non-blocking** call – if the mailbox is not empty, the function returns `-1` and sets `errno` accordingly.

*   **`int retrieve(package *pp, pid_t *pidp)`**  
    Reads a message from the calling process’s mailbox.  
    The message is stored at `pp`, and the mailbox becomes empty.  
    The sender’s PID is stored at `pidp`.  
    If the mailbox is empty, the function returns `-1` and sets `errno`.

*   **`int retrieve_wait(package *pp, pid_t *pidp)`**  
    Blocking version of `retrieve`.  
    If the mailbox is empty, the process is blocked until a message arrives or a signal is received.  
    If interrupted by a signal, the function returns `-1` and sets `errno = EINTR`.

*   **`int check(pid_t *pidp)`**  
    Checks if there is a message in the mailbox.  
    If yes, returns `0` and stores the sender’s PID at `pidp`.  
    If empty, returns `-1` and sets `errno`.

*   **`int send_back(void)`**  
    Attempts to send the message from the current process’s mailbox back to the sender.  
    If unsuccessful, the message remains in the mailbox.

*   **`int forward(pid_t pid)`**  
    Forwards the message from the current process’s mailbox to the mailbox of process `pid`.  
    If unsuccessful because the recipient mailbox is occupied, the message remains in the current process's mailbox.

*   **`int send_bomb(pid_t pid, int timer)`**  
    Sends a special message (“bomb”) to the mailbox of process `pid`.  
    A process that tries to read this message using `retrieve` or `retrieve_wait` immediately receives a `SIGTERM` signal.  
    Additionally, if the message is not read within `timer` microseconds, `SIGTERM` is sent to the process whose mailbox contains the bomb.  
    After sending the signal, the mailbox becomes empty.  
    If `timer <= 0`, `SIGTERM` is sent immediately to the sender.  
    If the message cannot be delivered, it is put in the sender’s mailbox.  
    In such a case, if the sender’s mailbox is busy, the sender immediately receives `SIGTERM`.  
    In all cases, the message disappears after the signal is sent.

UPDATE: forwarded and sent back packages do not change its sender.

***

## Error Codes (`errno`)

Functions return `0` on success and `-1` on failure.  
Possible `errno` values:

*   `ENOMSG` – no message in the mailbox (e.g., `retrieve`, `send_back`)
*   `EBUSY` – mailbox is busy (e.g., `post`, `forward`, `send_bomb`)
*   `ESRCH` – process with given PID does not exist (e.g., `post`, `forward`)
*   `EINVAL` – invalid arguments (e.g., `NULL` pointers)
*   `EINTR` – interrupted by a signal (only for `retrieve_wait`)

***

## Signal Handling

Processes blocked in `retrieve_wait` immediately handle incoming signals according to their registered handlers.  
If waiting is interrupted, the function returns `-1` and sets `errno = EINTR`.

***

## Tests

The solution was tested on **MINIX 3.2.1**.  
## Tests

The project was tested on **MINIX 3.2.1**.  
The `tests/` directory contains files for user-level tests.

***

## Repository Structure

*   `src/` – mirrors part of the MINIX source tree, containing modified files.  
    To test the solution:
    1.  Copy the contents of `src/` to `/usr/src` in MINIX.
    2.  Install headers, recompile and install the appropriate libraries and the new server.
    3.  Rebuild the system image to include changes in core servers (e.g., PM).
    4.  After reboot, start the server and verify functionality.

*   `tests/` – contains user-level tests.