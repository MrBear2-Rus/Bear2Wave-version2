/*
 * Minimal pthread API for libvzt block prefetch on MSVC.
 * Enables vzt_rd_init_smp() when BEAR2WAVE_VZT_PTHREAD_WIN32 is defined.
 */
#ifndef BEAR2WAVE_PTHREAD_WIN32_H
#define BEAR2WAVE_PTHREAD_WIN32_H

#include <stdlib.h>
#include <windows.h>

#ifndef PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED 1
#endif
#ifndef PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_JOINABLE 0
#endif

typedef struct {
    CRITICAL_SECTION cs;
    int inited;
} pthread_mutex_t;

typedef int pthread_mutexattr_t;
typedef HANDLE pthread_t;

typedef struct {
    int detach;
} pthread_attr_t;

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* attr)
{
    (void)attr;
    if (!m)
        return -1;
    InitializeCriticalSection(&m->cs);
    m->inited = 1;
    return 0;
}

static inline void pthread_mutex_lock(pthread_mutex_t* m)
{
    if (m && m->inited)
        EnterCriticalSection(&m->cs);
}

static inline void pthread_mutex_unlock(pthread_mutex_t* m)
{
    if (m && m->inited)
        LeaveCriticalSection(&m->cs);
}

static inline void pthread_mutex_destroy(pthread_mutex_t* m)
{
    if (m && m->inited) {
        DeleteCriticalSection(&m->cs);
        m->inited = 0;
    }
}

static inline int pthread_attr_init(pthread_attr_t* attr)
{
    if (!attr)
        return -1;
    attr->detach = PTHREAD_CREATE_JOINABLE;
    return 0;
}

static inline int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
    if (!attr)
        return -1;
    attr->detach = detachstate;
    return 0;
}

static inline void pthread_attr_destroy(pthread_attr_t* attr)
{
    (void)attr;
}

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} bear2wave_pthread_start;

static DWORD WINAPI bear2wave_pthread_trampoline(LPVOID param)
{
    bear2wave_pthread_start* start = (bear2wave_pthread_start*)param;
    void* (*fn)(void*) = start->start_routine;
    void* arg = start->arg;
    free(start);
    if (fn)
        fn(arg);
    return 0;
}

static inline int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg)
{
    if (!thread || !start_routine)
        return -1;

    bear2wave_pthread_start* start =
        (bear2wave_pthread_start*)malloc(sizeof(bear2wave_pthread_start));
    if (!start)
        return -1;
    start->start_routine = start_routine;
    start->arg = arg;

    HANDLE h = CreateThread(NULL, 0, bear2wave_pthread_trampoline, start, 0, NULL);
    if (!h) {
        free(start);
        return -1;
    }

    if (attr && attr->detach == PTHREAD_CREATE_DETACHED)
        CloseHandle(h);

    *thread = h;
    return 0;
}

#endif /* BEAR2WAVE_PTHREAD_WIN32_H */
