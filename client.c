//
//  main.c
//  Client
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

int main(int argc, const char * argv[])
{
    printf("You enter number %s\n", argv[1]);
    printf("My PID is %d\n", getpid());
    /* IPC дескриптор для очереди сообщений */
    int msqid;
    
    /* IPC ключ */
    key_t key = 125;
    
    int msqIdRes; //IPC-дескриптор очереди сообщений для результатов
    key_t msqKeyRes = 121; //IPC-ключ
    
    /* Длина информативной части сообщения */
    int len;
    
    /* Ниже следует пользовательская структура для сообщения */
    struct mymsgbuf
    {
        long mtype;
        char mtext[81];
    } mybuf;
    
    int maxlen;
    /* Пытаемся получить доступ по ключу к очереди сообщений, если она существует,
     или создать ее, если она еще не существует, с правами доступа
     read & write для всех пользователей */
    if((msqid = msgget(key, 0666 | IPC_CREAT)) < 0)
    {
        printf("Can\'t get msqid\n");
        exit(-1);
    }
    
    /* Пытаемся получить доступ по ключу к очереди сообщений, если она существует,
     или создать ее, если она еще не существует, с правами доступа
     read & write для всех пользователей */
    if((msqIdRes = msgget(msqKeyRes, 0666 | IPC_CREAT)) < 0) {
        printf("Can\'t get msqId\n");
        exit(-1);
    }
    
    /* Сначала заполняем структуру для нашего сообщения и определяем длину информативной части */
    mybuf.mtype = getpid();
    strcpy(mybuf.mtext, argv[1]);
    len = (int)strlen(mybuf.mtext) + 1;
        
    /* Отсылаем сообщение. В случае ошибки сообщаем об этом и удаляем очередь сообщений из системы. */
    if (msgsnd(msqid, (struct msgbuf *) &mybuf, len, 0) < 0)
    {
        printf("Can\'t send message to queue\n");
        //msgctl(msqid, IPC_RMID, (struct msqid_ds*)NULL);
        exit(-1);
    }
    
    int exitCode = 0;
    maxlen = 20;

    while (exitCode == 0)
    {
        if ((len = (int)msgrcv(msqIdRes, (struct msgbuf *)&mybuf, maxlen, getpid(), 0)) < 0)
        {
            printf("Can\'t receive message from queue\n");
            exit(-1);
        }
        if (mybuf.mtype == getpid())
        {
            exitCode = 1;
            printf("MyPID = %ld, Result = %s\n", mybuf.mtype, mybuf.mtext);
        }
        usleep(10);
    }
    
    return 0;
}
