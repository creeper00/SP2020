#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct {
    char method[10];
    char hostname[MAXLINE];
    char port[10];
    char query[MAXLINE];
    char version[10];
    char header[MAXLINE];
} Request;

typedef struct CachedItem CachedItem;

struct CachedItem {
    char name[MAXLINE];
    size_t size;
    char* response;
    struct CachedItem* next;
};

typedef struct {
    size_t tsize;
    pthread_rwlock_t* sem;
    CachedItem* start;
} CacheList;

CacheList* clist = NULL;

void handle_client(void* vargp);
void initialize_struct(Request* req);
void parse_request(char request[MAXLINE], Request* req);
void assemble_request(Request* req, char request[MAXLINE]);
int get_from_cache(char request[MAXLINE], int clientfd);
void get_from_server(char request[MAXLINE], int serverfd, int clientfd);
void print_struct(Request* req);

/****** Cache functions ******/
void cache_init();
void cache_insert(CachedItem* item, CacheList* cache);
void evict(CacheList* cache);
CachedItem* find(char request[MAXLINE], CacheList* cache);
void move_to_front(CachedItem* item, CacheList* cache);
void cache_destruct(CacheList* cache);

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: proxy <port>\n\nwhere <port> is listening port number between 4500 and 65000\n");
        return 0;
    }
    printf("%s", user_agent_hdr);
    Signal(SIGPIPE, SIG_IGN);

    cache_init();

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
    cache_destruct(cache_list);
    return 0;
}

void handle_client(void* vargp) {
    Pthread_detach(pthread_self());
    int connfd = *((int*)vargp);
    //free(vargp);

    int lines = 0;
    int serverfd;
    char buf[MAXLINE];
    char request[MAXLINE];
    Request* req = malloc(sizeof(Request));
    rio_t rio;

    initialize_struct(req);
    Rio_readinitb(&rio, connfd);
    buf[0] = 0;
    while (strncmp(buf, "\r\n", 2)) {
        size_t n = Rio_readlineb(&rio, buf, MAXLINE);
        if (lines == 0) {
            parse_request(buf, req);
            sprintf(req->header, "%sHost: %s:%s\r\n", req->header, req->hostname, req->port);
            sprintf(req->header, "%s%s", req->header, user_agent_hdr);
            sprintf(req->header, "%sConnection: close\r\n", req->header);
            sprintf(req->header, "%sProxy-Connection: close\r\n", req->header);
        }
        else if (strncmp(buf, "Proxy-Connection:", 17) && strncmp(buf, "Host:", 5) && strncmp(buf, "User-Agent:", 11)) {
            sprintf(req->header, "%s%s", req->header, buf);
        }
        lines++;
    }

    sprintf(request, "%s %s %s\r\n", req->method, req->query, req->version);
    sprintf(request, "%s%s", request, req->header);

    CachedItem* item = find(request, cache_list);
    if (item != NULL) {
        Rio_writen(connfd, item->response, item->response_size);
        Close(connfd);
        return;
    }
    printf("%s", request);
    serverfd = Open_clientfd(req->hostname, req->port);

    printf("Connected to server: (%s, %s)\n", req->hostname, req->port);
    get_from_server(request, serverfd, connfd);
    Close(serverfd);
    Close(connfd);
    free(req);
}

void initialize_struct(Request* req) {
    for (int i = 0; i < MAXLINE; i++) {
        req->hostname[i] = 0;
        req->query[i] = 0;
        req->header[i] = 0;
    }
    for (int i = 0; i < 10; i++) {
        req->method[i] = 0;
        req->version[i] = 0;
        req->port[i] = 0;
    }
}

void parse_request(char request[MAXLINE], Request* req) {
    char name[MAXLINE];
    char ver[MAXLINE];
    sscanf(request, "%s %s %s", req->method, name, ver);

    char* tmp1 = strstr(name, "//");
    char* tmp2 = strstr(tmp1, ":");
    char* tmp3;
    if (tmp2 != NULL) tmp3 = strstr(tmp2, "/");
    else tmp3 = strstr(tmp1 + 2, "/");
    if (tmp2 != NULL) {
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


void get_from_server(char request[MAXLINE], int serverfd, int clientfd) {
    char response[MAXLINE];
    char buff[MAX_OBJECT_SIZE];
    size_t n;
    size_t size = 0;
    int check = 0;
    rio_t rio;
    Rio_readinitb(&rio, serverfd);

    Rio_writen(serverfd, request, strlen(request));
    while ((n = Rio_readnb(&rio, response, MAXLINE)) > 0) {
        Rio_writen(clientfd, response, n);
        size += n;
        if (size < MAX_OBJECT_SIZE) {
            if (check == 0) snprintf(buff, n + 1, "%s", response);
            else snprintf(buff, size + 1, "%s%s", buff, response);
            check = 1;
        }
    }
    size++;
    if (n == -1 && errno == ECONNRESET) return;
    if (check) {
        CachedItem* item = malloc(sizeof(CachedItem));
        snprintf(item->name, strlen(request) + 1, "%s", request);
        item->size = size;
        item->response = malloc(sizeof(char) * size);
        snprintf(item->response, size, "%s", buff);
        cache_insert(item, clist);
    }
}

/****** Cache functions ******/

void cache_init() {
    clist = malloc(sizeof(CacheList));
    clist->tsize = 0;
    clist->sem = malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(clist->sem, NULL);
    clist->start = NULL;
}

void cache_insert(CachedItem* item) {
    pthread_rwlock_wrlock(clist->sem);
    while ((item->size) + (clist->tsize) >= MAX_CACHE_SIZE) {
        evict(clist);
    }
    item->next = clist->start;
    clist->start = item;
    clist->tsize += item->size;
    pthread_rwlock_unlock(clist->sem);
}

void evict(CacheList* cache) {
    pthread_rwlock_wrlock(cache->sem);
    CachedItem* parent = cache->start;
    if (parent == NULL) return;
    if (parent->next == NULL) {
        parent = NULL;
        return;
    }
    CachedItem* node = parent->next;
    while (node->next != NULL) {
        parent = node;
        node = parent->next;
    }
    parent->next = NULL;
    pthread_rwlock_unlock(cache->sem);
    return;

}

CachedItem* find(char request[MAXLINE], CacheList* cache) {
    if (cache->start == NULL) return NULL;
    pthread_rwlock_rdlock(cache->sem);
    CachedItem* node = cache->start;
    CachedItem* parent = NULL;
    while (node != NULL && strcmp(node->name, request)) {
        parent = node;
        node = node->next;
    }
    pthread_rwlock_unlock(cache->sem);
    if (node != NULL && parent != NULL) {
        parent->next = node->next;
        move_to_front(node, cache);
    }
    return node;
}

void move_to_front(CachedItem* item, CacheList* cache) {
    pthread_rwlock_wrlock(cache->sem);
    item->next = cache->start;
    cache->start = item;
    pthread_rwlock_unlock(cache->sem);
}

void cache_destruct(CacheList* cache) {
    CachedItem* node = cache->start;
    while (node) {
        CachedItem* next = node->next;
        free(node);
        node = next;
    }
    pthread_rwlock_destroy(cache->sem);
    free(cache->sem);
    free(cache);
}
