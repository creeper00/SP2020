#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
typedef struct {
    char method[MAXLINE];
    char hostname[MAXLINE];
    char version[MAXLINE];
    char port[MAXLINE];
    char query[MAXLINE];
    char header[MAXLINE];
}Request;

typedef struct CachedItem CachedItem;

struct CachedItem {
    char name[MAXLINE];
    size_t size;
    char* response;
    struct CachedItem* next;
};

typedef struct {
    size_t totalsize;
    pthread_rwlock_t* lock;
    CachedItem* start;
} CacheList;

CacheList* clist = NULL;

CacheList* cache_init();
void cache_insert(CachedItem* item, CacheList* cache);
void evict(CacheList* cache);
CachedItem* find(char request[MAXLINE], CacheList* cache);
void move_to_front(CachedItem* item, CacheList* cache);
void cache_destruct(CacheList* cache);

void handle_client(void* vargp);
void initialize_struct(Request* req);
void parse_request(char request[MAXLINE], Request* req);
int get_from_cache(char request[MAXLINE], int clientfd);
void get_from_server(char request[MAXLINE], int serverfd, int clientfd);
void print_struct(Request* req);


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: proxy <port>\n\nwhere <port> is listening port number between 4500 and 65000\n");
        return 0;
    }
    printf("%s", user_agent_hdr);

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
    char buf[MAXLINE], request[MAXLINE], response[MAXLINE];
    rio_t rio, srio;
    int clientfd;

    Rio_readinitb(&rio, connfd);

    Request* rq = Malloc(sizeof(Request));
    initialize_struct(rq);
    char header[MAXLINE];
    buf[0] = 0;
    while (strncmp(buf, "\r\n", 2)) {
        size_t n = Rio_readlineb(&rio, buf, MAXLINE);
        if (check == 0) {
            parse_request(buf, rq);
            sprintf(header, "Host: %s:%s\r\n", rq->hostname, rq->port);
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

    if (get_from_cache(request, clientfd)) {
        printf("%s", request);
        printf("Got from Cache\n");
        Close(clientfd);
        return;
    }

    clientfd = Open_clientfd(rq->hostname, rq->port);

    get_from_server(request, clientfd, connfd);
    Close(clientfd);
    Close(connfd);
}

int get_from_cache(char request[MAXLINE], int clientfd) {
    CachedItem* item = NULL;
    if ((item = find(request, cache_list)) != 0) {
        Rio_writen(clientfd, item->response, item->size);
        return 1;
    }
    return 0;
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
    char cachebuf[MAX_OBJECT_SIZE];
    size_t total_size = 0;
    char save = 0;
    rio_t rio_to_server;
    Rio_readinitb(&rio_to_server, serverfd);

    Rio_writen(serverfd, request, strlen(request));
    while ((n = Rio_readnb(&rio_to_server, response, MAXLINE)) > 0) {
        Rio_writen(clientfd, response, n);
        if (total_size + n < MAX_OBJECT_SIZE) {
            strncpy(cachebuf + total_size, response, n);
            save = 1;
        }
    }
    if (save) {
        CachedItem* item = malloc(sizeof(CachedItem));
        strncpy(item->name, request, strlen(request) + 1);
        item->size = total_size + 1;
        item->response = malloc(sizeof(char) * (total_size + 1));
        strncpy(item->response, cachebuf, total_size + 1);
        cache_insert(item, clist);
    }
}

CacheList* cache_init() {
    CacheList* cache = malloc(sizeof(CacheList));
    cache->totalsize = 0;
    cache->lock = malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(cache->lock, NULL);
    cache->start = NULL;
    return cache;
}

void cache_insert(CachedItem* item, CacheList* cache) {
    pthread_rwlock_wrlock(cache->lock);
    while ((item->size) + (cache->totalsize) >= MAX_CACHE_SIZE) {
        evict(cache);
    }
    item->next = cache->start;
    cache->start = item;
    cache->totalsize += item->size;
    pthread_rwlock_unlock(cache->lock);
}

void evict(CacheList* cache) {
    CachedItem* node = cache->list;
    while (node && node->next && node->next->next) {
        node = node->next;
    }
    if (node == NULL) {
        return;
    }

    if (node->next == NULL) {
        cache->list = NULL;
        cache->size = 0;
    }
    else {
        cache->size -= node->next->size;
        free(node->next);
        node->next = NULL;
    }
}

CachedItem* find(char request[MAXLINE], CacheList* cache) {
    if (cache->list == NULL) {
        return NULL;
    }
    pthread_rwlock_rdlock(cache->lock);
    CachedItem* ret = NULL;
    CachedItem* node = cache->start;
    if (!strncmp(node->request, request, strlen(request))) {
        ret = node;
        node = NULL;
    }
    while (node && node->next) {
        CachedItem* target = node->next;
        if (!strncmp(target->request, request, strlen(request))) {
            ret = target;
            break;
        }
        node = node->next;
    }
    pthread_rwlock_unlock(cache->lock);
    if (ret != NULL && node != NULL) {
        move_to_front(node, cache);
    }
    return ret;
}

void move_to_front(CachedItem* cache, CacheList* list) {
    pthread_rwlock_wrlock(list->lock);
    CachedItem* tmp= cache->next;
    cache->next = tmp->next;
    tmp->next = list->start;
    list->start = tmp;
    pthread_rwlock_unlock(list->lock);
}

void cache_destruct(CacheList* cache) {
    CachedItem* node = cache->list;
    while (node) {
        CachedItem* next = node->next;
        free(node);
        node = next;
    }
    pthread_rwlock_destroy(cache->lock);
    free(cache->lock);
    free(cache);
}
