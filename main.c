//
//  main.c
//  Server
//
//  Created by Владислав Агафонов on 05.11.2017.
//  Copyright © 2017 Владислав Агафонов. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //sleep
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#define TASKS_N 100
#define THREADS_N 10
#define RAND_PARAM 10000
#define LAST_MESSAGE 255 //тип сообщения для прекращения работы программы

void addToResults(int number, long PID);
void addToTasks(long PID, char* number);
void* my_thread(void* dummy);
void* sender(void* dummy);


typedef enum{
    NEW,
    PROCESSING,
    DONE,
    EMPTY,
    FULL
} statusesTask;
typedef enum{
    READY,
    NOTHING
} statusesResult;
typedef struct{
    int duration;
    int number;
    statusesTask status;
    pthread_t worker;
    long clientPID;
} task_t;
typedef struct{
    int res;
    long PID;
    statusesResult status;
} result_t;

pthread_mutex_t lock;
task_t tasks[TASKS_N];
result_t results[TASKS_N];
pthread_t thread_id[THREADS_N];

void* sender(void* msqId)
{
    int* copy = (int*)msqId;
    //Пользовательская структура для сообщения
    struct mymsgBuf {
        long mtype;
        char mtext[10];
    } myMsqBuf;
    unsigned long len;
    while (1)
    {
        for (int i = 0; i < TASKS_N; i++)
        {
            if (results[i].status == READY)
            {
                /* Сначала заполняем структуру для нашего сообщения и определяем длину информативной части */
                myMsqBuf.mtype = results[i].PID;
                char a[20];
                sprintf(a, "%d", results[i].res);
                strcpy(myMsqBuf.mtext, a);
                len = strlen(myMsqBuf.mtext) + 1;
                
                /* Отсылаем сообщение. В случае ошибки сообщаем об этом и удаляем очередь сообщений из системы. */
                if (msgsnd(*copy, (struct mymsgBuf *) &myMsqBuf, len, 0) < 0)
                {
                    printf("Can\'t send message to queue\n");
                    //msgctl(*copy, IPC_RMID, (struct msqid_ds*)NULL);
                    exit(-1);
                }
                results[i].status = NOTHING;
            }
        }
    }
    
    return 0;
}

void* my_thread(void* dummy)
{
    pthread_t worker = pthread_self();
    while (1)
    {
        for (int i = 0; i < TASKS_N; i++)
        {
            pthread_mutex_lock(&lock);
            if (tasks[i].status == FULL)
            {
                tasks[i].status = PROCESSING;
                tasks[i].worker = worker;
            }
            pthread_mutex_unlock(&lock);
            if ( (tasks[i].status == PROCESSING) && (tasks[i].worker == worker) )
            {
                usleep(100 * tasks[i].duration);
                int sq = tasks[i].number * tasks[i].number;
                addToResults(sq, tasks[i].clientPID);
                tasks[i].status = EMPTY;
                tasks[i].clientPID = 0;
                tasks[i].number = 0;
                tasks[i].duration = 0;
            }
        }
    }
    
    return 0;
}

void addToTasks(long PID, char* number)
{
    int flag = 0;
    for (int i = 0; i < TASKS_N; i++)
    {
        if (tasks[i].status == EMPTY)
        {
            tasks[i].status = FULL;
            tasks[i].number = atoi(number);
            tasks[i].duration = atoi(number);
            tasks[i].clientPID = PID;
            flag = 1;
            break;
        }
    }
    if (flag == 0)
    {
        addToTasks(PID, number);
    }
}

void addToResults(int number, long PID)
{
    int flag = 0;
    for (int i = 0; i < TASKS_N; i++)
    {
        if (results[i].status == NOTHING)
        {
            results[i].status = READY;
            results[i].PID = PID;
            results[i].res = number;
            flag = 1;
            break;
        }
    }
    if (flag == 1)
    {
        addToResults(number, PID);
    }
}

int main(int argc, const char * argv[])
{
    printf("Server starts...\n");
    int len;//длина сообщения в очереди сообщений
    int result;
    int maxlen = 10;//максимальная длина в очереди сообщений
    
    int msqId; //IPC-дескриптор очереди сообщений для задач
    key_t msqKey = 125; // IPC-ключ
    
    int msqIdRes; //IPC-дескриптор очереди сообщений для результатов
    key_t msqKeyRes = 121; //IPC-ключ
    
    //Пользовательская структура для сообщения
    struct mymsgBuf {
        long mtype;
        char mtext[10];
    } myMsqBuf;
    
    /* Пытаемся получить доступ по ключу к очереди сообщений, если она существует,
     или создать ее, если она еще не существует, с правами доступа
     read & write для всех пользователей */
    if((msqId = msgget(msqKey, 0666 | IPC_CREAT)) < 0) {
        printf("Can\'t get msqId\n");
        exit(-1);
    }
    
    /* Пытаемся получить доступ по ключу к очереди сообщений, если она существует,
     или создать ее, если она еще не существует, с правами доступа
     read & write для всех пользователей */
    if((msqIdRes = msgget(msqKeyRes, 0666 | IPC_CREAT)) < 0) {
        printf("Can\'t get msqId\n");
        exit(-1);
    }
    
    pthread_mutex_init(&lock, NULL); //инициализация лока
    
    //инициализация массива задач
    for (int i = 0; i < TASKS_N; i++)
    {
        tasks[i].status = EMPTY;
    }
    //инициализация массива результатов
    for (int i = 0; i < TASKS_N; i++)
    {
        results[i].status = NOTHING;
    }
    
    //создание тредов
    for (int i = 0; i < THREADS_N; i++) {
        result = pthread_create(&thread_id[i], NULL, my_thread, NULL);
        if (result) {
            printf("Can't create thread, returned value = %d\n", result);
            exit(-1);
        }
    }
    
    //создание треда для передачи
    pthread_t threadID;
    result = pthread_create(&threadID, NULL, sender, (void*)&msqIdRes);
    if (result) {
        printf("Can't create thread, returned value = %d\n", result);
        exit(-1);
    }
    
    printf("All threads created\n");
    //pthread_mutex_destroy(&lock);

    while (1)
    {
        if ((len = (int)msgrcv(msqId, (struct msgbuf *)&myMsqBuf, maxlen, 0, 0)) < 0)
        {
            printf("Can\'t receive message from queue\n");
            exit(-1);
            
        }
        printf("Client's PID = %ld, Client's task = %s\n", myMsqBuf.mtype, myMsqBuf.mtext);
        addToTasks(myMsqBuf.mtype, myMsqBuf.mtext);
    }
    
    return 0;
}
