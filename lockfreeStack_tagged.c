#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <stdint.h>

/*-------上位64bitがtag,下位64bitがptr-----*/
#define GetPtr(p) ((Node*)((int64_t)(p)))
#define GetTag(p) ((uint64_t)((p) >> 64))
#define SetPtr(dst, src) {\
    dst &= (((__int128)(0xffffffffffffffff)) << 64);\
    dst |= ((int64_t)(src));\
}
#define SetTag(dst, src) {\
    dst &= 0xffffffffffffffff;\
    dst |= (((__int128)(src)) << 64);\
}

/*-----------------------------------
 * Lock-free Stack tag.ver
 * 64bitのOS用
 * intel x86_64が__int128対応
 * __sync_bool_compare_and_swap_16()で実行される
 * コンパイルオプション -mcx16 -fopenmp
 *----------------------------------*/

// Node 
typedef struct _Node {
    __int128 ptr;
    int value;
} Node;

// Stack Pointer
__int128 head;
// tag Counter
uint64_t tagCnt;

int empty() { return GetPtr(head) == 0; }

// データ挿入
void push(int value){
    __int128 newNode = 0;
    __int128 oldNode = 0;
    uint64_t newTag = 0;
    uint64_t oldTag = 0;

    /*--------allocate-------*/
    SetPtr(newNode, (Node*)malloc(sizeof(Node)));
    GetPtr(newNode)->value = value;

    /*--------タグ更新-------*/
    do{
        oldTag = tagCnt;
        newTag = oldTag + 1;
    }while(!__sync_bool_compare_and_swap(&tagCnt, oldTag, newTag));
    SetTag(newNode, newTag);
    SetTag(GetPtr(newNode)->ptr, newTag);

    /*--------head更新-------*/
    do{
        oldNode = head;
        SetPtr(GetPtr(newNode)->ptr, GetPtr(oldNode));
    }while(!__sync_bool_compare_and_swap(&head, oldNode, newNode));
}

// データ削除
void pop(){
    __int128 prev = 0;
    __int128 oldNode = 0;
    
    while(1){
        oldNode = head;

        if(GetPtr(oldNode) == 0){ return; }

        prev = GetPtr(oldNode)->ptr;
        if(__sync_bool_compare_and_swap(&head, oldNode, prev)){
            //SetPtr(GetPtr(oldNode), 0);
            //memset(GetPtr(oldNode), 0, sizeof(Node));
            free(GetPtr(oldNode));
            return;
        }
    }
}

void stack_destroy(){
    __int128 oldNode = 0;
    __int128 prev = 0;

    while(GetPtr(head) != NULL){
        oldNode = head;
        head = GetPtr(head)->ptr;
        free(GetPtr(oldNode));
    }
    tagCnt = 0;
}

// Stack表示
void show() {
    int cnt = 0;
    __int128 p = head;

    while(GetPtr(p) != NULL){
        //printf("val:%05d,\ttag:%05d,\tadress:%p\n", GetPtr(p)->value, (int)GetTag(GetPtr(p)->ptr), GetPtr(p));
        p = GetPtr(p)->ptr;
        cnt++;
    }
    printf("\nStackの個数:%d\n",cnt);
    printf("\nStackにPushした総数:%d\n", (int)tagCnt);
}

int main(void) {
    int i;
    omp_set_num_threads();
    
#pragma omp parallel private(i)
    {
        for(i = 0; i < 1000000; ++i){
            push(i);
            pop();
            push(i);
            pop();
        }
    }

#pragma omp parallel private(i)
    {
        for(i = 0; i < 100000; ++i){
           push(i);
           pop();
           push(i);
           pop();
           push(i);
        }
    }
        
    show();
    stack_destroy();
    return 0;
}
