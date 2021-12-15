#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> 
#define SALESMAN_SEMAPKEY 7777777L      //售货员相关资源信号量
#define SALESMAN_TOTAL_NUMBER_SHMKEY 5555555L //售货员总数量
#define CUSTOMER_QUEUE_MSGKEY 8888888L  //顾客数据消息队列

typedef struct
{
    long message_type;
	int customer_id;
	int need_bread_number;
} customer_info;

int main()
{
    int salesman_total_number_shmid;
    int *salesman_total_number_p;
    int salesman_semaph_id;
    
    int customer_message_queue_id;

    int customer_id = getpid();
    srand((unsigned int)(time(NULL)));
    int customer_need_bread = (rand() % 4) + 1;
    customer_info current_customer_info,response;
    current_customer_info.message_type = 1;
    current_customer_info.customer_id = customer_id;
    current_customer_info.need_bread_number = customer_need_bread;

    //新建消息队列
	if((customer_message_queue_id=msgget(CUSTOMER_QUEUE_MSGKEY,IPC_CREAT|0660))<0) {
        perror("Could not create queue...");
		exit(1);
	}

    printf("一个新顾客进入了面包店，顾客id为%d,想要购买%d个面包\n",customer_id,customer_need_bread);

    if(msgsnd(customer_message_queue_id,&current_customer_info,sizeof (current_customer_info),IPC_NOWAIT)<0) {
        printf("排队人数超过了3倍售货员数量，放弃了购买");
		exit(1);
	} 
    msgrcv(customer_message_queue_id,&response,sizeof(response),customer_id,0);
    printf("购买成功，离开商店\n");

    return 0;
}
