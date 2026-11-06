// Atomics, locks and wait groups (spec 14.4, docs/design/runtime.md).
//
// Only `std.sync` calls this file. The atomics work on an `int` that
// `std.sync` keeps in an `UnsafeCell`; every operation is sequentially
// consistent. The locks and wait groups live on the heap, so that the
// Ofir values that own them can move.

#include "ofir_rt.h"

#include <pthread.h>

int64_t ofir_atomic_load(int64_t *p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

void ofir_atomic_store(int64_t *p, int64_t value) {
    __atomic_store_n(p, value, __ATOMIC_SEQ_CST);
}

int64_t ofir_atomic_fetch_add(int64_t *p, int64_t delta) {
    return __atomic_fetch_add(p, delta, __ATOMIC_SEQ_CST);
}

int64_t ofir_atomic_swap(int64_t *p, int64_t value) {
    return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

bool ofir_atomic_compare_exchange(int64_t *p, int64_t expected, int64_t desired) {
    return __atomic_compare_exchange_n(p, &expected, desired, false, __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

void *ofir_mutex_new(void) {
    pthread_mutex_t *m = ofir_alloc(sizeof *m);
    pthread_mutex_init(m, NULL);
    return m;
}

void ofir_mutex_lock(void *m) {
    pthread_mutex_lock(m);
}

void ofir_mutex_unlock(void *m) {
    pthread_mutex_unlock(m);
}

void ofir_mutex_free(void *m) {
    pthread_mutex_destroy(m);
    ofir_free(m);
}

void *ofir_rwlock_new(void) {
    pthread_rwlock_t *l = ofir_alloc(sizeof *l);
    pthread_rwlock_init(l, NULL);
    return l;
}

void ofir_rwlock_read(void *l) {
    pthread_rwlock_rdlock(l);
}

void ofir_rwlock_write(void *l) {
    pthread_rwlock_wrlock(l);
}

void ofir_rwlock_unlock(void *l) {
    pthread_rwlock_unlock(l);
}

void ofir_rwlock_free(void *l) {
    pthread_rwlock_destroy(l);
    ofir_free(l);
}

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t zero;   // the count reached zero
    int64_t count;
} waitgroup;

void *ofir_waitgroup_new(void) {
    waitgroup *wg = ofir_alloc(sizeof *wg);
    pthread_mutex_init(&wg->lock, NULL);
    pthread_cond_init(&wg->zero, NULL);
    wg->count = 0;
    return wg;
}

bool ofir_waitgroup_add(void *w, int64_t delta) {
    waitgroup *wg = w;
    pthread_mutex_lock(&wg->lock);
    bool ok = wg->count + delta >= 0;
    if (ok) {
        wg->count += delta;
        if (wg->count == 0) {
            pthread_cond_broadcast(&wg->zero);
        }
    }
    pthread_mutex_unlock(&wg->lock);
    return ok;
}

void ofir_waitgroup_wait(void *w) {
    waitgroup *wg = w;
    pthread_mutex_lock(&wg->lock);
    while (wg->count > 0) {
        pthread_cond_wait(&wg->zero, &wg->lock);
    }
    pthread_mutex_unlock(&wg->lock);
}

void ofir_waitgroup_free(void *w) {
    waitgroup *wg = w;
    pthread_cond_destroy(&wg->zero);
    pthread_mutex_destroy(&wg->lock);
    ofir_free(wg);
}
