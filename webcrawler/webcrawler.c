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
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char* save_dir;                  //some globals (threads use them)
char* host_or_IP;
int port;
char* port_in_ascii;
int total_pages_num=0;
int total_bytes_num=0;
struct timeval t1,t2;
double time_passed;


pthread_mutex_t mutex;           //for pooling (shared buffer where threads collect the URLS if exist)
pthread_mutex_t mutex2;           //for protecting STATS variables (total_pages_num,total_bytes_num)
pthread_mutex_t mutex3;         //for ensuring that there will be only one writer (writing to file) simultaneously

pthread_cond_t cond_nonempty;                 //condition variables for full pool and empty pool
pthread_cond_t cond_nonfull;
#define POOL_SIZE 10000
#define MAX_FILE_SIZE 1000000                //file size will not exceed that number
#define MAX_PATHNAME_LEN 400
typedef struct {                                    //pool struct contains the urls that threads obtain
    char* urls[POOL_SIZE];
    int start;
    int end;
    int count;                          //how many urls are in the pool simultaneously
} pool_t;

pool_t pool;

void initialize(pool_t * pool) {            //initialize pool
    pool->start = 0;
    pool->end = -1;
    pool->count = 0;
}

void place(pool_t * pool, char* data) {              //place an url in the pool
    pthread_mutex_lock(&mutex);
    while (pool->count >= POOL_SIZE) {
        //printf(">> Found Buffer Full \n");
        pthread_cond_wait(&cond_nonfull, &mutex);
    }
    pool->end = (pool->end + 1) % POOL_SIZE;
    pool->urls[pool->end]=malloc(400);
    strcpy(pool->urls[pool->end],data);
    pool->count++;
    pthread_mutex_unlock(&mutex);
}

char* obtain(pool_t * pool) {                           //obtain an url from the pool
    pthread_mutex_lock(&mutex);
    while (pool->count <= 0) {
        //printf(">> Found Buffer Empty \n");
        pthread_cond_wait(&cond_nonempty, &mutex);
    }
    char* data=malloc(400);
    strcpy(data,pool->urls[pool->start]);
    pool->start = (pool->start + 1) % POOL_SIZE;
    pool->count--;
    pthread_mutex_unlock(&mutex);
    return data;
}



int valid_digit(char *ip_str) {                       //checks if string is an INTEGER
    while (*ip_str) {
        if (*ip_str >= '0' && *ip_str <= '9')
            ++ip_str;
        else
            return 0;
    }
    return 1;
}


int is_valid_ip(char *ip_str) {                     // return 1 if IP string is valid, else return 0
    int i, num, dots = 0;
    char *ptr;
    if (ip_str == NULL)
        return 0;
    ptr = strtok(ip_str,".");
    if (ptr == NULL)
        return 0;
    while (ptr) {
        if (!valid_digit(ptr))
            return 0;
        num = atoi(ptr);
        if (num >= 0 && num <= 255) {
            ptr = strtok(NULL,".");
            if (ptr != NULL)
                ++dots;
        }
        else
            return 0;
    }

    /* valid IP string must contain 3 dots */
    if (dots != 3)
        return 0;
    return 1;
}

int isDirectoryEmpty(char *dirname) {                  //checks if directory is empty
  int n = 0;
  struct dirent *d;
  DIR *dir = opendir(dirname);
  if (dir == NULL) //Not a directory or doesn't exist
    return 1;
  while ((d = readdir(dir)) != NULL) {
    if(++n > 2)
      break;
  }
  closedir(dir);
  if (n <= 2) //Directory Empty
    return 1;
  else
    return 0;
}



int remove_directory(const char *path) {                   //remove a directory and its containts
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;
   if (d) {
      struct dirent *p;
      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
             continue;
          }
          len = path_len + strlen(p->d_name) + 2;
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) {
                   r2 = remove_directory(buf);
                }
                else{
                   r2 = unlink(buf);
                }
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }
   if (!r) {
      r = rmdir(path);
   }
   return r;
}


int FindWord(char *word , char *file,char** lines){       //it fills the array lines[] with the lines of the file that contain the word given
   char line[1024] ;
   int i=-1;
   FILE* fp = fopen(file, "r") ;
   while(fgets(line,sizeof(line),fp)!=NULL) {
      if(strstr(line,word)!=NULL) {
          i++;
          lines[i]=malloc(500);
          strcpy(lines[i],line);
         //printf("lines[i]=%s",lines[i]);
      }
   }
   return i+1;                     //return the number of lines that contain the word
}

int network_accept_any(int* return_fd,int fds[],int count,struct sockaddr *addr,socklen_t *addrlen) {
    fd_set readfds;                                     //this function will return ONE file descriptor (choosing from a set of file descriptors)
    int maxfd, fd;
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
        *return_fd=-100;              //some negative (it is not a valid file descriptor)
        return -5;
    }
    else {
        *return_fd=fd;
        //printf("oxiiiiiii\n");
        return 1;
    }
}

int
recv_all(int socket, const void *buffer, size_t length, int flags) { //continiously receiving data into buffer
    ssize_t n;                                                         //until we reach the desired size
    const char *p = buffer;
    while (length > 0)
    {
        n = recv(socket, p, length, flags);
        if (n <= 0)
            return -1;
        p += n;
        length -= n;
    }
    return 0;
}


void* thread_function(void* argp) {
    //printf("Starting thread %d\n",(int)argp);
    while(1) {
        char* url=malloc(400);
        strcpy(url,obtain(&pool));                          //take some url from the pool
        int client_socket;
        client_socket=socket(AF_INET,SOCK_STREAM,0);     //creating client socket
        int yes=1;
        if (setsockopt(client_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1) {
            printf("error in reusing socket\n");
            exit(1);
        }
        struct sockaddr_in server_address;
        server_address.sin_family=AF_INET;
        server_address.sin_port=htons(port);
        //printf("host_or_IP=%s\n",host_or_IP);
        server_address.sin_addr.s_addr=inet_addr(host_or_IP);
        int ret=connect(client_socket,(struct sockaddr*)&server_address,sizeof(server_address));      //try to connect to server
        if(ret<0) {
            printf("error in connection to server\n");
            close(client_socket);
            exit(1);
        }
        char http_request[2048];
        //char incoming_message[MAX_FILE_SIZE];
        char* incoming_message=malloc(MAX_FILE_SIZE);
        char* host=malloc(200);

        char file_requested[400];                       //extract file from url (px /sitex/pagex/..html)
        char delim[2]="/";
        char delim2[2]=":";
        char delim3[2]="/";
        char delim4[2]="\0";
        char* initial_url=malloc(200);
        strcpy(initial_url,url);
        //sscanf(initial_url,"%s",initial_url);
        //printf("initial_url=%s\n",initial_url);
        char* url2;
        url2=strtok(initial_url,delim);
        url2=strtok(NULL,delim2);
        url2=strtok(NULL,delim3);
        url2=strtok(NULL,delim4);
    //    sscanf(url2,"%s",url2);
        //printf("url2=%s\n",url2);
        strcpy(file_requested,"/");
        strcat(file_requested,url2);
        //sscanf(file_requested,"%s",file_requested);
        //printf("file=%s\n",file_requested);
        sprintf(http_request,"GET %s HTTP/1.1\nUser-Agent: crawler\nHost: %s\nAccept-Language: en-us\nAccept-Encoding: gzip, deflate\nConnection: Closed\n\n\r",file_requested,host_or_IP);
        //printf("message sent=%s\n",http_request);
        send(client_socket,http_request,strlen(http_request),0);      //send request for file
        //printf("receiving from server..\n");
        int* total_bytes=malloc(sizeof(int));
        recv(client_socket,total_bytes,sizeof(int),0);             //receiving file from server
        printf("total_bytes are %d\n",*total_bytes);
        recv_all(client_socket,incoming_message,*total_bytes,0);
        incoming_message[*total_bytes]='\0';
        if(strlen(incoming_message)==0) {
            close(client_socket);
            continue;
        }
        if(strcmp(incoming_message,"<html>Sorry dude, I could not find this file.</html>")==0) {
            printf("the server does not have the requested starting url\n");
            free(url);
            free(incoming_message);
            close(client_socket);
            continue;
        }
        else if(strcmp(incoming_message,"<html>Trying to access this file but I do not think I can make it.</html>")==0) {
            printf("the server does not have permission for the requested starting url\n");
            free(url);
            free(incoming_message);
            close(client_socket);
            continue;
        }

        char file_path[MAX_PATHNAME_LEN];
        sprintf(file_path,"%s%s",save_dir,file_requested);
        if(access(file_path,F_OK) != -1 ) {                                   //if file already exists, continue
            printf("file %s exists\n",file_path);
            free(incoming_message);
            free(url);
            close(client_socket);
            continue;
        }                                                                       //else
        printf("file %s NOT exists\n",file_path);

        char* site_name;                            //create 'site' directory inside save_dir (pc /save_dir/site1)
        site_name=strtok(file_requested,delim3);
        char* site_path=malloc(MAX_PATHNAME_LEN);
        sprintf(site_path,"%s/%s",save_dir,site_name);
        mkdir(site_path,0777);
        //printf("site_name=%s\n",site_name);
        //printf("file path=%s\n",file_path);
        int fd;
        if((fd=open(file_path,O_RDWR | O_CREAT,0777))==-1) {              //create the html file inside /save_dir/sitex/
                printf("can not create file inside save_dir\n");
                free(url);
                free(incoming_message);
                close(client_socket);
                continue;
        }
                                        //now we have to PUT content to the file
        char delim5[2]="\n";
        char delim6[2]="\0";
        char* file_contents;
        file_contents=strtok(incoming_message,delim5);
        file_contents=strtok(NULL,delim5);
        file_contents=strtok(NULL,delim5);
        file_contents=strtok(NULL,delim5);
        file_contents=strtok(NULL,delim5);
        file_contents=strtok(NULL,delim5);
        file_contents=strtok(NULL,delim6);
        //printf("file_contents=%s\n",file_contents);
        pthread_mutex_lock(&mutex3);
        write(fd,file_contents,strlen(file_contents));          //write to the new file the contents of the original html file that server gave us
        pthread_mutex_unlock(&mutex3);

        pthread_mutex_lock(&mutex2);
        total_pages_num++;                                   //increasing total number of pages returned by server
        total_bytes_num=total_bytes_num+strlen(file_contents);          //and total number of bytes
        pthread_mutex_unlock(&mutex2);
        char str[100];
        char** lines;                                   //find all the URLS in the file and EXTRACT THEM
        lines=malloc(1000*sizeof(char*));
        int num_of_lines=FindWord("href",file_path,lines);  // num_of_lines=number of lines in file that contain the word "href"
        char delim7[2]="=";
        char delim8[2]=">";
        char delim9[2]="..";
        char* real_url=malloc(500);
        char* real_url2=malloc(500);
//array lines[] contains ALL THE URLS of the file
        for(int j=0;j<num_of_lines;j++) {                //vazw ola ta urls tou arxeiou mesa sto koino pool
            real_url2=strtok(lines[j],delim7);
            real_url2=strtok(NULL,delim8);
            real_url=strtok(real_url2,"..");               //extracting ".." from "../sitex/pagex.html"
            strcat(real_url,".html");
            char* complete_url=malloc(sizeof(port)+strlen(host_or_IP)+strlen(real_url)+100);    //constructing complete url(px http://linux01.di.uoa.gr:8080/site1/page0_1234.html)
            strcpy(complete_url,"http://");
            strcat(complete_url,host_or_IP);
            strcat(complete_url,":");
            strcat(complete_url,port_in_ascii);                  //argv[4]=port (in ascii form)
            strcat(complete_url,real_url);
            place(&pool,complete_url);                    //PLACE each url in the POOL (another thread will obtain it)
            free(complete_url);
        }
        for(int j=0;j<num_of_lines;j++) {
                free(lines[j]);
        }
        free(url);
        free(incoming_message);
        free(lines);
        close(client_socket);
        pthread_cond_broadcast(&cond_nonempty);             //SIGNAL the other threads that there are urls in the pool
    }
}

int main(int argc,char* argv[]) {
    char* first_param="-h";                 //parameters
    char* second_param="-p";
    char* third_param="-c";
    char* fourth_param="-t";
    char* fifth_param="-d";
    char* starting_URL;
    int command_port,num_of_threads;

    if(argc!=12) {                                                    //checking if input from command line is correct
        printf("LESS ARGUMENTS GIVEN THAN EXPECTED\n");
        exit(1);
    }

    if(strcmp(argv[1],first_param)==0) {
        host_or_IP=malloc(strlen(argv[2])+100);
        strcpy(host_or_IP,argv[2]);
        if(strcmp(argv[3],second_param)==0) {
            port=atoi(argv[4]);
            port_in_ascii=malloc(100);
            strcpy(port_in_ascii,argv[4]);
            if(strcmp(argv[5],third_param)==0) {
                command_port=atoi(argv[6]);
                if(strcmp(argv[7],fourth_param)==0) {
                    num_of_threads=atoi(argv[8]);
                    if(strcmp(argv[9],fifth_param)==0) {
                        save_dir=malloc(300);
                        strcpy(save_dir,argv[10]);
                        DIR* dir = opendir(save_dir);
                        if (dir) {
                            printf("directory exists\n");
                            if(isDirectoryEmpty(save_dir)==0) {
                                printf("directory exists but it is not empty\n");
                                remove_directory(save_dir);
                                mkdir(save_dir,0777);
                            }
                            closedir(dir);
                        }
                        else if (ENOENT == errno) {
                            printf("Directory does not exist,create one\n");
                            mkdir(save_dir,0777);
                        }
                        else {
                            printf("can not open directory\n");
                        }
                        starting_URL=malloc(300);
                        strcpy(starting_URL,argv[11]);
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
    }
    else {
            printf("NOT CORRECT ARGUMENTS GIVEN FROM COMMAND LINE\n");
            exit(1);
    }
    pthread_mutex_init(&mutex,0);                           //initialize mutexes and condition variables
    pthread_mutex_init(&mutex2,0);
    pthread_mutex_init(&mutex3,0);
    pthread_cond_init(&cond_nonempty,0);
    pthread_cond_init(&cond_nonfull,0);
    pthread_t threads[num_of_threads];
    for(int i=0; i<num_of_threads;i++) {                            //creation of threads
        pthread_create(&(threads[i]), 0, thread_function, (void*)i);
    }
    initialize(&pool);          //initialize pool struct


char* host_or_IP2=malloc(400);
strcpy(host_or_IP2,host_or_IP);

int is_ip=is_valid_ip(host_or_IP2);                ////checking if it is an IP or domain name
if(is_ip==0) {                           //it is not an IP address (convert it to IP address)
    printf("it is an IP \n");
    struct in_addr ip;
    struct hostent *hp=malloc(sizeof(struct hostent));
    if ((hp = gethostbyname(host_or_IP)) == NULL) {
        printf("no ip address associated with %s",host_or_IP);
        exit(1);
    }
    printf("ip=%s\n",hp->h_addr_list[0]);
    char buff[200];
    inet_ntop(AF_INET,hp->h_addr_list[0],buff,sizeof(buff));
    strcpy(host_or_IP,buff);               //host_or_IP now contains the IP ADDRESS
}

char* complete_url=malloc(500);    //constructing complete starting url(px http://linux01.di.uoa.gr:8080/site1/page0_1234.html)
strcpy(complete_url,"http://");
strcat(complete_url,host_or_IP);
strcat(complete_url,":");
strcat(complete_url,argv[4]);                  //argv[4]=port (in ascii form)
strcat(complete_url,starting_URL);
place(&pool,complete_url);                    //vazw to arxiko url sto pool
sleep(1);
pthread_cond_signal(&cond_nonempty);        //send signal to wake some thread to take the url from the pool



////////////////////////////////////////////////////////////////////////////////////for commands

int command_socket;
command_socket=socket(AF_INET,SOCK_STREAM,0);                  //creating command_socket
struct sockaddr_in command_address;
command_address.sin_family=AF_INET;
command_address.sin_port=htons(command_port);
command_address.sin_addr.s_addr=INADDR_ANY;
int yes=1;
if (setsockopt(command_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1) {
    printf("error in reusing socket\n");
    exit(1);
}
int ret=bind(command_socket,(struct sockaddr*)&command_address,sizeof(command_address));
if(ret<0) {
    printf("error in binding\n");
    exit(1);
}
printf("bind to port:%d\n",command_port);
if(listen(command_socket,100)==0) {            //listening up to 100 clients simultaneously
    printf("Listening..\n");
}
else {
    printf("error in listening\n");
}

gettimeofday(&t1, NULL);
int client_socket;
struct sockaddr_in client_address;
socklen_t addr_len=sizeof(client_address);
int* return_fd=malloc(sizeof(int));
*return_fd=-100;
int fds[1];
fds[0]=command_socket;
while(1) {
    network_accept_any(return_fd,fds,2,(struct sockaddr*)&client_address,&addr_len);
    if(*return_fd==fds[0] && (*return_fd)>=0) {
        client_socket=accept(*return_fd,(struct sockaddr*)&client_address,&addr_len);        //waiting to accept a connection from client
        char command[2048];
        recv(client_socket,command,2048,0);
        sscanf(command, "%s",command);
        if(strcmp(command,"SHUTDOWN")==0) {
            close(client_socket);
            exit(0);
        }
        else if(strcmp(command,"STATS")==0) {
            gettimeofday(&t2, NULL);
            time_passed=(t2.tv_sec-t1.tv_sec)*1000.0;
            char cur_time[1000];                                         //constructing current DATE in apprpriate format
            time_t now=time(0);
            struct tm tm=*gmtime(&now);
            strftime(cur_time,sizeof(cur_time),"%a, %d %b %Y %H:%M:%S %Z",&tm);
            char http_reply[2048];
            char msg[2048];
            snprintf(msg,sizeof(msg),"the server is running for %f milliseconds..",time_passed);
            int msg_len=sizeof(msg);
            printf("msg=%s\n",msg);
            snprintf(http_reply,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg_len,msg);
            send(client_socket,http_reply,300,0);
            char http_reply2[2048];
            char msg2[2048];
            snprintf(msg2,sizeof(msg2),"total number of page requests served with success is %d",total_pages_num);
            int msg2_len=sizeof(msg2);
            printf("msg2=%s\n",msg2);
            snprintf(http_reply2,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg2_len,msg2);
            send(client_socket,http_reply2,300,0);
            char http_reply3[2048];
            char msg3[2048];
            snprintf(msg3,sizeof(msg3),"total number of bytes from pages is %d",total_bytes_num);
            int msg3_len=sizeof(msg3);
            printf("msg3=%s\n",msg3);
            snprintf(http_reply3,300,"HTTP/1.1 200 OK\nDate: %s GMT\nServer: myhttpd/1.0.0 (Ubuntu64)\nContent-Length: %d\nContent-Type: text/html\nConnection: Closed\n\n%s\r\n\n",cur_time,msg3_len,msg3);
            send(client_socket,http_reply3,300,0);
        }
        close(client_socket);
    }
}
exit(0);
}
