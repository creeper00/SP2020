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

void handle_client(void* vargp);
void initialize_struct(Request* req);
void parse_request(char request[MAXLINE], Request* req);
void assemble_request(Request* req, char request[MAXLINE]);
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
    int lines = 0;
    char buf[MAXLINE], request[MAXLINE], response[MAXLINE];
    rio_t rio, srio;
    int clientfd;

    Rio_readinitb(&rio, connfd);

    Request* rq = Malloc(sizeof(Request));
    initialize_struct(rq);
    Rio_readlineb(&rio, buf, MAXLINE);
    parse_request(buf, rq);
    sprintf(rq->header, "%sHost: %s:%s\r\n", rq->header, rq->hostname, rq->port);
    sprintf(rq->header, "%s%s", rq->header, user_agent_hdr);
    sprintf(rq->header, "%sConnection: close\r\n", rq->header);
    sprintf(rq->header, "%sProxy-Connection: close\r\n", rq->header);
    
    if (lines <= 0) {
        return;
    }
    assemble_request(rq, request);

    clientfd = Open_clientfd(rq->hostname, rq->port);

    get_from_server(request, clientfd, connfd);
    Close(clientfd);
    Close(connfd);
}

void parse_request(char request[MAXLINE], Request* req) {
    char name[MAXLINE];
    char ver[MAXLINE];
    sscanf(request, "%s %s %s", req->method, name, ver);

    char* tmp1 = strstr(name, "//");
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

void assemble_request(Request* req, char request[MAXLINE]) {
    sprintf(request, "%s %s %s\r\n", req->method, req->query, req->version);
    sprintf(request, "%s%s", request, req->header);
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
    rio_t rio_to_server;
    Rio_readinitb(&rio_to_server, serverfd);

    Rio_writen(serverfd, request, strlen(request));
    while ((n = Rio_readnb(&rio_to_server, response, MAXLINE)) > 0) {
        Rio_writen(clientfd, response, n);
    }
}
