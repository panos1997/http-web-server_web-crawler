#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <libgen.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_FILE_SIZE 1000000                        //files will not exceed that limit
pthread_cond_t cond_nonempty;                   //condition variables for empty pool and full pool
pthread_cond_t cond_nonfull;
char* root_dir;
int total_pages_num=0;
int total_bytes_num=0;
struct timeval t1,t2;
double time_passed;
pthread_mutex_t mutex;            //for pool struct variables
pthread_mutex_t mutex2;             //for protecting total_pages_num,total_bytes_num,time_passed
//pthread_mutex_t mutex3;             //for nonempty condition varable
//pthread_mutex_t mutex4;          //for nonfull condition variable
//int connection_socket=0;
#define POOL_SIZE 300           //pool is a struct that contains the file descriptors that threads will need for the connection with the client
typedef struct {                        //when there is a file descriptor in the pool,a thread extracts it
    int connection_sockets[POOL_SIZE];            //the array of connection sockets (=file descriptors)
    int start;
    int end;
    int count;                          //how many fds are at the same time
} pool_t;

pool_t pool;

void initialize(pool_t * pool) {                       //initialize pool
    pool->start = 0;
    pool->end = -1;
    pool->count = 0;
}

void place(pool_t * pool, int data) {           //place a file descriptor to the pool
    pthread_mutex_lock(&mutex);
    while (pool->count >= POOL_SIZE) {
        printf(">> Found Buffer Full \n");
        pthread_cond_wait(&cond_nonfull, &mutex);       //if buffer is full,wait here
    }
    pool->end = (pool->end + 1) % POOL_SIZE;
    pool->connection_sockets[pool->end] = data;
    pool->count++;
    pthread_mutex_unlock(&mutex);
}

int obtain(pool_t * pool) {                             //obtain a file descriptor from the pool
    int data=0;
    pthread_mutex_lock(&mutex);
    while (pool->count <= 0) {
        printf(">> Found Buffer Empty \n");
        pthread_cond_wait(&cond_nonempty, &mutex);        //if buffer is empty,wait here
    }
    data = pool->connection_sockets[pool->start];
    pool->start = (pool->start + 1) % POOL_SIZE;
    pool->count--;
    pthread_mutex_unlock(&mutex);
    return data;                            //it returns the file descriptor
}


int network_accept_any(int* return_fd,int fds[],int count,struct sockaddr *addr,socklen_t *addrlen) {
    fd_set readfds;                         //this function will return ONE file descriptor (choosing from a set of file descriptors)
    int maxfd, fd;                          //it uses select() to find out to which descriptor (=socket) have been sent messages
    unsigned int i;
    int status;
    FD_ZERO(&readfds);
    maxfd = -1;
    for (i = 0;i<count;i++) {
        FD_SET(fds[i],&readfds);
        if (fds[i]>maxfd)
            maxfd=fds[i];
    }
    status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if (status < 0)
        return -5;
    fd = -5;
    for (i = 0; i < count; i++) {
        if (FD_ISSET(fds[i], &readfds)) {
            fd = fds[i];
            break;
        }
    }
    if (fd == -5) {
        return -5;
    }
    else {
        *return_fd=fd;
        return 1;
    }
}


int
send_all(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t n;
    const char *p = buffer;
    while (length > 0)
    {
        n = send(socket, p, length, flags);
        if (n <= 0)
            return -1;
        p += n;
        length -= n;
    }
    return 0;
}

void* thread_function() {
    while(1) {
        int connection_socket=obtain(&pool);
        printf("connection_socket is %d\n",connection_socket);
        char incoming_message[2048];
        recv(connection_socket,incoming_message,2048,0);                 //receiving message from client
//checking if HTTP request is valid..
        char* first_word_of_line[2];
        first_word_of_line[0]="GET";
        first_word_of_line[1]="Host:";
        char delim_new_line[2]="\n";
        char delim_space[2]=" ";
        char* line;
        char* first_word;
        char* lines[5];
        int line_num=0;
        line=strtok(incoming_message,delim_new_line);            //read lines of HTTP request
        if(line!=NULL) {
            lines[0]=malloc(strlen(line)+100);
            strcpy(lines[0],line);
        }
        else {
            break;
        }
        line=strtok(NULL,delim_new_line);
        if(line!=NULL) {
            lines[1]=malloc(strlen(line)+100);
            strcpy(lines[1],line);
        }
        else {
            printf("break\n");
            break;
        }
        line=strtok(NULL,delim_new_line);
        if(line!=NULL) {
            lines[2]=malloc(strlen(line)+100);
            strcpy(lines[2],line);
        }
        else
            break;
        line=strtok(NULL,delim_new_line);
        if(line!=NULL) {
            lines[3]=malloc(strlen(line)+100);
            strcpy(lines[3],line);
        }
        else
            break;
        line=strtok(NULL,delim_new_line);
        if(line!=NULL) {
            lines[4]=malloc(strlen(line)+100);
            strcpy(lines[4],line);
        }
        else
            break;
//now we have all the lines of the HTTP request stored in array lines[]
        first_word=strtok(lines[0],delim_new_line);                         //checking if first word is 'GET'
        sscanf(first_word,"%s",first_word);
        //printf("first_word=%s\n",first_word);
        if(strcmp(first_word,first_word_of_line[0])==0) {
            //printf("panooooos\n");
            line_num++;
        }
        first_word=strtok(lines[1],delim_new_line);                         //checking if there is the word 'Host'
        sscanf(first_word,"%s",first_word);
        //printf("first_word=%s\n",first_word);
        if(strcmp(first_word,first_word_of_line[1])==0)
            line_num++;
        first_word=strtok(lines[2],delim_new_line);
        sscanf(first_word,"%s",first_word);
        //printf("first_word=%s\n",first_word);
        if(strcmp(first_word,first_word_of_line[1])==0)
            line_num++;
        first_word=strtok(lines[3],delim_new_line);
        sscanf(first_word,"%s",first_word);
        //printf("first_word=%s\n",first_word);
        if(strcmp(first_word,first_word_of_line[1])==0)
            line_num++;
        first_word=strtok(lines[4],delim_new_line);
        sscanf(first_word,"%s",first_word);
        //printf("first_word=%s\n",first_word);
        if(strcmp(first_word,first_word_of_line[1])==0)
            line_num++;

        if(line_num!=2) {                     //it should be 2 because we identify an HTTP request ONLY from fields:GET,Host
            printf("line_num=%d\n",line_num );
            printf("the request has not the appropriate HTTP format\n");
            exit(1);
        }
        for(int r=0;r<line_num;r++) {
            free(lines[r]);
        }
        //printf("the request has the right format");
        //char* file_requested2;
        struct dirent* p_dirent;
        struct dirent* p_dirent2;
        DIR* dir;
        DIR* dir2;
        line=strtok(incoming_message,delim_new_line);
        char* file_requested2=strtok(line,delim_space);                //the file that the client wants
        file_requested2=strtok(NULL,delim_space);
        char* file_requested=malloc(strlen(file_requested2)+100);
        snprintf(file_requested,strlen(file_requested2)+100,"./%s%s",root_dir,file_requested2);
        dir=opendir(root_dir);
        if (dir==NULL) {
             printf("Cannot open directory '%s'\n",root_dir);
             exit(1);
         }
        int found=0;
        int exit_loop=0;
        while((p_dirent=readdir(dir)) != NULL) {                //LOOKING for the file inside root_dir
            //printf("checking exit loop\n");
            if(exit_loop==1) {
                    exit_loop=0;
                    //break;
                    continue;
            }
            if(strcmp(p_dirent->d_name,".")!=0 && strcmp(p_dirent->d_name,"..")!=0) {
                    char* cur_site=malloc(strlen(p_dirent->d_name)+100);
                    snprintf(cur_site,strlen(p_dirent->d_name)+100,"./%s/%s",root_dir,p_dirent->d_name);
                    //printf ("current site is: %s\n", cur_site);
                    dir2=opendir(cur_site);
                    if (dir2==NULL) {
                         printf ("Cannot open site file '%s'\n",cur_site);
                         break;
                    }
                    while((p_dirent2=readdir(dir2)) != NULL) {                //looking for the file inside every site of the root_dir
                        if(strcmp(p_dirent2->d_name,".")!=0 && strcmp(p_dirent2->d_name,"..")!=0) {
                                char* cur_file=malloc(strlen(p_dirent2->d_name)+100);
                                snprintf(cur_file,strlen(p_dirent2->d_name)+100,"%s/%s",cur_site,p_dirent2->d_name);
                                //printf ("current file is: %s\n", cur_file);
                                //printf("file_requested is:%s\n",file_requested);
                                if(strcmp(cur_file,file_requested)==0) {    //if file exists,then send an apprpriate reply to client
                                    printf("I HAVE FOUND THE PAGE\n");
                                    found=1;                     //file found
                                    FILE* fp=fopen(file_requested,"rwa");
                                    if(fp==NULL) {          //checking if I have permission for reading the file
                                        if(errno==EACCES) {
                                            printf("I DON'T HAVE PERMISSION FOR READING THE FILE\n");
                                            char cur_time[1000];
                                            time_t now=time(0);
                                            struct tm tm=*gmtime(&now);
                                            strftime(cur_time,sizeof(cur_time),"%a, %d %b %Y %H:%M:%S %Z",&tm);
                                            char* msg="<html>Trying to access this file but I do not think I can make it.</html>";
                                            int msg_len=strlen(msg);
                                            char http_reply[2048];
                                            snprintf(http_reply,2048,"HTTP/1.1 404 Not Found\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg_len,msg);
                                            send(connection_socket,http_reply,sizeof(http_reply),0);
                                            exit_loop=1;
                                            close(connection_socket);
                                            fclose(fp);
                                            continue;
                                        }
                                    }
                                    fseek(fp,0,SEEK_END);
                                    int file_size=ftell(fp);
                                    fseek(fp,0,SEEK_SET);
                                    printf("file size is %d bytes\n",file_size);
                                    char cur_time[1000];
                                    time_t now=time(0);
                                    struct tm tm=*gmtime(&now);
                                    strftime(cur_time,sizeof(cur_time),"%a, %d %b %Y %H:%M:%S %Z",&tm);
                                    //char* file_content=malloc(file_size+1000);
                                    //char file_content[file_size];
                                    char* file_content=malloc(file_size);
                                    int bytes_num=fread(file_content,sizeof(char),file_size,fp);
                                    file_content[bytes_num-1]='\0';
                                    //file_content[bytes_num-2]='\n';
                                    printf("file %s has size:%d\n",file_requested,file_size);
                                    char* http_reply=malloc(file_size+2048);
                                    snprintf(http_reply,file_size+2048,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n%s\n",cur_time,file_size,file_content);
                                    //send(connection_socket,http_reply,strlen(http_reply),0);
                                    int* total_bytes=malloc(sizeof(int));
                                    *total_bytes=strlen(http_reply);
                                    if(*total_bytes>1000000) {
                                        continue;
                                    }
                                    send(connection_socket,total_bytes,sizeof(int),0);
                                    http_reply[*total_bytes]='\0';
                                    send_all(connection_socket,http_reply,strlen(http_reply),0);
                                    pthread_mutex_lock(&mutex2);            //increasing total number of pages returned by server
                                    total_pages_num++;
                                    total_bytes_num=total_bytes_num+file_size;
                                    pthread_mutex_unlock(&mutex2);
                                    exit_loop=1;
                                    //free(http_reply);
                                    //free(file_content);
                                    fclose(fp);
                                    close(connection_socket);
                                    continue;
                                }
                        free(cur_file);
                        }
                    }
                    closedir(dir2);
            free(cur_site);
            }
        }
        free(file_requested);
        closedir(dir);
        printf("found===%d\n",found);
        if(found==0) {                 //if thread did not find the file,send back the appropriate HTTP reply
            char cur_time[1000];
            time_t now=time(0);
            struct tm tm=*gmtime(&now);
            strftime(cur_time,sizeof(cur_time),"%a, %d %b %Y %H:%M:%S %Z",&tm);
            //char http_reply2[2048];
            char* http_reply2=malloc(2048);
            char* msg="<html>Sorry dude, I could not find this file.</html>";
            int msg_len=strlen(msg);
            snprintf(http_reply2,2048,"HTTP/1.1 404 Not Found\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg_len,msg);
            send(connection_socket,http_reply2,strlen(http_reply2),0);
            free(http_reply2);
            close(connection_socket);
        }
    }
    printf("end\n");
    pthread_exit((void*)20);
}


int main(int argc,char* argv[]) {
    char* first_param="-p";                 //parameters
    char* second_param="-c";
    char* third_param="-t";
    char* fourth_param="-d";
    int serving_port,command_port,num_of_threads;
    if(argc!=9) {                                                    //checking if input from command line is correct
        printf("LESS ARGUMENTS GIVEN THAN EXPECTED\n");
        exit(1);
    }
    if(strcmp(argv[1],first_param)==0) {
        serving_port=atoi(argv[2]);
        if(strcmp(argv[3],second_param)==0) {
            command_port=atoi(argv[4]);
            if(strcmp(argv[5],third_param)==0) {
                num_of_threads=atoi(argv[6]);
                if(strcmp(argv[7],fourth_param)==0) {
                    root_dir=malloc(300);
                    strcpy(root_dir,argv[8]);
                }
                else {
                        printf("NOT CORRECT ARGUMENTS GIVEN FROM COMMAND LINE\n");
                        exit(1);
                }
            }
            else {
                    printf("NOT CORRECT ARGUMENTS GIVEN FROM COMMAND LINE\n");
                    exit(1);
            }
        }
        else {
                printf("NOT CORRECT ARGUMENTS GIVEN FROM COMMAND LINE\n");
                exit(1);
        }
    }
    else {
            printf("NOT CORRECT ARGUMENTS GIVEN FROM COMMAND LINE\n");
            exit(1);
    }

    DIR* dir=opendir(root_dir);
    if(!dir) {
        printf("DIRECTORY NAME DOES NOT EXIST\n");
        exit(1);
    }
   //serving_port,command_port,num_of_threads are now known
    //the work starts here..
    pthread_t threads[num_of_threads];
    pthread_mutex_init(&mutex,0);
    pthread_mutex_init(&mutex2,0);
    pthread_cond_init(&cond_nonempty,0);
    pthread_cond_init(&cond_nonfull,0);
    initialize(&pool);
    for(int i=0; i<num_of_threads;i++) {                //creation of threads
        pthread_create(&(threads[i]), 0, thread_function, (void*)i);
    }
    int server_socket;
    server_socket=socket(AF_INET,SOCK_STREAM,0);     //creating server socket (for requests)

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t addr_len=sizeof(client_address);
    server_address.sin_family=AF_INET;
    server_address.sin_port=htons(serving_port);
    server_address.sin_addr.s_addr=INADDR_ANY;//inet_addr("127.0.0.1");

    //tell kernel that I want to reuse server socket later
    int yes=1;
    if (setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1) {
        printf("error in reusing socket\n");
        exit(1);
    }

    int ret=bind(server_socket,(struct sockaddr*)&server_address,sizeof(server_address));
    if(ret<0) {
        printf("error in binding\n");
        exit(1);
    }
    printf("bind to port:%d\n",serving_port);
    if(listen(server_socket,1000)==0) {            //listening up to 100 clients simultaneously
        printf("Listening..\n");
    }
    else {
        printf("error in listening\n");
    }


    int server_socket2;
    server_socket2=socket(AF_INET,SOCK_STREAM,0);     //creating server socket 2 (for commands)
    struct sockaddr_in server_address2;
    server_address2.sin_family=AF_INET;
    server_address2.sin_port=htons(command_port);
    server_address2.sin_addr.s_addr=INADDR_ANY;//inet_addr("127.0.0.1");

    //tell kernel that I want to reuse server socket2 later
    int yes2=1;
    if (setsockopt(server_socket2,SOL_SOCKET,SO_REUSEADDR,&yes2,sizeof(yes2))==-1) {
        printf("error in reusing socket\n");
        exit(1);
    }

    int ret2=bind(server_socket2,(struct sockaddr*)&server_address2,sizeof(server_address2));
    if(ret2<0) {
        printf("error in binding\n");
        exit(1);
    }
    printf("bind to port:%d\n",command_port);
    if(listen(server_socket2,100)==0) {            //listening up to 100 clients simultaneously
        printf("Listening..\n");
    }
    else {
        printf("error in listening\n");
    }


    gettimeofday(&t1, NULL);
    int client_socket;
    int* return_fd=malloc(sizeof(int));
    int fds[2];
    fds[0]=server_socket;
    fds[1]=server_socket2;
    while(1) {
        //network_accept_any() uses select() to decide the apprpriate file descriptor (server_socket OR server_socket2)
        network_accept_any(return_fd,fds,2,(struct sockaddr*)&client_address,&addr_len);   //it returns the file descriptor that has a message to read
        if(*return_fd==fds[1]) {
            client_socket=accept(*return_fd,(struct sockaddr*)&client_address,&addr_len);        //waiting to accept a connection from client
            char command[2048];
            recv(client_socket,command,2048,0);
            sscanf(command, "%s",command);
            //printf("command=%s\n",command);
            if(strcmp(command,"STATS")==0) {
                gettimeofday(&t2, NULL);
                time_passed=(t2.tv_sec-t1.tv_sec)*1000.0;
                char cur_time[1000];
                time_t now=time(0);
                struct tm tm=*gmtime(&now);
                strftime(cur_time,sizeof(cur_time),"%a, %d %b %Y %H:%M:%S %Z",&tm);
                char http_reply[2048];
                char msg[2048];
                snprintf(msg,sizeof(msg),"the server is running for %f milliseconds..",time_passed);
                int msg_len=sizeof(msg);
                //printf("msg=%s\n",msg);
                snprintf(http_reply,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg_len,msg);
                send(client_socket,http_reply,300,0);
                char http_reply2[2048];
                char msg2[2048];
                snprintf(msg2,sizeof(msg2),"total number of page requests served with success is %d",total_pages_num);
                int msg2_len=sizeof(msg2);
                //printf("msg2=%s\n",msg2);
                snprintf(http_reply2,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg2_len,msg2);
                send(client_socket,http_reply2,300,0);
                char http_reply3[2048];
                char msg3[2048];
                snprintf(msg3,sizeof(msg3),"total number of bytes from pages is %d",total_bytes_num);
                int msg3_len=sizeof(msg3);
                //printf("msg3=%s\n",msg3);
                snprintf(http_reply3,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg3_len,msg3);
                send(client_socket,http_reply3,300,0);
            }
            if(strcmp(command,"SHUTDOWN")==0) {
                exit(0);
            }
            close(client_socket);
        }
        else if(*return_fd==fds[0]){
            client_socket=accept(*return_fd,(struct sockaddr*)&client_address,&addr_len);        //waiting to accept a connection from client
            place(&pool,client_socket);
            pthread_cond_broadcast(&cond_nonempty);        //send signal to wake some thread to do the job
        }
    }


    exit(0);
}
