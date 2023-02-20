#include <stdio.h>
#include "csapp.h"



/* Recommended max cache sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


/*about cache*/


typedef struct cache_item{
   char obj[MAX_OBJECT_SIZE];
   char host[MAXLINE];
   char path[MAXLINE];
   int port;

   int LRU;
   size_t load;
   struct cache_item* next;
   struct cache_item* prev;   
}cache_item_t;

typedef struct {
    cache_item_t* head;
    cache_item_t* tail;
    size_t load;
    int cache_cnt;
    sem_t mutex,w;
    sem_t LRU_mutex;
    int LRU_NUMBER;
    int readcnt;
}cache_t;

cache_t cash;

void cash_init(){
    cash.head=NULL;
    cash.tail=NULL;
    cash.head=0;
    cash.tail=0;
    cash.cache_cnt=0;
    cash.LRU_NUMBER=1;
    cash.readcnt=0;

    Sem_init(&cash.mutex,0,1);
    Sem_init(&cash.w,0,1);
    Sem_init(&cash.LRU_mutex,0,1);
}

cache_item_t* new_cache(char* host, char* path, int port, char* obj, size_t size){
    cache_item_t* ret=Malloc(sizeof(cache_item_t));
    P(&cash.LRU_mutex);
    cash.LRU_NUMBER++;
    ret->LRU=cash.LRU_NUMBER;
    V(&cash.LRU_mutex);

    strcpy(ret->host,host);
    strcpy(ret->path,path);
    ret->port=port;
    strcpy(ret->obj,obj);
    ret->load=size;
    ret->next=NULL;
    ret->prev=NULL;
    return ret;
}


void insert(cache_item_t* node){
    if (cash.cache_cnt == 0) {
		cash.head = node;
		cash.tail = node;
		node->next = NULL;
		node->prev = NULL;
	} else {
		cash.tail->next = node;
		node->prev = cash.tail;
		node->next = NULL;
		cash.tail = node;
	}
	cash.load += node->load;
	cash.cache_cnt++;
}

void delete(){
    if(cash.cache_cnt==0){
        return;
    }
    else if(cash.cache_cnt==1){
        cash.cache_cnt=0;
        cash.head=NULL;
        cash.tail=NULL;
        return;
    }
    cache_item_t* deleteNode;

    cache_item_t* p=cash.head;
    deleteNode=p;
    while(p){
        /*big lRu means it is latest*/

        if((deleteNode->LRU)>(p->LRU)){
            deleteNode=p;
        }
        p=p->next;
    }
    if(deleteNode==cash.head){
        cash.head=cash.head->next;
        cash.head->prev=NULL;
    }
    else if(deleteNode==cash.tail){
        cash.tail=cash.tail->prev;
        cash.tail->next=NULL;
    }
    else{
        deleteNode->prev->next=deleteNode->next;
        deleteNode->next->prev=deleteNode->prev;
    }
    free(deleteNode);
}

int cache_check(cache_item_t *this, char *host, char* path, int port) {
	if (strcasecmp(this->host, host)||this->port!=port||strcasecmp(this->path,path)){
        return 0;
    }
		
	else{
        return 1;
    }
	
}

cache_item_t* Find_cache(char* host,char* path, int port){
    cache_item_t* ret=cash.head;
    
    while(ret){
        if(cache_check(ret,host,path,port)){
            return ret;
        }
        ret=ret->next;
    }
    return NULL;
}





/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
void acting(int fd);
int parse_uri_proxy(char *uri, char *hostname, char *path);
void Make_Head(char* header, char* hostname, char *path, int * port,  rio_t* rio);
int check_key(char * string, const char * key);
void* thread_func(void* connfd);

int main(int argc, char** argv)
{

    Signal(SIGPIPE,SIG_IGN);
    cash_init();
    int listenfd;
    int connfd;
   

    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    

    if(argc!=2){
        exit(1);
    }
    pthread_t tid;
    listenfd=Open_listenfd(argv[1]);
    while(1){
        clientlen=sizeof(clientaddr);
        connfd=Accept(listenfd,(struct sockaddr*)(&clientaddr),&clientlen );
      

        Pthread_create(&tid,NULL,thread_func,(void*)connfd);
    }

    printf("%s", user_agent_hdr);

   
    return 0;
}

void* thread_func(void* connfd){
        int fd=(int)(connfd);
        Pthread_detach(pthread_self());
        acting(fd);
        Close(fd);
        return NULL;
}

void acting(int fd){
    int connfd=fd;
    int i;
    rio_t rio;
    
    char buf[MAXLINE];

    
    Rio_readinitb(&rio,connfd);
    Rio_readlineb(&rio,buf,MAXLINE);

    char Method[MAXLINE];
    char Version[MAXLINE];
    char URI[MAXLINE];

    char hostname[MAXLINE];
    char path[MAXLINE];
    
    int port;
    


    /*buf => method  uri version */
    sscanf(buf,"%s %s %s",Method,URI,Version);

    if(strcmp(Method,"GET")){
        /*we implement only get method*/
        printf("this is not GET METHOD\n");
        return;
    }
    




    
    port=parse_uri_proxy(URI,hostname,path);
    /*we get real_server information*/
    


int find_flag=0;
cache_item_t* node;

    P(&cash.mutex);
        cash.readcnt++;
        if (cash.readcnt == 1){
            P(&cash.w);
        }      
    V(&cash.mutex);

        if ((node = Find_cache(hostname, path, port)) != NULL) {  
            Rio_writen(connfd, node->obj, node->load);
            P(&cash.LRU_mutex);
            cash.LRU_NUMBER++;
            V(&cash.LRU_mutex);
            node->LRU=cash.LRU_NUMBER;
            find_flag = 1;
        }        


    P(&cash.mutex);
        cash.readcnt--;
        if (cash.readcnt == 0){
        V(&cash.w);
        } 
    V(&cash.mutex);
       
        if (find_flag == 1) {
            return;
        }
        else{

        }



    char head[MAXLINE];
    Make_Head(head,hostname,path,&port,&rio);
    


    char port_string[10];
    sprintf(port_string,"%d",port);
    int real_server_fd=Open_clientfd(hostname,port_string);


    rio_t server_rio;
    Rio_readinitb(&server_rio,real_server_fd);
    Rio_writen(real_server_fd,head,strlen(head));

    char cache_buf[MAX_OBJECT_SIZE];
    int size_buf=0;

    while((i=Rio_readlineb(&server_rio,buf,MAXLINE))>0){
        size_buf+=i;
        if(size_buf<MAX_OBJECT_SIZE){
            strcat(cache_buf,buf);
        }

        Rio_writen(connfd,buf,i);
    }


    if(size_buf<MAX_OBJECT_SIZE){
        cache_item_t* node=new_cache(hostname,path,port,cache_buf,size_buf);
        P(&cash.w);

         while((cash.load+size_buf)>MAX_CACHE_SIZE){
            delete();
         }   
         insert(node);

        V(&cash.w);
    }
    Close(real_server_fd);    
}

int parse_uri_proxy(char *uri, char *hostname, char *path) 
{
    int port_num=80;
    char* url_pos= strstr(uri, "//");
    char* port_pos;
    if(url_pos==NULL){
        url_pos=uri;
    }
    else{
        url_pos=url_pos+2;
    }
    port_pos=strstr(url_pos,":");

    if(port_pos==NULL){
        /*there is no port number*/
        char* path_pos=strstr(url_pos,"/");
        *path_pos='\0';
        sscanf(url_pos,"%s",hostname);
        sscanf(path_pos+1,"/%s",path);
        
        
        /*80is default port number*/ 

    }
    else{
        *port_pos='\0';/* null character*/
        sscanf(url_pos,"%s",hostname);
        sscanf(port_pos+1,"%d%s",&port_num,path);
    }

    return port_num;
}
/*this is header format*/
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_line_format = "Host: %s\r\n";
static const char *EOF_h = "\r\n";
static const char *request_format="GET %s HTTP/1.0\r\n";
/*header format*/

/*key*/
static const char *host_key = "Host";
static const char *proxy_key = "Proxy-C";
static const char *connection_key = "Connec";
static const char *user_key= "User-Ag";
/*key end*/


void Make_Head(char* header, char* hostname, char *path, int * port,  rio_t* rio){
    char buf[MAXLINE];
    char request_line[MAXLINE],host_line[MAXLINE];

    char others[MAXLINE];
    sprintf(request_line,request_format,path);
    int i;
    while((i=Rio_readlineb(rio,buf,MAXLINE))>0){
        if(strcmp(buf,EOF_h)==0){
            break;
        }
        if(check_key(buf,host_key)){
            strcpy(host_line,buf);
        }
        else if(!check_key(buf,connection_key)&&!check_key(buf,proxy_key)&&!check_key(buf,user_key)){
            strcat(others,buf);
        }
    }

    if(strlen(host_line)==0){
        sprintf(host_line,host_line_format,hostname);
    }
    sprintf(header,"%s%s%s%s%s%s%s", request_line,host_line,user_agent_hdr,
        conn_hdr,prox_hdr,others,EOF_h
    );
}

int check_key(char * string, const char * key){
    if(!strncasecmp(string,key,strlen(key))){
        return 1;
    }  
    else{
        return 0;
    }
}