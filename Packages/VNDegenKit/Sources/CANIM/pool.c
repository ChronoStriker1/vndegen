#include "anim.h"

#include <pthread.h>
#include <unistd.h>

enum { POOL_MAX = 8 };

struct Pool {
    pthread_t thread[POOL_MAX];
    int threads;
    pthread_mutex_t lock;
    pthread_cond_t work, done;
    void (*fn)(void *, int);
    void *ctx;
    int jobs, next, finished;
    bool quit;
};

static bool take(struct Pool *p, void (**fn)(void *, int), void **ctx, int *job) {
    if (p->next >= p->jobs) return false;
    *fn = p->fn;
    *ctx = p->ctx;
    *job = p->next++;
    return true;
}

static void finish(struct Pool *p) {
    if (++p->finished == p->jobs) pthread_cond_broadcast(&p->done);
}

static void *worker(void *arg) {
    struct Pool *p = arg;
    pthread_mutex_lock(&p->lock);
    for (;;) {
        void (*fn)(void *, int);
        void *ctx;
        int job;
        while (!p->quit && !take(p, &fn, &ctx, &job)) pthread_cond_wait(&p->work, &p->lock);
        if (p->quit) break;
        pthread_mutex_unlock(&p->lock);
        fn(ctx, job);
        pthread_mutex_lock(&p->lock);
        finish(p);
    }
    pthread_mutex_unlock(&p->lock);
    return NULL;
}

static struct Pool *pool(AN *a) {
    if (a->pool) return a->pool;
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int want = cpus > POOL_MAX ? POOL_MAX : cpus < 1 ? 1 : (int)cpus;
    struct Pool *p = an_alloc(sizeof *p);
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->work, NULL);
    pthread_cond_init(&p->done, NULL);
    for (int i = 0; i + 1 < want; i++)
        if (!pthread_create(&p->thread[p->threads], NULL, worker, p)) p->threads++;
    a->pool = p;
    return p;
}

int an_pool_size(AN *a) { return pool(a)->threads + 1; }

void an_parallel(AN *a, int jobs, void (*fn)(void *, int), void *ctx) {
    if (jobs <= 0) return;
    struct Pool *p = pool(a);
    if (jobs == 1 || !p->threads) {
        for (int i = 0; i < jobs; i++) fn(ctx, i);
        return;
    }
    pthread_mutex_lock(&p->lock);
    p->fn = fn;
    p->ctx = ctx;
    p->jobs = jobs;
    p->next = 0;
    p->finished = 0;
    pthread_cond_broadcast(&p->work);
    void (*f)(void *, int);
    void *c;
    int job;
    while (take(p, &f, &c, &job)) {
        pthread_mutex_unlock(&p->lock);
        f(c, job);
        pthread_mutex_lock(&p->lock);
        finish(p);
    }
    while (p->finished < p->jobs) pthread_cond_wait(&p->done, &p->lock);
    p->jobs = 0;
    p->next = 0;
    pthread_mutex_unlock(&p->lock);
}

void an_pool_free(AN *a) {
    struct Pool *p = a->pool;
    if (!p) return;
    pthread_mutex_lock(&p->lock);
    p->quit = true;
    pthread_cond_broadcast(&p->work);
    pthread_mutex_unlock(&p->lock);
    for (int i = 0; i < p->threads; i++) pthread_join(p->thread[i], NULL);
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->work);
    pthread_cond_destroy(&p->done);
    free(p);
    a->pool = NULL;
}
