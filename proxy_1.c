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

    size_t n;
    char buf[MAXLINE], request[MAXLINE], response[MAXLINE];
    rio_t rio, srio;
    int clientfd;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);

    Request* rq = Malloc(sizeof(Request));
    initialize_struct(rq);
    parse_request(buf, rq);
    assemble_request(rq, request);

    clientfd = Open_clientfd(rq->hostname, rq->port);

    Rio_readinitb(&srio, clientfd);
    Rio_writen(clientfd, request, MAXLINE);
    while ((n = Rio_readlineb(&srio, response, MAXLINE)) > 0) {
        Rio_writen(clientfd, response, n);
    }
    Close(clientfd);
    Close(connfd);
}

void parse_request(char request[MAXLINE], Request* req) {
    char uri[MAXLINE];
    char version[MAXLINE];
    sscanf(request, "%s %s %s", req->method, uri, version);
    int uri_len = strlen(uri);
    strtok(uri, "/");
    char* url;
    if (strncmp(uri, "http", 4)) {
        url = malloc(sizeof(char) * (strlen(uri) + 1));
        strncpy(url, uri, strlen(uri) + 1);
    }
    else {
        url = strtok(uri + strlen(uri) + 1, "/ ");
    }
    if (strlen(url) + strlen(uri) + 2 < uri_len) {
        char* query = url + strlen(url) + 1;
        sprintf(req->query, "/%s", query);
    }
    else {
        strncpy(req->query, "/", 2);
    }
    if (strstr(url, ":")) {
        char* hostname = strtok(url, ":");
        char* port = hostname + strlen(hostname) + 1;
        sprintf(req->hostname, "%s", hostname);
        sprintf(req->port, "%s", port);
    }
    else {
        sprintf(req->hostname, "%s", url);
        strncpy(req->port, "80", 3);
    }
    if (!strncmp(version, "HTTP/1.1", 8)) {
        strncpy(req->version, "HTTP/1.0", 9);
    }
}

void assemble_request(Request* req, char request[MAXLINE]) {
    char host[MAXLINE];
    char user[MAXLINE];
    sprintf(host, "Host: %s%s\r\n", req->name, req->port);
    sprintf(user, "User-Agent: %s\r\n", user_agent_hdr);
    sprintf(request, "GET %s HTTP/1.0\r\n", req->query);
    sprintf(request, "%s%s%s%s%s", request, host, user, "Connection: close\r\n", "Proxy-Connection: close\r\n");
}

void initialize_struct(Request* req) {
    for (int i = 0; i < MAXLINE; i++) {
        req->method[i] = '\0';
        req->hostname[i] = '\0';
        req->port[i] = '\0';
        req->query[i] = '\0';
        req->version[i] = '\0';
    }
}
