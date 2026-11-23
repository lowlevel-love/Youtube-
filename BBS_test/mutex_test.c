
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define MAX_ENTRIES 128
#define MAX_THREADS 100
#define THE_LOOP 65535

// 既存ASMの関数
extern uint32_t __set_index(
    uint32_t *array_rb_tail, int index);

// 既存ASMのグローバル変数
uint32_t *sq_head;
uint32_t *sq_tail;
uint32_t *array;

_Atomic uint32_t head = 0;
_Atomic uint32_t tail = 0;
uint32_t array_variable[MAX_ENTRIES];
uint32_t user_array_rb = 0;

static pthread_mutex_t mutex =
    PTHREAD_MUTEX_INITIALIZER;

static _Atomic int finished = 0;
static int use_mutex = 0;

static uint32_t mutex_set_index(int index)
{
    uint32_t result = 0;

    pthread_mutex_lock(&mutex);

    if ((uint32_t)(tail - head) < MAX_ENTRIES) {
        array_variable[tail & (MAX_ENTRIES - 1)] =
            (uint32_t)index;
        tail++;
        result = 1;
    }

    pthread_mutex_unlock(&mutex);
    return result;
}

static void *consumer(void *arg)
{
    (void)arg;

    for (;;) {
        if (use_mutex) {
            pthread_mutex_lock(&mutex);

            if (head != tail)
                head++;

            int done = atomic_load(&finished)
                == MAX_THREADS && head == tail;

            pthread_mutex_unlock(&mutex);

            if (done) break;
        } else {
            if (atomic_load(&head) <
                atomic_load(&tail))
                atomic_fetch_add(&head, 1);

            if (atomic_load(&finished)
                == MAX_THREADS &&
                atomic_load(&head) ==
                atomic_load(&tail))
                break;
        }
    }

    return NULL;
}

static void *worker(void *arg)
{
    (void)arg;

    for (int i = 0; i < THE_LOOP;) {
        uint32_t ok;

        if (use_mutex)
            ok = mutex_set_index(i);
        else
            ok = __set_index(&user_array_rb, i);

        if (ok)
            i++;
    }

    atomic_fetch_add(&finished, 1);
    return NULL;
}

static double run_bench(int mutex_mode)
{
    pthread_t threads[MAX_THREADS];
    pthread_t reader;
    struct timespec start, end;

    use_mutex = mutex_mode;
    head = 0;
    tail = 0;
    user_array_rb = 0;
    finished = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_create(&reader, NULL, consumer, NULL);

    for (int i = 0; i < MAX_THREADS; i++)
        pthread_create(&threads[i], NULL,
                       worker, NULL);

    for (int i = 0; i < MAX_THREADS; i++)
        pthread_join(threads[i], NULL);

    pthread_join(reader, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start.tv_sec) +
        (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(void)
{
    sq_head = (uint32_t *)&head;
    sq_tail = (uint32_t *)&tail;
    array = array_variable;

    double asm_time = run_bench(0);
    double mutex_time = run_bench(1);

    printf("\n===== BENCHMARK =====\n");
    printf("ASM   : %.6f sec\n", asm_time);
    printf("MUTEX : %.6f sec\n", mutex_time);

    if (asm_time < mutex_time) {
        printf("WINNER: ASM\n");
        printf("SPEED : %.2fx\n",
               mutex_time / asm_time);
    } else {
        printf("WINNER: MUTEX\n");
        printf("SPEED : %.2fx\n",
               asm_time / mutex_time);
    }

    return 0;
}
