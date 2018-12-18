/*!
 * \file
 *
 * \brief \ref process "Process", \ref sysV "System-V", and \ref posix_sem "POSIX libraries" wrapper module.
 *
 * \details This module removes defensive programming approach of native libraries.
 *
 * All implemented functions, have exactly the same arguments and/or result of the original function,
 * with the exception of returning an error indication.
 *
 * Errors are handled by the implementation of two policies:
 *      -# EXIT_POLICY (default): describes the failed call in `stderr` (with the identification
 *         of the errno error, and the precise location the call), generates a segmentation fault
 *         (enabling a stack trace within a debugger like `gdb`), and exits program execution;
 *      -# EXCEPTION_POLICY: throws a `int` exception with the (errno) status error returned by the original function.
 * 
 * <p><b>System V group of IPC mechanisms</b>
 *
 * @anchor sysV
 *
 * There are three resources for IPC: \ref shm "shared memory", \ref sem "semaphores", and \ref msg "message queues".
 *
 * Common to all, is resource handling through a non-negative integer identifier.
 * After proper creation and initialization, different processes can establish communication
 * using the same integer identifier.
 *
 * A key (`key_t`) is used to establish this common identifier.
 * There are three possibilities to define a key:
 *
 * 1. `IPC_PRIVATE`: In this case an alternative channel to communicate the identifier
 *    between processes is necessary (parent/child fork, file system, ...).
 *
 * 2. A fixed predetermined key number (may collide with other existing keys).
 *
 * 3. `ftok` function to generate a key from a path and a byte integer.
 *
 * Usage:
 *
 * - create new identifier: `msgget`/`semget`/`shmget` with a key and `IPC_CREAT` and `IPC_EXCL` flags
 * - get existing identifier: `msgget`/`semget`/`shmget` with a key and other arguments as zero
 *
 * \author Miguel Oliveira e Silva, 2017-2018
 **/

#ifndef PROCESS_H
#define PROCESS_H

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>

//#define EXCEPTION_POLICY
//#define EXIT_POLICY // DEFAULT


/** @name Process handling
 *  @anchor process @{
 */

/**
 *  \brief `fork` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 fork @endverbatim
 *
 *  @see https://man.cx/fork(2)
 */
pid_t pfork(void);

/**
 *  \brief `wait` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 wait @endverbatim
 *
 *  @see https://man.cx/wait(2)
 */
pid_t pwait(int *status);

/**
 *  \brief `waitpid` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 waitpid @endverbatim
 *
 *  @see https://man.cx/waitpid(2)
 */
pid_t pwaitpid(pid_t pid, int *status, int options);

/** @} */


/** @name System V - shared memory
 *  @anchor shm
 *
 *  \par <b>Example</b>
 *  @code
    #include <sys/shm.h>
    ...
    int shmid;
    // creation:
    shmid = pshmget(key, size, 0600 | IPC_CREAT | IPC_EXCL);
    // or, use existing:
    shmid = pshmget(key, 0, 0);
    ...
    // attach shm to pointer address:
    void* p = pshmat(shmid, NULL, 0);
    ...
    // detach shm from pointer address:
    pshmdt(p);
    ...
    // destroy shm:
    pshmctl(shmid, IPC_RMID, NULL);
    @endcode
 *
 *  @{
 */

/**
 *  \brief `shmget` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>size > 0</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 2 shmget @endverbatim
 *
 *  @see https://man.cx/shmget(2)
 */
int pshmget(key_t key, size_t size, int shmflg);

/**
 *  \brief `shmctl` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 shmctl @endverbatim
 *
 *  @see https://man.cx/shmctl(2)
 */
int pshmctl(int shmid, int cmd, struct shmid_ds *buf);

/**
 *  \brief `shmat` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 shmat @endverbatim
 *
 *  @see https://man.cx/shmat(2)
 */
void *pshmat(int shmid, const void *shmaddr, int shmflg);

/**
 *  \brief `shmdt` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 shmdt @endverbatim
 *
 *  @see https://man.cx/shmdt(2)
 */
void pshmdt(const void *shmaddr);

/** @} */


/** @name System V - semaphores
 *  @anchor sem
 *
 *  \par <b>Example</b>
 *  @code
    #include <sys/sem.h>
    ...
    int semid;
    // creation:
    semid = psemget(key, 1, 0600 | IPC_CREAT | IPC_EXCL); // 1 semaphore!
    // or, use existing:
    semid = psemget(key, 0, 0);
    ...
    // lock:
    struct sembuf down = {0, -1, 0};
    psemop(semid, &down, 1);
    // or, simply use provided function:
    psem_down(semid, 0);
    ...
    //unlock:
    struct sembuf up = {0, 1, 0};
    psemop(semid, &up, 1);
    // or, simply use provided function:
    psem_up(semid, 0);
    ...
    // destroy sem 0:
    psemctl(semid, 0, IPC_RMID, NULL);
    @endcode
 *
 *  @{
 */

/**
 *  \brief `semget` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>nsems > 0</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 2 semget @endverbatim
 *
 *  @see https://man.cx/semget(2)
 */
int psemget(key_t key, int nsems, int semflg);

/**
 *  \brief `semctl` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 semctl @endverbatim
 *
 *  @see https://man.cx/semctl(2)
 */
int psemctl(int semid, int semnum, int cmd, ...);

/**
 *  \brief `semop` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 semop @endverbatim
 *
 *  @see https://man.cx/semop(2)
 */
void psemop(int semid, struct sembuf *sops, size_t nsops);

/**
 *  \brief Increment a semaphore (uses psemop()).
 */
void psem_up(int semid, short unsigned int index);

/**
 *  \brief Decrements a semaphore (uses psemop()).
 */
void psem_down(int semid, short unsigned int index);

/** @} */


/** @name System V - message queues
 *  @anchor msg
 *
 *  \par <b>Example</b>
 *  @code
    #include <sys/msg.h>
    ...
    typedef struct Item
    {
       ...
    } Item;
    typedef struct Message
    {
       long type;
       Item item;
    } Message;
    ...
    int msgid;
    // creation:
    msgid = pmsgget(key, 0600 | IPC_CREAT | IPC_EXCL);
    // or, use existing:
    msgid = pmsgget(key, 0);
    ...
    Message msg;
    // send msg:
    msg = ...;
    pmsgsnd(msgid, &msg, sizeof(Item), 0);
    ...
    // receive msg:
    pmsgrcv(msgid, &msg, sizeof(Item), type, 0);
    ...
    // destroy msg:
    pmsgctl(msgid, IPC_RMID, NULL);
    @endcode
 *
 *  @{
 */

/**
 *  \brief `msgget` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 msgget @endverbatim
 *
 *  @see https://man.cx/msgget(2)
 */
int pmsgget(key_t key, int msgflg);

/**
 *  \brief `msgctl` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 msgctl @endverbatim
 *
 *  @see https://man.cx/msgctl(2)
 */
int pmsgctl(int msqid, int cmd, struct msqid_ds *buf);

/**
 *  \brief `msgsnd` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 msgsnd @endverbatim
 *
 *  @see https://man.cx/msgsnd(2)
 */
void pmsgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);

/**
 *  \brief `msgrcv` wrapper function.
 *
 *  \details Documentation in @verbatim man 2 msgrcv @endverbatim
 *
 *  @see https://man.cx/msgrcv(2)
 */
size_t pmsgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);

/** @} */


/** @name POSIX semaphores
 *  @anchor posix_sem @{
 */

// named:
/**
 *  \brief `sem_open` wrapper function.
 *
 *  \details Documentation in @verbatim man 3 sem_open @endverbatim
 *
 *  @see https://man.cx/sem_open(3)
 */
sem_t *psem_open(const char *name, int oflag, ...);

/**
 *  \brief `sem_close` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_close @endverbatim
 *
 *  @see https://man.cx/sem_close(3)
 */
void psem_close(sem_t *sem);

/**
 *  \brief `sem_unlink` wrapper function.
 *
 *  \details Documentation in @verbatim man 3 sem_unlink @endverbatim
 *
 *  @see https://man.cx/sem_unlink(3)
 */
void psem_unlink(const char *name);

// unnamed:
/**
 *  \brief `sem_init` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_init @endverbatim
 *
 *  @see https://man.cx/sem_init(3)
 */
void psem_init(sem_t *sem, int pshared, unsigned int value);

/**
 *  \brief `sem_destroy` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_destroy @endverbatim
 *
 *  @see https://man.cx/sem_destroy(3)
 */
void psem_destroy(sem_t *sem);

// named and unnamed:
/**
 *  \brief `sem_wait` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_wait @endverbatim
 *
 *  @see https://man.cx/sem_wait(3)
 */
void psem_wait(sem_t *sem);

/**
 *  \brief `sem_trywait` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_trywait @endverbatim
 *
 *  @see https://man.cx/sem_trywait(3)
 *
 *  \return true (`!=0`) on success, false (`0`) if semaphore is zero
 */
int psem_trywait(sem_t *sem);

/**
 *  \brief `sem_timedwait` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_timedwait @endverbatim
 *
 *  @see https://man.cx/sem_timedwait(3)
 *
 *  \return true (`!=0`) on success, false (`0`) if timeout has expired before being able to decrement the semaphore
 */
int psem_timedwait(sem_t *sem, const struct timespec *abs_timeout);

/**
 *  \brief `sem_post` wrapper function.
 *
 *  <DL><DT><B>Precondition:</B></DT>
 *     <DD><code>sem != NULL</code></DD>
 *  </DL>
 *
 *  \details Documentation in @verbatim man 3 sem_post @endverbatim
 *
 *  @see https://man.cx/sem_post(3)
 */
void psem_post(sem_t *sem);

/** @} */

#endif
