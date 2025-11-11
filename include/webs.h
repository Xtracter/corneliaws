/*
Copyright (c) 2022 CrazedoutSoft / Fredrik Roos

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _WEBS_CONF_
#define _WEBS_CONF_

#define ORG_SERVER_NAME "Cornelia"
#define ORG_SERVER_VERSION "1.3"

#define MAX_WWW_ROOT		256
#define MAX_WORK_DIR		1024
#define MAX_DEFAULT_PAGE 	64
#define MAX_CGI_BIN		256
#define MAX_EXEC_DEF		64
#define MAX_LOG_FILE		64
#define MAX_AUTH_REALMS		64
#define MAX_CONTENT_TYPES	64
#define MAX_VIRTUAL_HOSTS	64
#define MAX_PROXY_TARGETS	64
#define MAX_UEP			16
#define MAX_VIRTUAL_PATH	256
#define MAX_VIRTUAL_FILE	256
#define MAX_HTTP_HEADERS	256
#define MAX_ENV_VARS		256
#define MAX_QUERYSTRING	 	2048

#define ACCESS_ALL              "*/*"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/time.h>
#include <poll.h>
#include <time.h>
#include <crypt.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

void handle_request(int sockfd, char* clientIP, void* cSSL);
void set_user_proxy(char* cmd);

typedef struct tls_client_t {

	void* ctx;
	void* ssl;

} tls_client;

typedef struct proxy_targets_t {

	char host[256];
	char proxy_host[256];
	int proxy_port;
	char type[8];

} proxy_target;

typedef struct user_endpoint_t {

        char endpoint[256];
        char response[256];
        char content_type[128];

} user_endpoint;

typedef struct virtual_files_t {

        char name[128];
        char path[1024];
	char file[255];

} virtual_files;

typedef struct virtual_host_t {

	char name[128];
	char path[1024];

} virtual_host;

typedef struct auth_t {

        char realm[128];
        char path[1024];
        char base64auth[256];

} auth_conf;

typedef struct cgi_exec_t {
	char ext[16];
	char exec[128];
} cgi_exec;

typedef struct content_type_t {

        char file_ext[16];
        char content_type[64];

} content_type;

typedef struct http_request_t {

	int   sockfd;
	char  request[2048];
        char  method[12];
        char  file[256];
        char  path[2048];
        char  query_string[2048];
        char  httpv[12];
        char  clientIP[16];
        char  clientDomain[1024];
        char* headers[MAX_HTTP_HEADERS];
        int   headers_len;
        unsigned char* post_data;
	int post_data_len;
	void* cSSL;
	char connection[65];
	char virtual_path[MAX_VIRTUAL_PATH];

} http_request;

typedef struct http_response_t {

        int sockfd;
        http_request* request;
        char  content_type[128];
        int   content_length;
        char* envp[MAX_ENV_VARS];

} http_response;


typedef struct server_conf_t {

	unsigned int port;
	unsigned int ssl_port;
	unsigned int tls_port;
	unsigned int max_keep_alive;
	char server_name[64];
        char www_root[256];
	char workdir[MAX_WORK_DIR];
        char default_page[MAX_DEFAULT_PAGE];
	char cgi_bin[MAX_CGI_BIN];
        char execs[MAX_EXEC_DEF];
	char logfile[MAX_LOG_FILE];
	char allow_dir_listing[8];
	char allow_put[8];
	char allow_delete[8];
	char rest_path[256];
        auth_conf* auth[MAX_AUTH_REALMS];
	content_type* content_types[MAX_CONTENT_TYPES];
	cgi_exec* exec_c[MAX_EXEC_DEF];
	char cert[1024];
	char cert_key[1024];
	char certcrt[1024];
	char keycrt[1024];
	int  max_keep_alive_requests;
	int  keep_alive_timeout;
	proxy_target* v_proxys[MAX_PROXY_TARGETS];
	virtual_host* v_hosts[MAX_VIRTUAL_HOSTS];
	virtual_files* v_files[MAX_VIRTUAL_FILE];
	user_endpoint* uep[MAX_UEP];
	unsigned int max_post_data;

} server_conf;

void  logc(int type, const char* string, ...);
void  doPut(http_response* response);
void  init_server();
int   get_file_size(const http_request* request);
int   readline_ssl(const http_request* request, char* buffer, int len);
int   readline(const http_request* request, char* buffer, int len);
void  send_options_reply(http_request* request);
void  send_bad_request(http_response* response, char* code);
void  send_forbidden(http_request* request);
void  send_internal_error(http_response* response);
void  list_dir (const char* dir, char* buffer);
void  send_list_dir(http_request* request);
int   find_default_page(http_request* request);
char* get_content_type(char* file, char* ct);
char* get_head(http_response* response, char* head, char* code, int skipcl, int skipct);
int   exec_cgi(http_response* response, const char* exe_ptr);
int   write_plain_file(const http_response* response, int len, char*path, char* fil);
char* getExecutable(const char* file);
char* get_user(const char* basic, char* buff);
int   get_user_pass_from_file(const char* file, const char* base64);
int   handle_auth(http_request* request);
void  exec_response(http_request *request);
int   handle_virtual_files(http_request* request);
int   parse_http(char* buffer, http_request* request);
void  dump_request(http_request* r);
char* get_header(const http_request *request, const char* header);
void  parse_headers(char* buffer, http_request* request);
int   read_post_data(http_request *request, unsigned int len);
void  handle_request(int sockfd, char* clientIP, void* cSSL);
int   exec_request(int sockfd, char* clientIP, void* cSSL);
void  free_request(http_request* r);
void  free_response(http_response* r);
void  parse_env(http_response* res);
int   read_http_responses();
char* encode_url(unsigned char* url, char* url_enc);
char* get_header(const http_request* request, const char* header);
char* encode_url(unsigned char* url, char* url_enc);
int   socket_read(const http_request* request, char* buffer, int len);
int   socket_write(const http_request* request, const char* buffer, int len);
int   proxy_read(int sockfd, char* buffer, int len, void* cSSL);
int   proxy_write(int sockfd, const char* buffer, int len, void* cSSL, void* pSSL);
int   exec_request(int sockfd, char* clientIP, void* cSSL);
void  check_conf(int use_ssl, int use_tls);
int   handle_proxy(int sockfd, http_request* request);
int   proxy_connect(char* clientIP, int port);
void  domain_to_ip(char* dest, const char* domain);
int   is_regular_file(const server_conf* serv, const http_request* request);
int   accept_encoding(const http_request* request, const char* enc);
user_endpoint* get_user_endpoint(char* argstr);
virtual_host* get_virtual_host(char* host);
void  send_bad_request2(http_request* request);
void  send_internal_error2(http_request* request);
int   write_chunked(int fd, http_request* request, int gzip);
int   accept_encoding(const http_request* request, const char* enc);
//void  usleep(unsigned long);
int   compress_stream(const unsigned char *input, size_t input_len,
                    unsigned char *output, size_t *output_len);
int getnameinfo(socklen_t hostlen, socklen_t servlen;
                       const struct sockaddr *restrict addr, socklen_t addrlen,
                       char host[],
                       socklen_t hostlen,
                       char serv[],
                       socklen_t servlen,
                       int flags);


#endif

