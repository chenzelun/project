#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <syslog.h>
#define BREAD_NUMBER_SHMKEY 6666666L
#define BREAD_NUMBER_FULL_SEMPHKEY 9999999L
#define BREAD_NUMBER_EMPTY_SEMPHKEY 1111111L
#define BREAD_NUMBER_MUTEX_SEMPHKEY 2222222L
#define BAKER_QUEUE_MSGKEY 3333333L
#define BREAD_NUMBER_MAX 200
#define BAKER_NUMBER_MAX 100
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
int bread_number_shmid;
int* bread_number_p;

int bread_number_full_semphid;
int bread_number_empty_semphid;
int bread_number_mutex_semphid;
int baker_message_queue_id;
int baker_number;
time_t current_time;
int bread_id = 0;
FILE* log_fp;
int inotify_fd;
pthread_t baker_tid[BAKER_NUMBER_MAX];
char buffer[EVENT_BUF_LEN];

union semun
{
	int 			val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
};

void P(int semid)
{
    struct sembuf sb;
    sb.sem_num=0;
    sb.sem_op=-1;
    sb.sem_flg=SEM_UNDO;
    semop(semid,&sb,1);
}
void V(int semid)
{
    struct sembuf sb;
    sb.sem_num=0;
    sb.sem_op=1;
    sb.sem_flg=SEM_UNDO;
    semop(semid,&sb,1);
}
void cleanup_m(void * arg)
{
    V(bread_number_mutex_semphid);
    V(bread_number_full_semphid);
}

void *baker(void* arg) {
    int current_baker_tid = pthread_self();
    while(1) {
        
        P(bread_number_empty_semphid);
        P(bread_number_mutex_semphid);
        pthread_cleanup_push(cleanup_m,NULL);
        (*bread_number_p)++;
        time(&current_time);
        fprintf(log_fp,"%s%u号面包师生产了1个面包，面包编号为%d,当前剩余面包数量%d\n",
                asctime(localtime(&current_time)),current_baker_tid,bread_id++,*bread_number_p);
        fflush(log_fp); 
        pthread_testcancel();       
        V(bread_number_mutex_semphid);
        V(bread_number_full_semphid);
        pthread_cleanup_pop(0);
        sleep(3);
    }
}
void init() {

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
    *bread_number_p = 0;

    //创建面包数量相关信号量
    if ((bread_number_full_semphid = semget(BREAD_NUMBER_FULL_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get full semphid...");
        exit(1);
    }
    if ((bread_number_empty_semphid = semget(BREAD_NUMBER_EMPTY_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get full semphid...");
        exit(1);
    }
    if ((bread_number_mutex_semphid = semget(BREAD_NUMBER_MUTEX_SEMPHKEY, 1, IPC_CREAT | 0660)) <= 0) {
        perror("Could not get full semphid...");
        exit(1);
    }
    
    // if((baker_message_queue_id=msgget(BAKER_QUEUE_MSGKEY,IPC_CREAT|0660))<0) {
    //     perror("Could not create queue...");
    //     exit(1);
    // }
    //初始化信号量
    semctl(bread_number_mutex_semphid, 0, SETVAL, 1);
    semctl(bread_number_empty_semphid, 0, SETVAL, 200);
    semctl(bread_number_full_semphid, 0, SETVAL, 0);

    //读取面包师线程数
    FILE *fp;
    fp=0;
    if((fp=fopen("/home/tao/workspace/Linux编程/project/bakerd.conf","r"))==0)
    {
            printf("文件打开失败!\n");
            exit(1);
    }
    fscanf(fp,"BakerNumber=%d",&baker_number);

    log_fp=0;
    if((log_fp=fopen("/home/tao/workspace/Linux编程/project/baker_log.txt","w"))==0)
    {
            printf("文件打开失败!\n");
            exit(1);
    }
} 

int main(int argc, char *argv[]) {
    
    pid_t process_id = 0;
    pid_t sid = 0;
    // Create child process
    process_id = fork();
    // Indication of fork() failure
    if (process_id < 0)
    {
        printf("fork failed!\n");
        // Return failure in exit status
        exit(1);
    }
    // PARENT PROCESS. Need to kill it.
    if (process_id > 0)
    {
        // printf("process_id of child process %d \n", process_id);
        // return success in exit status
        exit(0);
    }
    //unmask the file mode
    umask(0);
    //set new session
    sid = setsid();
    if (sid < 0)
    {
        // Return failure
        exit(1);
    }
    // Change the current working directory to root.
    chdir("/home/tao/workspace/Linux编程/project/");
    // Close stdin. stdout and stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    init();
    int i = 0;
    // for (i = 0; i < baker_number; i++) {
        pthread_create(&(baker_tid[i]),NULL,baker,NULL);
    //     syslog(LOG_USER | LOG_INFO, "Program[%s]:新增了一个新的面包师,当前面包师数量为%d \n", argv[0],i+1);
    // }

    inotify_fd = inotify_init();
    int wd = inotify_add_watch(inotify_fd, "/home/tao/workspace/Linux编程/project/bakerd.conf", IN_CLOSE_WRITE);
    while(1){
        int length = read(inotify_fd, buffer, EVENT_BUF_LEN);
        int i = 0;
        struct inotify_event *event = (struct inotify_event *) &buffer[i];
        if (event->mask & IN_CLOSE_WRITE) {
            FILE *new_fp;
            if((new_fp=fopen("/home/tao/workspace/Linux编程/project/bakerd.conf","r"))==0)
            {
                    printf("文件打开失败!\n");
                    exit(1);
            }
            int new_baker_number;
            fscanf(new_fp,"BakerNumber=%d",&new_baker_number);
            if (new_baker_number > baker_number){
                for (i = baker_number;i < new_baker_number; i++){
                    pthread_create(&(baker_tid[i]),NULL,baker,NULL);
                    syslog(LOG_USER | LOG_INFO, "Program[%s]:新增了一个新的面包师,当前面包师数量为%d \n", argv[0],i+1);
                }
                    
            } else {
                for (i = baker_number; i > new_baker_number; i--){
                    pthread_cancel(baker_tid[i]);
                    syslog(LOG_USER | LOG_INFO, "Program[%s]:减少了一个新的面包师,当前面包师数量为%d \n", argv[0],i-1);
                }
            }
            baker_number = new_baker_number;
        }
    }
    // pthread_join(baker_tid[0],NULL);
    return 0;
} 