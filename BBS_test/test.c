#define _GNU_SOURCE
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<stdatomic.h>
//arrayを競合するためのring buffer

//io_uring array用のrb操作関数
//要件としてindexのセットのみなので成功か失敗だけわかればいい
//io_uringのsq_headとarray用のringbuffer用のtail uint32_tを連動させる
//io_uringのheadは見るだけ
//成功なら1失敗なら0

#define MAX_ENTRIES 128
#define MAX_ENTRIES_MASK MAX_ENTRIES-1

#define MAX_THREADS 100
#define THE_LOOP 65535

extern __attribute__((hot)) uint32_t __set_index(uint32_t *array_rb_tail,int index);
__attribute__((hot)) void *__get_index(void *arg);//headをincしていくだけ
extern __attribute__((hot)) void *thread_worker(void *arg);

uint32_t *sq_head;
uint32_t *sq_tail;
uint32_t *array;

_Atomic uint32_t head=0;
_Atomic uint32_t tail=0;
uint32_t array_variable[MAX_ENTRIES];//2^nの想定
uint32_t user_array_rb=0;//第一引数に入れるためのもの

_Atomic int end_thread=0;


int main(void){
    sq_head=(uint32_t*)&head;
    sq_tail=(uint32_t*)&tail;
    array=(uint32_t*)&array_variable;

    pthread_t thread_id;
    pthread_create(&thread_id,NULL,&__get_index,NULL);
    pthread_t client_id[MAX_THREADS];
    for(int i=0;i<MAX_THREADS;i++){
        pthread_create(&client_id[i],NULL,&thread_worker,NULL);
    }

    for(;;){
        if(__builtin_expect(end_thread>=MAX_THREADS,0))break;
    }
    printf("sq_head=%d\nsq_tail=%d\n",head,tail);
    return 0;
}

extern __attribute__((hot)) void *thread_worker(void *arg){
    int ans=0;
    for(int i=0;i<THE_LOOP;i++){
        if(__set_index(&user_array_rb,i)==0){
            i--;
            continue;
        }else{
            ans++;
        }
    }
    printf("answer=>%d\n",ans);
    atomic_fetch_add((_Atomic int*)&end_thread,1);
    return NULL;
}


__attribute__((hot)) void *__get_index(void *arg){
    for(;;){
        if(end_thread>100)break;
        if(head<tail)head++;
    }
    return NULL;
}