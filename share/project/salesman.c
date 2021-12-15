#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#define CUSTOMER_QUEUE_MSGKEY 8888888L
#define BREAD_NUMBER_MUTEX_SEMPHKEY 2222222L
#define BREAD_NUMBER_FULL_SEMPHKEY 9999999L
#define BREAD_NUMBER_EMPTY_SEMPHKEY 1111111L
#define BREAD_NUMBER_SHMKEY 6666666L
#define BAKER_QUEUE_MSGKEY 3333333L
#define SALESMAN_MAX_NUMBER 10

typedef struct
{
    long message_type;
	int customer_id;
	int need_bread_number;
} customer_info;

int customer_avail = 0;
customer_info temp_customer_info;

pthread_mutex_t m; 
pthread_cond_t cv;
pthread_key_t key;

int bread_number_full_semphid;
int bread_number_empty_semphid;
int bread_number_mutex_semphid;
int bread_number_shmid;
int* bread_number_p;
int baker_message_queue_id;
int customer_message_queue_id;
int salesman_number;
pthread_t salesman_thread_id[SALESMAN_MAX_NUMBER];
time_t current_time;
FILE* log_fp;

typedef struct
{
    long message_type;
} produce_message;
produce_message temp_message;

void P(int semid)
{
    struct sembuf sb;
    sb.sem_num=0;
    sb.sem_op=-1;
    sb.sem_flg=0;
    semop(semid,&sb,1);
}
void V(int semid)
{
    struct sembuf sb;
    sb.sem_num=0;
    sb.sem_op=1;
    sb.sem_flg=0;
    semop(semid,&sb,1);
}
// void P(int semid,int value)
// {
//     struct sembuf sb;
//     sb.sem_num=0;
//     sb.sem_op=value;
//     sb.sem_flg=0;
//     semop(semid,&sb,1);
// }

void *salesman(void* arg) {
    pthread_t current_salesman_tid = pthread_self();
    customer_info *local_customer;
    local_customer =(customer_info*)malloc(sizeof(customer_info));
    pthread_setspecific(key, (void *)local_customer);
    while(1){
        // printf("%d号销售员开始工作\n",current_salesman_tid);
        pthread_mutex_lock(&m);
        while(customer_avail <= 0) {
            pthread_cond_wait(&cv, &m);
        }
        local_customer->customer_id = temp_customer_info.customer_id;
        local_customer->need_bread_number = temp_customer_info.need_bread_number;
        customer_avail--;
        pthread_mutex_unlock(&m);

        for (int i = 0; i < local_customer->need_bread_number; i++) {
            P(bread_number_full_semphid);
            P(bread_number_mutex_semphid);
            // printf("已经为顾客取出来面包1个面包");
            (*bread_number_p)--;
            V(bread_number_mutex_semphid);
            V(bread_number_empty_semphid);
            
        }
        
        sleep(1);
        P(bread_number_mutex_semphid);
        fprintf(log_fp,"%s%lu号销售员为%d号顾客取走了%d个面包，当前剩余面包数量%d\n",
                asctime(localtime(&current_time)),current_salesman_tid,local_customer->customer_id,local_customer->need_bread_number,(*bread_number_p));
        customer_info buy_susscess;
        buy_susscess.message_type = local_customer->customer_id;
        msgsnd(customer_message_queue_id,&buy_susscess,sizeof (buy_susscess),IPC_NOWAIT);
        // printf("%s%lu号销售员为%d号顾客取走了%d个面包，当前剩余面包数量%d\n",
        //         asctime(localtime(&current_time)),current_salesman_tid,local_customer->customer_id,local_customer->need_bread_number,(*bread_number_p));
        fflush(log_fp);
        V(bread_number_mutex_semphid);

        // printf("%lu号销售员为%d号顾客取了%d个面包\n",
        //         current_salesman_tid, local_customer->customer_id, local_customer->need_bread_number);
    }
    pthread_exit(NULL);
}

void init() {
    srand(time(0));
    salesman_number = (rand() % (SALESMAN_MAX_NUMBER-1)) + 2;

    //创建面包数量共享内存
    if ((bread_number_shmid = shmget(BREAD_NUMBER_SHMKEY, sizeof(int), IPC_CREAT | 0660)) < 0)
    {
        perror("Could not get shmid...");
        exit(1);
    }
    if ((bread_number_p = shmat(bread_number_shmid, 0, 0)) <= 0)
    { 
        perror("Could not shmat...");
        exit(2);
    }
    if ((bread_number_full_semphid = semget(BREAD_NUMBER_FULL_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get full semphid...");
        exit(1);
    }
    if ((bread_number_empty_semphid = semget(BREAD_NUMBER_EMPTY_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get full semphid...");
        exit(1);
    }
    if ((bread_number_mutex_semphid = semget(BREAD_NUMBER_MUTEX_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get mutex semphid...");
        exit(1);
    }
    //创建消息队列
    if((customer_message_queue_id=msgget(CUSTOMER_QUEUE_MSGKEY,IPC_CREAT|0660))<0) {
        perror("Could not create queue...");
        exit(1);
    }
    if((baker_message_queue_id=msgget(BAKER_QUEUE_MSGKEY,IPC_CREAT|0660))<0) {
        perror("Could not create queue...");
        exit(1);
    }

    //更改消息队列长度即排队长度
    struct msqid_ds msq_attr; 
    msgctl(customer_message_queue_id,IPC_STAT,&msq_attr);
    msq_attr.msg_qbytes = sizeof(customer_info) * salesman_number * 3;
    msgctl(customer_message_queue_id,IPC_SET,&msq_attr);

    log_fp=0;
    if((log_fp=fopen("/home/tao/workspace/Linux编程/project/salesman_log.txt","w"))==0)
    {
            printf("文件打开失败!\n");
            exit(1);
    }
}
int main() {
    init();
    customer_info current_customer;
    for (int i = 0; i < salesman_number; i++) {
        pthread_create(&(salesman_thread_id[i]), NULL, salesman, NULL);
    }
    //接受消息
    while(1) {

        while(1){
            pthread_mutex_lock(&m);
            if(customer_avail > 0){
                pthread_cond_signal(&cv);
                pthread_mutex_unlock(&m);
            } else {
                pthread_mutex_unlock(&m);
                break;
            }
        }
        msgrcv(customer_message_queue_id,&current_customer,sizeof(current_customer),1,0);
        pthread_mutex_lock(&m);
        temp_customer_info.customer_id = current_customer.customer_id;
        temp_customer_info.need_bread_number = current_customer.need_bread_number;
        customer_avail++;
        pthread_cond_signal(&cv);
        pthread_mutex_unlock(&m);
    }
    return 0;
}