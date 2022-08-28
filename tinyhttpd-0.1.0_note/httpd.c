/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE*);
void cannot_execute(int);
void error_die(const char*);
void execute_cgi(int, const char*, const char*, const char*);
int get_line(int, char*, int);
void headers(int, const char*);
void not_found(int);
void serve_file(int, const char*);
int startup(u_short*);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/

/**
 * @brief 处理从套接字上监听到的一个 HTTP 请求
 * @param 客户端套接字
 * @return void
 */
void accept_request(int client)
{
    char buf[1024]; // 将http请求体载入缓冲区,便于后序分析
    int numchars;
    char method[255]; // 缓存buf中http请求头状态行中的<method>
    char url[255];    // 缓存buf中http请求头状态行中的<request-url>
    char path[512];   // 缓存buf中http请求头状态行中的<request-url>
    size_t i, j;
    struct stat st;
    int cgi = 0; /* becomes true if server decides this is a CGI
                  * program */
    char* query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf)); // 获取http状态行
    i        = 0;
    j        = 0;

    // 分别解析http状态行 <method> <request-url> <version>
    // 循环读取<method>直到遇到空格结束
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    // 判断method是否等于GET或POST,如果都不是使用unimplemented函数放回未实现该方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unimplemented(client);
        return;
    }

    // 如果为POST方法开启cgi标志位
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    // 解析<request-URL>
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    // 解析url中的请求字符串
    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {
            cgi           = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/') // 如果path只是一个目录，默认设置为首页index.html
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */ // 如果访问的网页不存在,则循环读取剩余请求头信息并抛弃
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client); // 返回未找到网页给浏览器
    }
    else {                                    // 如果存在该网页
        if ((st.st_mode & S_IFMT) == S_IFDIR) // 判断文件是否为目录,是目录则返回index.html
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) // 判断文件是否有执行权限
            cgi = 1;
        if (!cgi)
            serve_file(client, path); // 没有权限则返回普通文件内容
        else
            execute_cgi(client, path, method, query_string); // 有权限则采用cgi进行处理
    }

    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
/**
 * @brief 请求失败
 * @param client 客户端套接字
 * @return void
 */
void bad_request(int client)
{
    // 拼接并发送响应体与错误页到buf,与下面的cannot_execute,headers,not_found,unimplemented函数同理,都是发送服务器错误状态码与错误信息
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
/**
 * @brief 读取服务器上某个文件写到 socket 套接字
 * @param client 客户端套接字
 * @param resource 文件指针
 * @return void
 */
void cat(int client, FILE* resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource)) { // 判断文件是否读取到末尾
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
/**
 * @brief 处理发生在执行 cgi 程序时出现的错误
 * @param client 客户端套接字
 * @return void
 */
void cannot_execute(int client)
{
    // 拼接并发送响应体与错误页到buf
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
/**
 * @brief 使用 perror 打印出错误消息
 * @param sc 错误消息
 * @return void
 */
void error_die(const char* sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
/**
 * @brief 执行 CGI 脚本
 * @param client 客户端套接字
 * @param path CGI 脚本路径
 * @param method 请求方法
 * @param query_string 请求字符串
 * @return void
 */
void execute_cgi(int client, const char* path, const char* method, const char* query_string)
{
    char buf[1024];
    int cgi_output[2]; // 由于该项目进程间使用管道通信,所以再次定义了两个读写管道
    int cgi_input[2];
    pid_t pid; // 进程PID
    int status;
    int i;
    char c;
    int numchars       = 1;
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)             // 如果是GET请求,读取并且丢弃头信息
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else /* POST */
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) { // content_length等于-1发送错误的请求给浏览器
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }
    if (pid == 0) /* child: CGI script */ // fork出一个子进程运行CGI脚本
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1); // 1代表着stdout,0代表着stdin,将系统标准输出重定向为cgi_output[1]
        dup2(cgi_input[0], 0);
        close(cgi_output[0]); // 关闭cgi_output中的读通道
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else { /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL); // 执行CGI脚本
        exit(0);
    }
    else { /* parent */
        close(cgi_output[1]); // 关闭cgi_output中的写通道
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0); // 等待子进程结束
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */

/**
 * @brief 从套接字中获取一行，该行是否以换行符结尾
 * @param client 客户端套接字
 * @param buf 保存数据的缓冲区
 * @param size 缓冲区大小
 * @return int 存储的字节数（不包括null）
 */
/**********************************************************************/
int get_line(int sock, char* buf, int size)
{
    int i  = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/

/**
 * @brief 有关文件的信息性 HTTP 标头
 * @param client 客户端套接字
 * @param filename 文件名称
 * @return void
 */
void headers(int client, const char* filename)
{
    // 拼接并发送响应体与错误页到buf
    char buf[1024];
    (void)filename; /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/

/**
 * @brief 响应404信息
 * @param client 客户端套接字
 * @return void
 */
void not_found(int client)
{
    // 拼接并发送响应体与错误页到buf
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/

/**
 * @brief 调用 cat 把服务器文件内容返回给浏览器
 * @param client 客户端套接字
 * @param filename 文件名称
 * @return void
 */
void serve_file(int client, const char* filename)
{
    FILE* resource = NULL;
    int numchars   = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/

/**
 * @brief 此函数启动侦听网络连接的过程
 * @param port 监听端口
 * @return int 监听套接字
 */
int startup(u_short* port)
{
    int httpd = 0; // 服务器监听套接字
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0); // 创建基于数据流套接字
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family      = AF_INET;                             // 地址协议族,AF_INET为IPv4,AF_INET6为IPv6
    name.sin_port        = htons(*port);                        // 监听端口,htons函数将本地字节序转换为网络字节序
    name.sin_addr.s_addr = htonl(INADDR_ANY);                   // 监听地址,INADDR_ANY为任意地址即0.0.0.0,htonl函数将本地字节序转换为网络字节序
    if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0) // 将服务器监听套接字绑定地址
        error_die("bind");

    if (*port == 0) /* if dynamically allocating a port */ // 动态分配端口
    {
        int namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0) // 监听套接字
        error_die("listen");
    return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/

/**
 * @brief 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持
 * @param client 客户端套接字
 * @return void
 */
void unimplemented(int client)
{
    // 拼接并发送响应体与错误页到buf
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;                      // 服务器监听套接字
    u_short port    = 2555;                    // 服务器监听端口,等于0使用系统分配未被占用端口
    int client_sock = -1;                      // 客户端通信套接字
    struct sockaddr_in client_name;            // 客户端地址结构体
    int client_name_len = sizeof(client_name); // 客户端地址结构体长度
    pthread_t newthread;                       // 创建线程结构体

    server_sock = startup(&port); // 使用startup函数封装socket创建-绑定-监听步骤
    printf("httpd running on port %d\n", port);

    // 循环接收客户端发送的请求,来一个请求创建一个线程进行处理
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(client_sock); */
        if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
            perror("pthread_create");
    }

    // 释放服务器监听文件描述符
    close(server_sock);

    return (0);
}
