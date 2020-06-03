#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
typedef struct {
    char method[MAXLINE];
    char hostname[MAXLINE];
    char version[MAXLINE];
    char port[MAXLINE];
    char query[MAXLINE];
    char header[MAXLINE];
}Request;

typedef struct {
    char* obj;
    char name[MAXLINE];
    size_t size;
    struct CachedItem* next;
}CachedItem; 

typedef struct {
    size_t totalsize;
    struct CachedItem* start;
}CacheList;

struct CacheList* clist=NULL;
struct CacheItem* nw = NULL;
struct CacheItem* adder = NULL;

void handle_client(void* vargp);
void initialize_struct(Request* req);
void parse_request(char request[MAXLINE], Request* req);
int get_from_cache(char request[MAXLINE], int clientfd);
void get_from_server(char request[MAXLINE], int serverfd, int clientfd);
void print_struct(Request* req);

void init_cache(CacheList* list);
//void cache_URL(CachedItem* cach, CacheList* list);
void evict(CacheList* list);
struct CachedItem* find(CachedItem cach, CacheList* list);
void move_to_front(char* URL, CacheList* list);
void cache_destruct(CacheList* list);

 
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: proxy <port>\n\nwhere <port> is listening port number between 4500 and 65000\n");
        return 0;
    }
    printf("%s", user_agent_hdr);
    init_cache(clist);

    int listenfd, connfd, clientlen;
    struct sockaddr_in clientaddr;
    char haddrp[MAXLINE];;
    char client_port[MAXLINE];
    listenfd = Open_listenfd(argv[1]); 

    while (1) {
        clientlen = sizeof(clientaddr);

        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); //connect
        Getnameinfo((SA*)&clientaddr, clientlen, haddrp, MAXLINE, client_port, MAXLINE, 0);
        
        pthread_t tid;
        Pthread_create(&tid, NULL, (void*)handle_client, &connfd);
       
    }
    return 0;
}

void handle_client(void* vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self());
    int check = 0;
    char buf[MAXLINE], request[MAXLINE];
    rio_t rio;
    size_t n;
    int clientfd;

    Rio_readinitb(&rio, connfd);

    Request* rq = Malloc(sizeof(Request));
    initialize_struct(rq);
    char header[MAXLINE];
    buf[0] = 0;
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0){
        if (strncmp(buf, "\r\n", 2) == 0) break;
        if (check == 0) {
            char name[MAXLINE];
            char ver[MAXLINE];
            sscanf(buf, "%s %s %s", req->method, name, ver);

            if (clist != NULL) {
                nw = find(name, clist);
                if (nw != NULL) {
                    move_to_front(nw, clist);
                }
                else adder->name = name;
            }

            parse_request(name, rq);
            sprintf(header, "Host: %s:%s\r\n",rq->hostname, rq->port);
            sprintf(header, "%s%s", header, user_agent_hdr);
            sprintf(header, "%s%s%s", header, "Connection: close\r\n", "Proxy-Connection: close\r\n");
        }
        else if (strncmp(buf, "Proxy-Connection:", 17) && strncmp(buf, "Host:", 5) && strncmp(buf, "User-Agent:", 11)) {
            sprintf(header, "%s%s", header, buf);
        }
        check++;
    }

    sprintf(request, "%s %s %s\r\n", rq->method, rq->query, rq->version);
    sprintf(request, "%s%s", request, header);

    clientfd = Open_clientfd(rq->hostname, rq->port);

    get_from_server(request, clientfd, connfd);
    Close(clientfd);
    Close(connfd);
}

void parse_request(char request[MAXLINE], Request* req) {

    char* tmp1 = strstr(request, "//");
    char* tmp2 = strstr(tmp1, ":");
    char* tmp3;
    if (tmp2 != NULL) tmp3 = strstr(tmp2, "/");
    else tmp3 = strstr(tmp1+2, "/");
    if (tmp2!=NULL) {
        *tmp2 = '\0';
        sscanf(tmp1 + 2, "%s", req->hostname);
        sscanf(tmp3, "%s", req->query);
        *tmp3 = '\0';
        sscanf(tmp2 + 1, "%s", req->port);
        *tmp3 = '/';
    }
    else {
        sscanf(tmp3, "%s", req->query);
        *tmp3 = '\0';
        sscanf(tmp1 + 2, "%s", req->hostname);
        sscanf("80", "%s", req->port);
        *tmp3 = '/';
    }
    sscanf("HTTP/1.0", "%s", req->version);
}


void initialize_struct(Request* req) {
    for (int i = 0; i < MAXLINE; i++) {
        req->method[i] = '\0';
        req->hostname[i] = '\0';
        req->port[i] = '\0';
        req->query[i] = '\0';
        req->version[i] = '\0';
        req->header[i] = '\0';
    }
}

void get_from_server(char request[MAXLINE], int serverfd, int clientfd) {
    char response[MAXLINE];
    size_t n;
    rio_t rio;
    if (nw == NULL) {
        Rio_readinitb(&rio, serverfd);
        int first = 1;
        adder->size = 0;
        Rio_writen(serverfd, request, strlen(request));
        while ((n = Rio_readnb(&rio, response, MAXLINE)) > 0) {
            Rio_writen(clientfd, response, n);
            adder->size += n;
            if (first) sprintf(adder->obj, "%s", response);
            else sprintf(adder->obj, "%s%s", adder->obj, response);
        }
        if (clist->totalsize + adder->size <= MAX_CACHE_SIZE) {
            cache_destruct(clist);
        }
        adder->next = clist->start->next;
        clist->start->next = adder;
        clist->totalsize += adder->size;
        adder = NULL;
    }
    else {
        Rio_writen(clientfd, nw->obj, nw->size);
        nw = NULL;
    }
}

void init_cache(CacheList* list) {
    list->start->obj = NULL;
    list->start->size = 0;
    list->start->name[0] = '\0';
    list->start->next = NULL;
    list->totalsize = 0;
}

struct CachedItem* find(char* URL, CacheList* list) {
    struct CacheItem* res = list->start->next;
    while (res != NULL) {
        if (!strcmp(res->name, URL)) return res;
        res = res->next;
    }
}

void move_to_front(CachedItem cach, CacheList* list) {
    cach->next = list->start->next;
    list->start->next = cach;
}

void evict(CacheList* list) {
    struct CacheItem* res = list->start->next;
    if (res == NULL) return;
    if (res->next == NULL) {
        list->totalsize = 0;
        list->start->next = NULL;
        return;
    }
    while (res->next->next != NULL) {
        res = res->next;
    }
    list->totalsize -= res->next->size;
    res->next = NULL;
}
