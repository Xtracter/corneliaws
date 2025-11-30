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

/*
	This project was initially started to teach my son Alfred C programming.
	Cornelia is his younger sister. She is Small, Fast and Furious...
	/Fredrik.
*/

#include "../include/webs.h"
#include "../include/mkpasswd.h"
#include "../include/tls.h"
#include "../include/ssl.h"
#include "../include/conf.h"
#include "../include/misc.h"
#include "../include/base64.h"

#define SA struct sockaddr
#define AUTH_REQUEST_SENT		0
#define AUTH_OK				1
#define BAD_AUTH			-1
#define CGI_BIN	 			"/cgi-bin/"

#define CONN_KEEP_ALIVE			1
#define CONN_CLOSE			0
#define MAX_ALLOC			65536
#define MAX_FILE_PATH			4096

#define HTTP_POST			"POST"
#define HTTP_GET			"GET"
#define HTTP_PUT			"PUT"
#define HTTP_DELETE			"DELETE"
#define HTTP_HEAD			"HEAD"
#define HTTP_OPTIONS			"OPTIONS"
#define AUTHORIZATION 			"Authorization="

#define D_204_NO_CONTENT		"204 No Content"
#define D_404_NOT_FOUND			"404 Not Found"
#define D_200_OK			"200 OK"
#define D_500_INTERNAL_SERVER_ERROR 	"500 Internal Server Error"
#define D_400_FORBIDDEN 		"400 Forbidden"
#define BUFF_SIZE 			5000

#define HTTP_PROTO			"http"
#define SSL_PROTO			"ssl"
#define TLS_PROTO			"tls"


int c_debug = 0;
int dump_req=0;
int conf_time=0;
int auto_reload_conf=0;
char local_proto[6];
server_conf serv_conf;
auth_conf   a_conf;

char log_tmp[3072];
char* bad_request;
char* internal_server_error;
char* unauthorized;
char* forbidden;
char* ACAOrigin;
char  conf_file[1024] = "conf/corny.conf";
char  cip[16];
void dump_request(http_request* r);
proxy_target* user_proxy_target = NULL;
void add_user_endpoint(user_endpoint*, server_conf* serv);
void confc(int init);


#define CHUNK_SIZE 4096
#define ACCESS_LOG "log/access.log"
#define ERROR_LOG  "log/error.log"
#define ERROR	    1
#define ACCESS	    0

void init_server() {

	int loop=1;
	int connfd;
	int port = serv_conf.port;
        int sockfd;
	unsigned int len;
        struct sockaddr_in servaddr, cli;
        struct sockaddr_in* pV4Addr;
        struct in_addr ipAddr;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
	  printf("Fatal: Socket creation failed.\n");
	  exit(-1);
        }
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
	    printf("setsockopt(SO_REUSEADDR) failed");

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(port);

        if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
	  fprintf(stderr,"Fatal: Socket bind failed.\nIs server already running?\nTry bin/restart.sh\n\n");
	  printf("\nFatal: Socket bind failed.\nIs server already running?\nTry bin/restart.sh\n\n");
	  exit(-1);
        }

        if ((listen(sockfd, 5)) != 0) {
	  printf("Fatal: sock listen failed.\n\nTry bin/restart.sh\n\n");
	  exit(-1);
        }

        len = sizeof(cli);
        printf("\nCornelia listening on %d [HTTP]\n", serv_conf.port);

	confc(1);
        while(loop){
	  connfd = accept(sockfd, (SA*)&cli, &len);
	  if(check_shutdown(1)) {
	    printf("Shutdown received. Cornelia HTTP exiting.\n");
	    shutdown(connfd,SHUT_RDWR);
	    break;
	  }
	  if(auto_reload_conf) confc(0);
	  if(connfd==-1){
	  char log_tmp[64];
	  sprintf(log_tmp,"Client socket failed. errno %d\n", errno);
	   logc(ERROR, log_tmp);
           continue;
	  }
	  int pid = fork();
	  if(pid>0){

	    memset(&cip[0],0,16);
            pV4Addr = (struct sockaddr_in*)&cli;
            ipAddr = pV4Addr->sin_addr;
            inet_ntop(AF_INET, &ipAddr, &cip[0], INET_ADDRSTRLEN );

	    struct timeval tv;
	    tv.tv_sec = 0;
	    tv.tv_usec = serv_conf.keep_alive_timeout;
	    setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	    handle_request(connfd,cip,NULL);
	    shutdown(connfd,SHUT_RDWR);
	    loop=0;
	    if(c_debug) printf("exit\n");
	  }
        }
        if (connfd < 0) {
	  perror("Fatal:Server accept failed.\n");
	  exit(-1);
        }

}

void localtime_r(time_t* t, struct tm* tm_info);

int check_shutdown(int mode){

	FILE* fd;
	char buff[256];
	int ret=0;

	sprintf(buff,"%s/stat",getenv("CORNELIA_HOME"));
	if((fd=fopen(buff,"r+"))!=NULL){
	  if(mode){
	    	if(fgets(buff,256,fd)!=NULL){
	     		if(strstr(buff,"shutdown")!=NULL){
		      		ret=1;
	     		}
	    	}
	  }
	  else{
	    fprintf(fd,"running");
	    ret=0;
	 }
	 fclose(fd);
       }else logc(ERROR,"Bad check_shutdown() file open\n");

 return ret;
}

void confc(int init){

	struct stat attr;
	time_t f;

	if (stat(conf_file, &attr) == 0) {
	  f=attr.st_mtime;
	  if(init) {
	   conf_time=f;
	   return;
  	  }
	  if(conf_time < f){
	   reload_conf(conf_file, &serv_conf);
	   printf("%s reloaded\n", conf_file);
	   conf_time = (int)f;
	  }
	}

}

void logc(int type, const char* string, ...){

	FILE* fd;
	char file[256];
	va_list args;

	if(type){
	  sprintf(file,"%s/%s",getenv("CORNELIA_HOME"),ERROR_LOG);
	}else{
	  sprintf(file,"%s/%s",getenv("CORNELIA_HOME"),ACCESS_LOG);
	}
	if((fd=fopen(file,"a"))!=NULL){
	  for (va_start(args, string); *string != '\0'; ++string){
 	    fwrite(string,1,strlen(string),fd);
	  }
	  fwrite("\n",1,0,fd);
	  fclose(fd);
	}else{
	  printf("Err: Can't write to logfile: %s\n", file);
	}
}


int ip_to_domain(const char* ip, char* host) {

    struct sockaddr_storage sa;
    socklen_t sa_len;

    memset(&sa, 0, sizeof(sa));

    struct sockaddr_in *sa4 = (struct sockaddr_in *)&sa;
    if (inet_pton(AF_INET, ip, &sa4->sin_addr) == 1) {
        sa4->sin_family = AF_INET;
        sa_len = sizeof(struct sockaddr_in);
    }
    else {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&sa;
        if (inet_pton(AF_INET6, ip, &sa6->sin6_addr) == 1) {
            sa6->sin6_family = AF_INET6;
            sa_len = sizeof(struct sockaddr_in6);
        } else {
            fprintf(stderr, "Invalid IP address format: %s\n", ip);
            return EXIT_FAILURE;
        }
    }

    int ret = getnameinfo((struct sockaddr *)&sa, sa_len,
                          host, sizeof(host),
                          NULL, 0, 8);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


void domain_to_ip(char* dest, const char* domain){

        struct hostent *host_info;
        struct in_addr *address;

        host_info = gethostbyname(domain);
        address = (struct in_addr *) (host_info->h_addr_list[0]);
        strcpy(dest, inet_ntoa(*address));
}

int proxy_connect(char* clientIP, int port){

    int status, client_fd;
    struct sockaddr_in serv_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr,"proxy: Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, clientIP, &serv_addr.sin_addr)
        <= 0) {
        fprintf(stderr,
            "\nproxy: Invalid address/ Address not supported \n");
        return -1;
    }

    if ((status
         = connect(client_fd, (struct sockaddr*)&serv_addr,
                   sizeof(serv_addr)))
        < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    return client_fd;
}



int p_ssl_write(void* ssl, const char* buffer, int len){

	int r = 0;
	#ifndef NO_SSL
	r=ssl_write(ssl,buffer,len);
	#endif

  return r;
}

int p_ssl_read(void* ssl, char* buffer, int len){

	int r = 0;
	#ifndef NO_SSL
	r=ssl_read(ssl,buffer,len);
	#endif

  return r;
}

int handle_proxy(int sockfd, http_request* request){

        int clientfd=0;
        char* tok;
        char buffer[BUFF_SIZE];
        char header[1024];
        char rem_host[256];
        int rem_port=0,r,n;
	char proxy_host[256];
	tls_client tlsc;
	char tport[8];
	char thost[256];
	int SSL=1;

	memset(&tlsc,0,sizeof(tls_client));
        memset(buffer,0,BUFF_SIZE);
        memset(header,0,1024);

        if(c_debug) printf("[handle_proxy]\n");

	char* head = get_header(request,"Host=");
	if(head==NULL){
	  return -1;
	}

        strcpy(buffer,head);
        tok = strtok(buffer,":");
        if(tok!=NULL){
                strcpy(rem_host,tok);
                tok=strtok(NULL,":");
		if(tok==NULL) rem_port=80;
		else rem_port = atoi(tok);
        }else {
	  	if(c_debug) printf("[exit_handle_proxy 1]\n");
		return -1;
	}
        n=0;
        memset(buffer,0,BUFF_SIZE);
        while(n<64){
                if(serv_conf.v_proxys[n]==NULL) break;
	        if(c_debug) printf("'%s' '%s':%d | '%s'\n", serv_conf.v_proxys[n]->host, serv_conf.v_proxys[n]->proxy_host,
			serv_conf.v_proxys[n]->proxy_port,rem_host);
    	            if(strcmp(rem_host,serv_conf.v_proxys[n]->host)==0 ||
                        strcmp("all", serv_conf.v_proxys[n]->host)==0) {
                        domain_to_ip(buffer,serv_conf.v_proxys[n]->proxy_host);
			strcpy(thost, serv_conf.v_proxys[n]->proxy_host);
	 		sprintf(tport,"%d",serv_conf.v_proxys[n]->proxy_port);
                        rem_port = serv_conf.v_proxys[n]->proxy_port;
			strcpy(proxy_host, serv_conf.v_proxys[n]->proxy_host);
			if(strcmp(serv_conf.v_proxys[n]->type,"http")==0) SSL=0;
                        break;
                }
                n++;
        }
	if(c_debug) printf("Proxy: %s %s mode:%s\n", thost, tport, (SSL==0?"http":"ssl"));
        if(strlen(buffer)==0) {
	  if(c_debug) printf("[exit_handle_proxy 2]\n");
	  return -1;
	}

	if(SSL) {
	  #ifndef NO_SSL
	  if(open_tls_socket(thost,tport,&tlsc)==-1){
	   logc(ERROR, "tls con failed: %s %s\n", buffer, tport);
	   free_client_socket(tlsc.ctx,tlsc.ssl);
	   if(c_debug) printf("[exit_handle_proxy 3]\n");
	   return -1;
	  }
	  #endif
	}
        else {
	  if((int)(clientfd = proxy_connect(buffer,rem_port))==-1) return -1;
	}

	strcat(request->request,"\n");

        if(!SSL) send(clientfd,request->request,strlen(request->request),0);
	else p_ssl_write(tlsc.ssl,request->request,strlen(request->request));

	if(c_debug) printf("\n");
        for(int i=0;i<request->headers_len; i++){
                memset(buffer,0,BUFF_SIZE);
		if(strstr(request->headers[i],"Connection=")!=NULL){
                  sprintf(buffer,"Connection: close\n");
		}
		else if(strstr(request->headers[i],"Host=")!=NULL){
		  if(strcmp(tport,"80")!=0){
                    sprintf(buffer,"Host: %s:%s\n", proxy_host, tport);
		  }else{
                    sprintf(buffer,"Host: %s\n", proxy_host);
		  }
		}else{
                  sprintf(buffer,"%s\n",str_replace(request->headers[i],"=",": "));
		}
		if(c_debug) printf("%s", buffer);

		if(!SSL) send(clientfd,buffer,strlen(buffer),0);
	        else {
		  r=p_ssl_write(tlsc.ssl,buffer,strlen(buffer));
		}
        }

        if(!SSL) send(clientfd,"\n\n",2,0);
	else p_ssl_write(tlsc.ssl,"\n\n",2);

	if(!SSL){
          while((r=read(clientfd,buffer,256))>0){
	    r=send(sockfd,buffer,r,0);
          }
	}else{
	  while((r=p_ssl_read(tlsc.ssl,buffer,256))>0){
	    r=send(sockfd,buffer,r,0);
	  }
	}

	if(SSL) {
	  #ifndef NO_SSL
	  free_client_socket(tlsc.ctx,tlsc.ssl);
	  #endif
	}
        else close(clientfd);

	if(c_debug) printf("[exit_handle_proxy]\n");

        return 0;
}


int get_file_size(const http_request* request){

        FILE *fd;
        int   size=-1;
        char  tmp[4096];
        sprintf(tmp,"%s%s%s",&request->virtual_path[0],&request->path[0],&request->file[0]);

	 if((fd=fopen(tmp,"rb"))!=NULL){
           fseek(fd,0L,SEEK_END);
           size = ftell(fd);
           fseek(fd,0L,SEEK_SET);
           fclose(fd);
        }

return size;
}

int readline_ssl(const http_request* request, char* buffer, int len){

        char sb[2];
        int n = 0;
        char c = 0;
	int r=0;

          while((r=socket_read(request,&sb[0],1))>0 && n<len-1){
	   printf("%c",sb[0]);
	   if(r==-1) {
	    return -1;
	   }
           c = sb[0];
           if(c=='\r') continue;
             if(c == '\n'){
             buffer[n] = '\0';
             break;
            }
            buffer[n++] = c;
          }
	  if(n==0) buffer[0]='\0';

 return n;
}

int readline(const http_request* request, char* buffer, int len){

        char sb[2];
        int n = 0;
        char c = 0;
	int r=0;

	struct pollfd fd;
	int ret;

	if(request->cSSL!=NULL){
	  return readline_ssl(request, buffer, len);
	}

	fd.fd = request->sockfd;
	fd.events = POLLIN;
	ret = poll(&fd, 1, (serv_conf.keep_alive_timeout/1000));

	switch (ret) {
    	  case -1:
	  case 0:
          return -1; // Socket timed out.
      	default:

          while((r=socket_read(request,&sb[0],1))>0 && n<len-1){
	   if(r==-1) return -1;
           c = sb[0];
           if(c=='\r') continue;
             if(c == '\n'){
             buffer[n] = '\0';
             break;
            }
            buffer[n++] = c;
          }
	  if(n==0) buffer[0]='\0';

          break;
	}

 return n;
}


int socket_read(const http_request* request, char* buffer, int len){

	int r = 0;
	if(request->cSSL==NULL){
	  r=read(request->sockfd, buffer, len);
	}else{
	  #ifndef NO_SSL
	  r=ssl_read(request->cSSL, buffer, len);
	  #endif
	}

  return r;
}

int socket_write(const http_request* request, const char* buffer, int len){

	int r = 0;
	if(request->cSSL==NULL){
	  r=write(request->sockfd, buffer, len);
	}else{
	 #ifndef NO_SSL
	 r=ssl_write(request->cSSL, buffer, len);
	 #endif
	}

  return r;
}

void send_bad_request(http_response* response, char* code){
	if(c_debug) printf("[send bad request]\n");
     	char buffer[4096];
	response->content_length=strlen(bad_request);
	strcpy(&response->content_type[0],"text/html");
        get_head(response, &buffer[0], code,0,0);
	socket_write(response->request, &buffer[0], strlen(&buffer[0]));
	socket_write(response->request,"\r\n",2);
        socket_write(response->request, bad_request, strlen(bad_request));
	socket_write(response->request, "\n\n",2);
}

void send_bad_request2(http_request* request){

	if(c_debug) printf("[send bad request 2]\n");
        char buffer[4096];
        http_response response;
        response.request=request;
        response.content_length=strlen(bad_request);
        strcpy(&response.content_type[0],"text/html");
        get_head(&response, &buffer[0], D_404_NOT_FOUND,0,0);

        socket_write(request, &buffer[0], strlen(&buffer[0]));
        socket_write(request,"\r\n",2);
        socket_write(request, bad_request, strlen(bad_request));
	socket_write(request, "\n\n",2);

}


void send_forbidden(http_request* request){

	if(c_debug) printf("[send forbidden]\n");

     	char buffer[4096];
	http_response response;
	response.request=request;
        response.content_length=strlen(forbidden);
        strcpy(&response.content_type[0],"text/html");
        get_head(&response, &buffer[0], D_400_FORBIDDEN,0,0);
	socket_write(request, &buffer[0], strlen(&buffer[0]));
        socket_write(request,"\r\n",2);
        socket_write(request, forbidden, strlen(forbidden));
	socket_write(request, "\n\n",2);

}

void send_internal_error(http_response* response){

	if(c_debug) printf("[send internal error]\n");
     	char buffer[4096];
        response->content_length=strlen(internal_server_error);
        strcpy(&response->content_type[0],"text/html");
        get_head(response, &buffer[0], D_500_INTERNAL_SERVER_ERROR, 0,0);
        socket_write(response->request, &buffer[0], strlen(&buffer[0]));
        socket_write(response->request,"\r\n",2);
        socket_write(response->request, internal_server_error, strlen(internal_server_error));
	socket_write(response->request, "\n\n",2);
}


void send_internal_error2(http_request* request){

	if(c_debug) printf("[send internal error 2]\n");
     	char buffer[4096];
	http_response response;
	response.request=request;
        response.content_length=strlen(forbidden);
        strcpy(response.content_type,"text/html");
        get_head(&response, buffer, D_500_INTERNAL_SERVER_ERROR, 0,0);
        socket_write(response.request, &buffer[0], strlen(&buffer[0]));
        socket_write(response.request,"\r\n",2);
        socket_write(response.request, internal_server_error, strlen(internal_server_error));
	socket_write(response.request, "\n\n",2);
}


void list_dir (const char* dir, char* buffer) {

   DIR *dp;
   struct dirent *ep;
   char tmp[1024];
   char fold[4096];
   char reg[4096];

   memset(tmp, 0, 1024);
   memset(fold,0, 2048);
   memset(reg, 0, 2048);

   dp = opendir (dir);
   if (dp != NULL){

      while ((ep = readdir (dp))){
        if(strlen(ep->d_name)<2) continue;
        if(strcmp(ep->d_name,"..")==0){
          sprintf(tmp,"<a href=\"%s\"><img src=\"/res/back.gif\" /> </a><a href=\"%s\">%s</a><br/>\n", ep->d_name, ep->d_name, ep->d_name);
	  strcat(buffer,tmp);
	  continue;
        }else if(ep->d_type==4){
          sprintf(tmp,"<a href=\"%s/\"><img src=\"/res/folder.gif\" /> </a><a href=\"%s/\">%s</a><br/>\n", ep->d_name, ep->d_name, ep->d_name);
	  strcat(fold,tmp);
	  continue;
        }else if(ep->d_type==8) {
          sprintf(tmp,"<a href=\"%s\"><img src=\"/res/text.gif\" /> </a><a href=\"%s\">%s</a><br/>\n", ep->d_name,ep->d_name, ep->d_name);
	  strcat(reg,tmp);
        }
      }

      if(strlen(buffer)+strlen(fold)>sizeof(fold)){
	 int size=strlen(buffer)+strlen(fold)+1;
	 buffer=(char*)realloc(buffer, size);
         strcat(buffer, fold);
      }else{
	strcat(buffer,fold);
      }

      if(strlen(buffer)+strlen(reg)>sizeof(reg)){
	 int size=strlen(buffer)+strlen(reg)+1;
	 buffer=(char*)realloc(buffer, size);
         strcat(buffer, reg);
      }else{
	strcat(buffer,reg);
      }

      (void) closedir (dp);
    }
  else{
    perror ("Couldn't open the directory for listing");
  }

}

void send_list_dir(http_request* request){

	int size = 2048+1024;
	char buffer[size];
	char dir[size+1024];
	char tmp[size+2048];
	char head[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\n";

	memset(buffer,0,size);
	memset(dir,0,size);
	memset(tmp,0,size);

	sprintf(dir,"%s/%s%s%s", serv_conf.workdir, request->virtual_path, request->path, request->file);
	if(file_exists(dir)){
	  list_dir(dir,buffer);
	  sprintf(tmp,"<!DOCTYPE html>\n<html><head><title>%s</title></head><body><b><i>Cornelia Web Server by CrazedoutSoft (c)</i></b><br><h1>Index of %s</h1>\n",
		&request->path[0], &request->path[0]);

	  memset(dir,0,1024);
	  sprintf(dir,"%sContent-Length: %d\r\n", &head[0], (int)strlen(buffer)+(int)strlen(tmp)+14);

	  socket_write(request,dir,strlen(dir));
          socket_write(request,"\r\n",2);
	  socket_write(request,tmp,strlen(tmp));
          socket_write(request, buffer, strlen(buffer));
	  socket_write(request, "</body></html>", 14);
	}else {
	  send_bad_request2(request);
	}

}


int find_default_page(http_request* request){

	if(strstr(request->method,HTTP_PUT)!=NULL) return 1;

	char* ptr;
	char fi[4096];
	char tmp[strlen(&serv_conf.default_page[0])+1];
	int found=0;


	strcpy(tmp,&serv_conf.default_page[0]);
	ptr=strtok(tmp,",");
	if(ptr!=NULL){
 	  sprintf(fi, "%s/%s%s%s", &serv_conf.workdir[0], &request->virtual_path[0], &request->path[0], ptr);
	  if(file_exists(fi)) {
	   strcpy(&request->file[0],ptr);
	   return 1;
	  }
	}
	while((ptr=strtok(NULL,","))!=NULL){
	 sprintf(fi, "%s/%s%s%s", &serv_conf.workdir[0], &request->virtual_path[0], &request->path[0],ptr);
	  if(file_exists(fi)) {
	   strcpy(&request->file[0],ptr);
	   return 1;
	  }
	}


 return found;
}

char* get_content_type(char* file, char* ct){

	char* ptr;
	char ext[64];
	int n = 0;
	int all_ok=0;

	if((ptr=strstr(file,"."))!=NULL){
	 strcpy(&ext[0],ptr);
	}

	while(1){
	 if(serv_conf.content_types[n]==NULL) break;
	  if(strcmp(&serv_conf.content_types[n]->file_ext[0], &ext[0])==0) {
 	    strcpy(&ct[0], &serv_conf.content_types[n]->content_type[0]);
	    all_ok=1;
	    break;
	  }
	 n++;
	}

	if(!all_ok) {
	  strcpy(ct,"text/html");
	}

	// TODO: Bad fix for type="module"
	if(ends_with(file,".js")==0){
	  strcpy(ct,"text/javascript");
	}

 return ct;
}


char* get_head(http_response* response, char* head, char* code, int skipcl, int skipct){

	char tmp[1024];
	char date[64];

	memset(head,0,4096);
	memset(date,0,64);

	get_formated_date(date,64);

	sprintf(tmp,"%s %s\n", response->request->httpv, code);
	strcat(head,tmp);
	sprintf(tmp,"Server: %s\n", &serv_conf.server_name[0]);
	strcat(head,tmp);
	sprintf(tmp,"Date: %s\n", date);
	strcat(head,tmp);
	sprintf(tmp,"Allow: %s,%s,%s,%s", HTTP_GET,HTTP_HEAD,HTTP_POST,HTTP_OPTIONS);
	if(strcmp(serv_conf.allow_put,"yes")==0) {
	  strcat(tmp,",");
	  strcat(tmp, HTTP_PUT);
	}
	if(strcmp(serv_conf.allow_put,"yes")==0) {
	  strcat(tmp,",");
	  strcat(tmp, HTTP_PUT);
	}
	if(strcmp(serv_conf.allow_delete,"yes")==0) {
	  strcat(tmp,",");
	  strcat(tmp, HTTP_DELETE);
	}
	strcat(tmp,"\n");
	strcat(head,tmp);
	if(strlen(ACAOrigin)>0){
	  sprintf(tmp,"%s", ACAOrigin);
	  strcat(head,tmp);
	}
	if(!skipct){
	  sprintf(tmp,"%s: %s\n", "Content-Type", &response->content_type[0]);
	  strcat(head,tmp);
	}
	if(!skipcl){
	  sprintf(tmp,"%s: %d\n", "Content-Length", response->content_length);
	  strcat(head,tmp);
	}
	sprintf(tmp,"%s: %s\n", "Connection", &response->request->connection[0]);
	strcat(head,tmp);

 return head;
}

int exec_cgi(http_response* response, const char* exe_ptr){

        int n = 0;
        int r = 0;
	int z = 0;
	char* argv[128];

        char headb[2048];
        char file_path[4096];
	char executable[1024];
	int  clen=0;

        int pipefd[2];
        int pin[2];
	int pid;
	int abort=0;

        n=pipe(pipefd);
        n=pipe(pin);
        n=0;

	if(c_debug) printf("[exec_cgi]\n");

        sprintf(file_path,"%s%s%s",
		&response->request->virtual_path[0],&response->request->path[0],&response->request->file[0]);

	if(strcmp(exe_ptr,"[shell]")!=0){
	  strcpy(executable, exe_ptr);
	  argv[0]=(char*)malloc(strlen(file_path)+1);
 	  strcpy(argv[0],file_path);
	  argv[1]=NULL;
	}else{
	  strcpy(executable, file_path);
	  argv[0]=(char*)malloc(strlen(file_path)+1);
 	  strcpy(argv[0],file_path);
	  argv[1]=NULL;
	}

	int ex=0;
        if ((pid=fork()) > 0){
          close(pipefd[0]);
          dup2(pin[0], 0);
          dup2(pipefd[1], 1);
          dup2(pipefd[1], 2);
          close(pipefd[1]);
          if((ex=execve(executable, argv, response->envp))==-1){
	   logc(ERROR, "Error in exec:", executable);
	   abort=1;
	  }
        }else if(pid==-1){
	  printf("Bad fork() in exec_cgi\n");
	}
	else{
          close(pipefd[1]);
	  if(!abort){
	    if(c_debug) printf("in post:%d\n", response->request->post_data!=NULL);
            if(strcmp(&response->request->method[0],HTTP_POST)==0 && response->request->post_data!=NULL) {
	      clen = response->content_length;
	      r=write(pin[1], response->request->post_data, clen);
	      if(c_debug) printf("[post_data_written: %d]\n",r);
	    }
            get_head(response, headb, D_200_OK, 1, 1);
	    strcat(headb,"Transfer-Encoding: chunked\n");
	    n=socket_write(response->request, headb, strlen(headb));
	    // TODO: Make gzip/chunked work.
	    /*
	    if((z = accept_encoding(response->request,"gzip"))){
              memset(headb,0,2048);
	      sprintf(headb,"Content-Encoding: gzip\n");
  	      n=socket_write(response->request, headb, strlen(headb));
	    }
	    */
	    n=write_chunked(pipefd[0], response->request,z);
	  }

	  (void)(r);
	  close(pipefd[0]);
	  close(pin[0]);
	  close(pin[1]);
	}

        n = 0;
        while(1){
	 if(argv[n]==NULL) break;
	   free(argv[n++]);
        }

	if(abort) {
		printf("Abort\n");
		send_internal_error(response);

	}

 return n;
}

int write_chunked(int fd, http_request* request, int gzip){

	int r=0;
	int n=0;
	char line[64];
	char buffer[CHUNK_SIZE];
	char comp[CHUNK_SIZE];
	size_t c_len;

	http_request tmp_request;

	tmp_request.sockfd=fd;
	tmp_request.cSSL = NULL;//request->cSSL;

	// Read cgi head;
	memset(buffer,0,256);
	while(1){
	  n=readline(&tmp_request, buffer, 256);
	  strcat(buffer,"\n");
	  n=socket_write(request, buffer, strlen(buffer));
	  if(n<3) break;
	}

	memset(buffer,0,CHUNK_SIZE);
	memset(comp,0,CHUNK_SIZE);

	while((r=read(fd,buffer, CHUNK_SIZE))){
	  if(gzip){
	    n=compress_stream((unsigned char*)buffer,r,(unsigned char*)comp,&c_len);
	    if(dump_req) printf("gzip:%d %d %zu\n", n, r, c_len);
	    r=c_len;
	  }
	  sprintf(line,"%X\r\n",r);
	  n=socket_write(request, line, strlen(line));
          if(gzip) {
	    n=socket_write(request, comp, r);
	  }else{
	    n=socket_write(request, buffer, r);
          }
  	  n=socket_write(request, "\r\n", 2);
	}

	sprintf(line,"%X\r\n",0);
	n=socket_write(request, line, strlen(line));
	n=socket_write(request, "\r\n", 2);

  return n;
}

int write_plain_file(const http_response* response, int len, char*path, char* fil){

	FILE *fd;
	char* enc = "Content-Encoding: gzip\n";
	size_t size = 16384;
	int r=0;
	char file[2048];
	unsigned char zoutput[size];
	int z = 0;
	unsigned char tmp[size+1];

	int gzip = 0; //accept_encoding(response->request,"gzip"); - gzip/chunked malfunctions at this time.

	if(gzip){
	  socket_write(response->request,enc,strlen(enc));
	}
	socket_write(response->request,"\n",1);

	memset(file,0,2048);
	memset(tmp,0,size+1);
	memset(zoutput,0,size);

	sprintf(file,"%s%s%s", &response->request->virtual_path[0], path, fil);
	if(c_debug) printf("%s:%d\n",file,len);

	int n = 0;
	if((fd=fopen(file,"rb"))!=NULL){
	  while((r=fread(tmp, 1, size, fd))>0){
	    if(gzip){
	      z=compress_stream(tmp,r,zoutput,&size);
	      ((void)z);
	      r=socket_write(response->request, (char*)zoutput,size);
	    }else{
	      socket_write(response->request, (char*)tmp, r);
	    }
	    n+=r;
	  }
	  if(n!=len) fprintf(stderr,"Bytes read: %d are less than content-length: %d\n", n, len);
	  fclose(fd);
	}else{
	  fprintf(stderr,"Bad file: %s%s\n", path, file);
	  return -1;
	}

 return 0;
}

char* getExecutable(const char* file){

	int n = 0;
	while(1){
	  if(serv_conf.exec_c[n]==NULL) break;
	  if( strstr( file,&serv_conf.exec_c[n]->ext[0]) !=NULL){
	   return &serv_conf.exec_c[n]->exec[0];
	  }
	  n++;
	}

 return NULL;
}

char* get_user(const char* basic, char* buff){

	char* ptr;
	if((ptr=strstr(basic,":"))!=NULL){
	  int i=ptr-basic;
	  strcpy(buff, basic);
	  buff[i] = '\0';
	}

  return buff;
}

char* get_passwd(const char* basic){
	return strstr(basic,":")+1;
}

int get_user_pass_from_file(const char* file, const char* base64){

	FILE* fd;
	int ret=0;
	char buffer[128];
	if((fd=fopen(file,"r"))!=NULL){
	 while((fgets(buffer, 128, fd))!=NULL){
	  if(strcmp(clip(buffer), base64)==0){
	   ret=1;
	  }
 	 }
	 fclose(fd);
	}

 return ret;
}

int handle_auth(http_request* request){

	 int len;
	 char tmp[2048];
	 char cmp[2050];
	 int handled=AUTH_OK;
	 char* basic;
	 int n = 0;
	 char* decode;
	 char* user;
	 char* passwd;

	while(1){
	 if(serv_conf.auth[n]==NULL) break;
	 if(!startsw(&request->path[0], &serv_conf.auth[n]->path[0])){
	  if((basic=get_header(request,AUTHORIZATION))!=NULL){
		decode=(char*)base64_decode((unsigned char*)basic+6, strlen(basic+6), &len);
	 	decode[len]='\0';
		user=get_user(decode, tmp);
		passwd=get_passwd(decode);
		sprintf(cmp, "%s:%s", user, crypt(passwd,SALT));
		if((strstr(&serv_conf.auth[n]->base64auth[0],".passwd"))!=NULL){
		  sprintf(tmp,"%s/conf/%s", &serv_conf.workdir[0], &serv_conf.auth[n]->base64auth[0]);
	          if(get_user_pass_from_file(tmp, cmp)){
		   return AUTH_OK;
		}
		}else{
		  if(strcmp(&serv_conf.auth[n]->base64auth[0],cmp)==0){
		   return AUTH_OK;
		}
	      }
	    }
	    strcpy(tmp,"HTTP/1.1 401 Unauthorized\r\n");
	    socket_write(request,tmp,strlen(tmp));

	    strcpy(tmp,"Connection: close\r\n");
	    socket_write(request,tmp,strlen(tmp));

	    sprintf(tmp,"WWW-Authenticate: Basic realm=%s\r\n\r\n",&serv_conf.auth[n]->realm[0]);
	    socket_write(request,tmp,strlen(tmp));

	    socket_write(request,unauthorized,strlen(unauthorized));

	    handled=AUTH_REQUEST_SENT;
	  }
  	  n++;
	}

 return handled;
}

int allow_rest_path(const char* path){

	char* ptr;
	char copy[256];
	int ret = 0;

	memset(copy,0,256);
	if(c_debug) printf("allow rest %s %s\n",path,serv_conf.rest_path);

	strcpy(copy,serv_conf.rest_path);
	ptr=strtok(copy,",");

	while(ptr!=NULL){
	  if(strcmp(ptr,path)==0 || strcmp(ptr,"*")==0) {
	    ret=1;
	    break;
	  }
	  ptr=strtok(NULL,",");
	}

	if(c_debug) printf("%s\n", ret==1?"OK":"FAIL");

  return ret;
}


void doPut(http_response* response){

	char file_path[MAX_FILE_PATH];
	char head[1024];
	int r = 0;
	FILE* fd;

	if(c_debug) printf("[doPut]\n");
	if(!allow_rest_path(response->request->path)){
	  send_forbidden(response->request);
	  return;
	}

	sprintf(file_path,"%s%s%s",
		response->request->virtual_path,response->request->path,response->request->file);
	if(file_path[strlen(file_path)-1]=='/') file_path[strlen(file_path)-1] = '\0';
	if((fd=fopen(file_path,"wb"))!=NULL){
          r=fwrite(response->request->post_data,1,response->request->post_data_len,fd);
	  if(r<response->request->post_data_len){
	    printf("Bad Post data length\n");
	  }
	  fclose(fd);
	  get_head(response,head,D_200_OK, 1,0);
	  strcat(head,"\n");
	  socket_write(response->request,head,strlen(head));
	}else{
	 send_forbidden(response->request);
	}

	if(c_debug) printf("[end doPut]\n");
}

void doDelete(http_response* response){

        char file_path[MAX_FILE_PATH];
        char head[1024];

        if(c_debug) printf("[doDelete]\n");
	if(!allow_rest_path(response->request->path)){
	  send_forbidden(response->request);
	  return;
	}

        sprintf(file_path,"%s%s%s",
                response->request->virtual_path,response->request->path,response->request->file);
        if(file_path[strlen(file_path)-1]=='/') file_path[strlen(file_path)-1] = '\0';
	if(remove(file_path)!=0){
	  send_forbidden(response->request);
	}else{
	  get_head(response,head,D_200_OK, 1,0);
	  strcat(head,"\n");
	  socket_write(response->request,head,strlen(head));
	}

        if(c_debug) printf("[exit doDelete]\n");
}

int doHead(http_response* response){

	char buffer[1024];

	if(c_debug) printf("[do head]\n");
	get_head(response, buffer, D_200_OK, 0,0);
	strcat(buffer,"\n");
	return socket_write(response->request,buffer,strlen(buffer));

}

int doOptions(http_response* response){

	char buffer[1024];

	if(c_debug) printf("[do options]\n");
	get_head(response, buffer, D_204_NO_CONTENT, 1,0);
	strcat(buffer,"\n");
	return socket_write(response->request,buffer,strlen(buffer));

}


void exec_response(http_request *request){

	char tmp[1024];
	char ext[128];
	int auth_mode=-1;
	char* exe_ptr;
	http_response response;
	memset(&response,0,sizeof(http_response));
	response.envp[0]=NULL;
	response.request=request;

	auth_mode=handle_auth(request);
	switch(auth_mode){
	 case AUTH_REQUEST_SENT:
	  return;
	 case BAD_AUTH:
	  //send_bad_auth(request);
	  return;
	}


  	if(strcmp(request->method,HTTP_PUT)!=0) response.content_length = get_file_size(request);
	strcpy(&response.content_type[0],get_content_type(&request->file[0], &ext[0]));

	if(c_debug) printf("[content-length read]\n");
	if(c_debug) fprintf(stdout,"[%s]\n",response.request->method);

	if(response.content_length<1 && strstr(request->method,HTTP_PUT)==NULL){
	  send_bad_request(&response, D_404_NOT_FOUND);
	}else{
	  parse_env(&response);
	  if(strcmp(response.request->method,HTTP_PUT)==0 &&
		strcmp(serv_conf.allow_put,"yes")==0){
	   doPut(&response);
	  }else if(strcmp(response.request->method,HTTP_DELETE)==0 &&
		strcmp(serv_conf.allow_delete,"yes")==0){
	   doDelete(&response);
	  }else if(strcmp(response.request->method,HTTP_HEAD)==0){
	   doHead(&response);
	  }else if(strcmp(response.request->method,HTTP_OPTIONS)==0){
	   doOptions(&response);
	  }else if((exe_ptr=getExecutable(request->file)) || strcmp(&response.request->method[0],HTTP_POST)==0){
	    exec_cgi(&response, exe_ptr);
	  }else{
	    get_head(&response,&tmp[0], D_200_OK, 0,0);
	    socket_write(request,&tmp[0],strlen(tmp));
	    if(write_plain_file(&response, response.content_length,
	        &request->path[0], &request->file[0])==-1){
		send_bad_request(&response, D_404_NOT_FOUND);
	    }
	  }
	}
	free_response(&response);

}

int handle_user_enpoints(http_request* request){

	int res=0;
	int n=0;
	char* tmp;
	char* origins = (char*)malloc(4096);
	user_endpoint* uep;

	if(c_debug) printf("[parse uep] %s\n",request->path);
	tmp = (char*)malloc(4096);
	sprintf(tmp,"%s%s", &request->path[0], request->file);
	while((uep=serv_conf.uep[n++])!=NULL){
	  if(strcmp(tmp,uep->endpoint)==0){
	    if(c_debug) printf("[start uep]\n");
	    if(c_debug) printf("eup:%s %s\n", tmp, uep->endpoint);
	    socket_write(request,"HTTP/1.1 200 OK\n",16);
	    socket_write(request,"Server: Cornelia\n",17);
	    socket_write(request,"Connection: close\n",18);
	  printf("2\n");

	    if(strlen(ACAOrigin)>0){
	     sprintf(origins,"%s", ACAOrigin);
	     socket_write(request,origins,(int)strlen(origins));
	    }
	  printf("3\n");

	    tmp = (char*)realloc(tmp,255);
	    if(strlen(uep->content_type)){
	      if(strlen(uep->content_type)>0){
	        sprintf(tmp,"Content-Type: %s\n", uep->content_type);
	      }else sprintf(tmp,"Content-Type: application/json\n");
	      socket_write(request, tmp, strlen(tmp));
	    }
	  printf("4\n");

	    if(strlen(uep->response)) {
	        tmp = realloc(tmp,128);
	        sprintf(tmp,"Content-Length: %d\n\n", (int)strlen(uep->response));
	        socket_write(request, tmp, strlen(tmp));
	        tmp = realloc(tmp,strlen(uep->response)+2);
 	        sprintf(tmp,"%s", uep->response);
	        socket_write(request, tmp, strlen(tmp));
	     }
 	  res=1;
	  break;
	}
	if(c_debug) printf("[end parse uep]\n");
	}

	free(origins);
  	free(tmp);

 return res;

}

int handle_virtual_files(http_request* request){

	int n = 0;
	int res=0;
	char virt[MAX_FILE_PATH];

	if(c_debug) printf("[start virtual files]\n");
 	while(1){
	  if(serv_conf.v_files[n]!=NULL){
	    sprintf(virt,"%s%s",&request->path[0],&request->file[0]);
	    if(strcmp(virt,serv_conf.v_files[n]->name)==0){
	      sprintf(request->path,"%s",serv_conf.v_files[n]->path);
	      sprintf(request->file,"%s",serv_conf.v_files[n]->file);
	      printf("virt: %s%s", request->path, request->file);
	      res=1;
	      break;
	    }
	  }else break;
	 n++;
	}

	if(c_debug) printf("[end virtual files: %d]\n",res);

 return res;
}

int parse_http(char* buffer, http_request* request){

        char* ptr;
	int res = 0;

	if(c_debug) printf("[parse_http]\n");

	ptr=strtok(&buffer[0]," ");
	if(ptr==NULL) return -1;
	strcpy(&request->method[0],ptr);

	ptr=strtok(NULL," ");
	if(ptr==NULL) return -1;
	split(ptr, &request->path[0], &request->file[0], &request->query_string[0], MAX_QUERYSTRING);

	ptr=strtok(NULL," ");
	if(ptr==NULL) return -1;
	strcpy(&request->httpv[0],ptr);

	if(strcmp(&request->path[0], CGI_BIN)==0){
	  strcpy(&request->path[0], &serv_conf.cgi_bin[0]);
	}

	if(handle_virtual_files(request) && c_debug) printf("Virtual file is active\n");

	if(strlen(&request->file[0])==0) {
	    return -1;
	}

	if(c_debug) printf("[exit parse_http]\n");

 return res;
}

void dump_request(http_request* r){

	int n = 0;
	printf("%s\n", r->request);
	while(1){
	 if(r->headers[n]==NULL) break;
	 printf("%s\n", r->headers[n++]);
	}
}

char* get_header(const http_request *request, const char* header){

	int n=0;
	char *ptr;

	if(c_debug) printf("[get_header %s]\n", header);

	while(1){
	 if(request->headers[n]==NULL || strstr(request->headers[n],"=")==NULL) break;
	 if(!startsw(request->headers[n], header)){
	   ptr = strstr(request->headers[n],"=");
	   if(ptr!=NULL) return ptr+1;
	   else return NULL;
	 }
  	 n++;
	 if(n>50) break;
	}

	if(c_debug) printf("[exit get_header]\n");

 return NULL;
}

void parse_headers(char* buffer, http_request* request){

	char* ptr;
	int  index;
	char name[128];
	char value[1024];
	char tmp[4096];

	if((ptr=strstr(buffer,":"))!=NULL){
	  index=ptr-buffer;
	  buffer[index]='\0';
	  strcpy(name,buffer);
	  strcpy(value,ptr+2);
	  sprintf(tmp,"%s=%s%c", name,value,'\0');
	  request->headers[request->headers_len]=(char*)malloc(strlen(tmp)+2);
	  memset(request->headers[request->headers_len],0, strlen(tmp)+2);
	  strcpy(request->headers[request->headers_len++], tmp);
	}
	request->headers[request->headers_len]=NULL;

}

void parse_env(http_response* res){

	int n = 0;
	char tmp[4096+1024];
	char buff[1024];
	char cpy[1024];

	while(1){
	  memset(&buff[0],0,1024);
	  memset(&cpy[0],0,1024);
	  if(res->request->headers[n]==NULL) break;
	  strcpy(tmp, res->request->headers[n]);
	  res->envp[n]=(char*)malloc(strlen(tmp)+6);
	  sprintf(res->envp[n],"HTTP_%s", toupperc(&cpy[0],tmp,'='));
	  if(n>=MAX_ENV_VARS-10) break;
	  n++;
	}

	if(strcmp(&res->request->method[0],"POST")==0 && res->request->post_data!=NULL){
	  res->content_length=atoi(get_header(res->request,"Content-Length="));
	  sprintf(tmp,"CONTENT_LENGTH=%d", res->content_length);
	  res->envp[n] = (char*)malloc(strlen(tmp)+1);
	  strcpy(res->envp[n], tmp);
	  n++;

	  sprintf(tmp,"CONTENT_TYPE=%s", get_header(res->request,"Content-Type="));
	  res->envp[n] = (char*)malloc(strlen(tmp)+1);
	  strcpy(res->envp[n], tmp);
	  strcpy(&res->content_type[0], get_header(res->request,"Content-Type="));
	  n++;
	}

	sprintf(tmp,"QUERY_STRING=%s",&res->request->query_string[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"REQUEST_METHOD=%s",&res->request->method[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"REQUEST_URI=%s%s\?%s",&res->request->path[0],&res->request->file[0],&res->request->query_string[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	strcpy(tmp,"REDIRECT_STATUS=200");
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"SCRIPT_FILENAME=%s%s%s", &res->request->virtual_path[0], &res->request->path[0],&res->request->file[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"SCRIPT_NAME=%s", &res->request->file[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"SERVER_NAME=%s", &serv_conf.server_name[0]);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	if(res->request->cSSL!=NULL){
	  sprintf(tmp,"HTTPS=true");
	  res->envp[n] = (char*)malloc(strlen(tmp)+1);
	  strcpy(res->envp[n], tmp);
	  n++;
	}

	ip_to_domain(res->request->clientIP, res->request->clientDomain);
	sprintf(tmp,"REMOTE_HOST=%s", res->request->clientDomain);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"REMOTE_ADDR=%s", res->request->clientIP);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"HTTP_REFERER=%s", get_header(res->request,"Referer=")!=NULL?get_header(res->request,"Referer="):"");
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"SERVER_PORT=%d", serv_conf.port);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

	sprintf(tmp,"SERVER_SOFTWARE=%s %s", ORG_SERVER_NAME, ORG_SERVER_VERSION);
	res->envp[n] = (char*)malloc(strlen(tmp)+1);
	strcpy(res->envp[n], tmp);
	n++;

}

int read_post_data(http_request *request, unsigned int len){

	if(c_debug) printf("[read_post_data]\n");
	int r = 0;
	unsigned int n = 0;
	unsigned int m_len = len<serv_conf.max_post_data?len:serv_conf.max_post_data;
	char buff[2];

	if(len>serv_conf.max_post_data){
	  return -1;
	}

	request->post_data = malloc(m_len);
	memset(request->post_data, 0, m_len);
	while((r=socket_read(request, &buff[0], 1))){
	  request->post_data[n++]=buff[0];
	  if(n>=m_len) break;
	}
	request->post_data_len=n;
	if(c_debug) printf("[read_post_data:%d]\n",n);

   return n;
}

virtual_host* get_virtual_host(char* host){

	int n = 0;
	char tmp[256];
	char htmp[256];
	virtual_host* h_ptr = NULL;
	char *ptr;

	strcpy(htmp, host);
	if((ptr=strstr(htmp,":"))!=NULL){
	 htmp[ptr-htmp]='\0';
	}

	while(1){
	  if(serv_conf.v_hosts[n]==NULL) break;
	  sprintf(tmp,"%s", &serv_conf.v_hosts[n]->name[0]);
	  if(strcmp(tmp,htmp)==0){
	    h_ptr=serv_conf.v_hosts[n];
	    break;
	  }
	 n++;
	}

 return h_ptr;
}



void handle_request(int sockfd, char* clientIP, void* cSSL){

	int r=CONN_CLOSE;

	if(c_debug) printf("[handle_request]\n");

	for(int i = 0; i < serv_conf.max_keep_alive_requests; i++){
	  r = exec_request(sockfd, clientIP, cSSL);
	  if(r!=CONN_KEEP_ALIVE) {
	   return;
	  }
	}
	if(c_debug) printf("[exit handle_request]\n");

}

int exec_request(int sockfd, char* clientIP, void* cSSL){

	int r=0;
	char buffer[2048];
	char tmp[4096];
	int n = 0;
	char* ptr;
	char* host;
	int ret = CONN_CLOSE;

	http_request request;
	memset(&request,0,sizeof(http_request));

	request.sockfd = sockfd;
	request.cSSL = cSSL;

	memset(buffer,0,2048);

	r=readline(&request, buffer, 2048);
	if(r<1) return CONN_CLOSE;

	strcpy(request.request,buffer);

	if(c_debug) printf("[readline]\n");

	sprintf(log_tmp,"%s|%s|%d|%s\n", buffer, clientIP, serv_conf.port, clip(get_date_time(tmp)));
	logc(ACCESS, log_tmp);

	strcpy(&request.virtual_path[0],&serv_conf.www_root[0]);
	memset(tmp,0,4048);

	int parse_h = parse_http(buffer,&request);

	// Handle directoery request without trailing '/'.
	if(c_debug) printf("[handle dir request]\n");

	// Handle user_enpoints
	if(handle_user_enpoints(&request)==1){
	  return CONN_CLOSE;
	}

	if(!is_regular_file(&serv_conf, &request) && strcmp(request.method,HTTP_PUT)!=0 && strcmp(request.method,HTTP_DELETE)!=0){
	  sprintf(tmp,"%s%s/",&request.path[0],&request.file[0]);
	  request.file[0]='\0';
	  strcpy(request.path,tmp);
	  parse_h=-1;
	}

	if(c_debug) printf("[exit parse_http]\n");

	while((r=readline(&request,buffer,1024))>0){
	 if(request.headers_len>=MAX_HTTP_HEADERS) {
		fprintf(stderr,"Header count greater than %d\n", MAX_HTTP_HEADERS);
		break;
	 }
	 parse_headers(buffer,&request);
	 n++;
	}
	if(dump_req) dump_request(&request);


	if(c_debug) printf("[exit read_headers]\n");


	// Handle proxy relay.
	if(serv_conf.v_proxys[0]!=NULL){
	  if(handle_proxy(sockfd,&request)==0){
  	        free_request(&request);
		return CONN_CLOSE;
	  }
	  if(c_debug) printf("No proxy match continuing\n");
	}

	// Set virtual path if Host header matches.
	if((host=get_header(&request,"Host="))!=NULL){
	  virtual_host* h = get_virtual_host(host);
	  if(h!=NULL){
	   // Setting virtual_path.
	   strcpy(&request.virtual_path[0], &h->path[0]);
	  }else{
	   // No virtual path defaulting to www_root.
	   strcpy(&request.virtual_path[0], &serv_conf.www_root[0]);
	  }
	}

	if(c_debug) printf("exit [read virtual_path]\n");

	if(parse_h<0 && find_default_page(&request)==0){
	 if(c_debug) printf("[no default page]\n");
	  if((strcmp(&serv_conf.allow_dir_listing[0],"yes"))==0){
	    send_list_dir(&request);
	  }else {
	    send_forbidden(&request);
	  }
	  close(sockfd);
	}
	if(c_debug) printf("[exit default page]\n");

	if(get_header(&request, "Connection=")!=NULL){
		strcpy(&request.connection[0],
		get_header(&request, "Connection="));
	}else{
		strcpy(&request.connection[0],"Close");
	}

	if(c_debug) printf("[exit connection]\n");

	if(strcmp(&request.method[0],HTTP_POST)==0 || strcmp(&request.method[0],HTTP_PUT)==0){
	 if((ptr = get_header(&request,"Content-Length="))!=NULL){
	  if(read_post_data(&request, atoi(ptr))==-1){
	   send_internal_error2(&request);
	   free_request(&request);
	   return CONN_CLOSE;
	  }
	 }else request.post_data=NULL;
	}

	if(c_debug) printf("exit [content-length]\n");

	request.headers_len=n-1;
	exec_response(&request);
	if(c_debug) printf("exit [exec_response]\n");

	if(strcmp(&request.connection[0],"keep-alive")==0) ret = CONN_KEEP_ALIVE;

	(void)(r);
	free_request(&request);

 return ret;
}

int accept_encoding(const http_request* request, const char* enc){

	int ret=0;
	char* head = get_header(request,"Accept-Encoding=");

	if(head!=NULL){
	  ret = strstr(head,enc)!=NULL;
	}
	if((head=get_header(request,"Accept="))!=NULL){
	  ret = strstr(head,ACCESS_ALL)!=NULL;
	}

  return ret;
}

int is_regular_file(const server_conf* serv, const http_request* request) {

    char path[MAX_FILE_PATH];

    sprintf(path, "%s/%s%s%s", serv->workdir, request->virtual_path, request->path, request->file);

    struct stat path_stat;
    stat(path, &path_stat);

    return S_ISREG(path_stat.st_mode);
}

void free_request(http_request* r){

	int n = 0;
	while(1){
	 if(r->headers[n]==NULL) break;
	 free(r->headers[n++]);
	}
	if(r->post_data!=NULL) free(r->post_data);
}

void free_response(http_response* r){

	int n = 0;
	while(1){
	 if(r->envp[n]==NULL) break;
	 free(r->envp[n++]);
	}
}

int read_http_responses(){

  	int len;
  	int r;
  	FILE* fd;
	char file[1024];
	char buffer[1024];

	sprintf(file,"%s/conf/404.txt", getenv("CORNELIA_HOME"));
  	if((fd=fopen(file,"r"))!=NULL){
   	  fseek(fd,0L,SEEK_END);
   	  len = ftell(fd);
   	  fseek(fd,0L,SEEK_SET);
   	  bad_request = (char*)malloc(len);
   	  r=fread(bad_request,1,len,fd);
   	  (void)(r);
   	  fclose(fd);
  	}else {
	  printf("Bad file: missing conf/404.txt\n");
	  fprintf(stderr,"Bad file: missing conf/404.txt\n");
	  return -1;
	}

	sprintf(file,"%s/conf/500.txt", getenv("CORNELIA_HOME"));
        if((fd=fopen(file,"r"))!=NULL){
          fseek(fd,0L,SEEK_END);
          len = ftell(fd);
          fseek(fd,0L,SEEK_SET);
          internal_server_error = (char*)malloc(len);
          r=fread(internal_server_error,1,len,fd);
          (void)(r);
          fclose(fd);
        }else{
	  fprintf(stderr,"Bad file: missing conf/500.txt\n");
	  printf("Bad file: missing conf/500.txt\n");
	  return -1;
	}

       sprintf(file,"%s/conf/403.txt", getenv("CORNELIA_HOME"));
       if((fd=fopen(file,"r"))!=NULL){
          fseek(fd,0L,SEEK_END);
          len = ftell(fd);
          fseek(fd,0L,SEEK_SET);
          forbidden = (char*)malloc(len);
          r=fread(forbidden,1,len,fd);
          (void)(r);
          fclose(fd);
        }else{
          fprintf(stderr,"Bad file: missing conf/403.txt\n");
          printf("Bad file: missing conf/403.txt\n");
          return -1;
        }

       sprintf(file,"%s/conf/401.txt", getenv("CORNELIA_HOME"));
       if((fd=fopen(file,"r"))!=NULL){
          fseek(fd,0L,SEEK_END);
          len = ftell(fd);
          fseek(fd,0L,SEEK_SET);
          unauthorized = (char*)malloc(len);
          r=fread(unauthorized,1,len,fd);
          (void)(r);
          fclose(fd);
        }else{
          fprintf(stderr,"Bad file: missing conf/401.txt\n");
          printf("Bad file: missing conf/401.txt\n");
          return -1;
        }


       sprintf(file,"%s/conf/Access-Control-Allow.txt", getenv("CORNELIA_HOME"));
        ACAOrigin = (char*)malloc(4096);
	memset(ACAOrigin,0,4096);
       if((fd=fopen(file,"r"))!=NULL){
	 while((fgets(buffer,1024,fd))!=NULL){
	   if(strstr(buffer,"#")!=NULL) continue;
	   strcat(ACAOrigin,buffer);
	 }
	 clipend(ACAOrigin);
	 if(ACAOrigin[strlen(ACAOrigin)-1]!='\n') strcat(ACAOrigin,"\n");
  	 fclose(fd);
        }else{
          fprintf(stderr,"Bad file: missing conf/Access-Control-Allow\n");
          printf("Bad file: missing conf/Access-Control-Allow-Origin\n");
          return -1;
        }


 return 0;
}

char* encode_url(unsigned char* url, char* url_enc){

	char rfc3986[256] = {0};
	char html5[256] = {0};

	url_encoder_rfc_tables_init(&html5[0], &rfc3986[0], 256);
	url_encode( &html5[0], url, url_enc);

 return url_enc;
}

int setenv(const char *name, const char *value, int overwrite);

void check_conf(int use_ssl, int use_tls){


	if(serv_conf.port==0){
	  printf("Warning: http port missing - defaulting to 8080\n");
	  serv_conf.port=8080;
	}
	if(use_ssl && serv_conf.ssl_port==0){
	  printf("Warning: ssl port missing - defaulting to 8081\n");
	  serv_conf.ssl_port=8081;
	}
	if(use_tls && serv_conf.tls_port==0){
	  printf("Warning: tls port missing - defaulting to 8082\n");
	  serv_conf.tls_port=8081;
	}
	if(strlen(&serv_conf.www_root[0])==0){
	  printf("Warning: www root missing - defaulting to 'www'\n");
	  strcpy(&serv_conf.www_root[0],"www");
	}
	if(serv_conf.keep_alive_timeout==0){
	  printf("Warning: keep-alive timout missing - defaulting to 500000\n");
	  serv_conf.tls_port=8081;
	}

}

void set_user_proxy(char* cmd){

	char* ptr = strtok(cmd,"=");
	if(ptr!=NULL){
	  user_proxy_target = (proxy_target*)malloc(sizeof(proxy_target));
	  strcpy(user_proxy_target->host,ptr);
	  ptr=strtok(NULL,":");
	  if(ptr!=NULL){
	    strcpy(user_proxy_target->proxy_host,ptr);
	    user_proxy_target->proxy_port=atoi(strtok(NULL,":"));
	  }
	  if((ptr=strtok(NULL,":"))!=NULL){
	    sprintf(user_proxy_target->type,"%s", ptr);
	  }
	  else {
	    sprintf(user_proxy_target->type,"%s","http");
	  }
	}

}

void usage(){

	printf("\nCornelia Web Server (c) CrazedoutSoft 2022\n\n");
	printf("usage: cornelia_d [OPTIONW]\n");
	printf("example: cornelia -tls -c myconf.conf -p 8080\n\n");
	printf("-http\tHTTP (Default)\n");
	printf("-ssl\tHTTP/SSL\n");
	printf("-tls\tHTTP/TLS\n");
	printf("-c\t<conf_file>\n");
	printf("-p\t<server_port>\n");
	printf("-ssl\t<server_ssl_port>\n");
	printf("-tsl\t<server_tsl_port>\n");
	printf("-i \tprints config\n");
	printf("-d \tdebug mode\n");
	printf("-reload\t Reload conf on change\n");
	printf("-head \tprint request headers to stdout\n");
	printf("-proxy\tset up proxy target [-proxy:<local_host>=<remote_host>:<port>:<type> (http or ssl)]\n");
	printf("-uep\tset up endpoint [-uep:/myendpoint%%{\\\"name\\\":\\\"value\\\"}%%application/json\n");
	printf("                        [-uep:/myendpoint%%file:myjson.js%%application/json\n");
	printf("                        Content-Type defaults to 'application/json' if omitted.\n");
	printf("--help prints this message\n\n");

}


int main(int args, char* argv[]){

	int  user_port = 0;
	int  user_ssl_port=0;
	int  user_tsl_port=0;
	int  dump_c=0;
	char *ptr;
	char dir[1024];
	int use_ssl=0;
	int use_tls=0;
	user_endpoint* uep = NULL;

	get_work_dir(dir,1024);

	memset(local_proto,0,sizeof(local_proto));

	if(getenv("CORNELIA_HOME")==NULL){
	  printf("env CORNELIA_HOME should be set to cornelia_d workdir\n");
	  printf("export CORNELIA_HOME=<dir of cornelia_d>\n");
	  printf("Asuming: %s - Lets's try it..\n", dir);
	  setenv("CORNELIA_HOME",dir,1);
	}

	if(args>1){

	 for(int i = 0; i < args; i++){
	  if(strcmp(argv[i],"-c")==0) {
		if(i+1>=args){
		  printf("Bad conf file:<null>\nTry --help\n");
		  return -1;
		}
		strcpy(&conf_file[0], argv[i+1]);
	  }
	  else if(strcmp(argv[i],"-p")==0 && i<args-1 && i+1<args) {
		user_port=atoi((ptr=argv[i+1]));
		if(user_port==0) {
		 printf("Bad port:%s\nTry --help\n", ptr);
		 return -1;
		}
	  }
	  else if(strcmp(argv[i],"-ssl_p")==0 && i<args-1 && i+1<args) {
                user_ssl_port=atoi((ptr=argv[i+1]));
                if(user_ssl_port==0) {
                 printf("Bad ssl port:%s\nTry --help\n", ptr);
                 return -1;
                }
          }
	  else if(strcmp(argv[i],"-tsl_p")==0 && i<args-1 && i+1<args) {
                user_tsl_port=atoi((ptr=argv[i+1]));
                if(user_tsl_port==0) {
                 printf("Bad ssl port:%s\nTry --help\n", ptr);
                 return -1;
                }
          }
	  else if(strcmp(argv[i],"-reload")==0) auto_reload_conf=1;
	  else if(strcmp(argv[i],"-head")==0) dump_req=1;
	  else if(strcmp(argv[i],"-ssl")==0) use_ssl=1;
	  else if(strcmp(argv[i],"-tls")==0) use_tls=1;
	  else if(strcmp(argv[i],"-i")==0) dump_c=1;
	  else if(strcmp(argv[i],"-d")==0) c_debug=1;
	  else if(strstr(argv[i],"-uep")!=NULL){
	    uep = get_user_endpoint(strstr(argv[i],"-uep"));
	  }
	  else if(strstr(argv[i],"-proxy")!=NULL){
	    set_user_proxy(&argv[i][7]);
	  }
	  else if(strstr(argv[i],"-help")!=NULL) {
		usage();
		return 0;
	  }
	 }
	}

	check_shutdown(0);
	if(read_http_responses()==-1) return -1;

	memset(&serv_conf,0,sizeof(server_conf));
	memset(&a_conf,0,sizeof(auth_conf));
	if(c_debug) printf("\nDebug mode is on\n");

	if(init_conf(&conf_file[0], &serv_conf)>-1){

	  if(uep!=NULL) add_user_endpoint(uep,&serv_conf);

	  // Add User proxy target if exists.
	  if(user_proxy_target!=NULL){
	    int n=0;
	    while(serv_conf.v_proxys[n]!=NULL){
	      n++;
	    }
	    serv_conf.v_proxys[n] = (proxy_target*)malloc(sizeof(proxy_target));
	    memcpy(serv_conf.v_proxys[n],user_proxy_target,sizeof(proxy_target));
	    serv_conf.v_proxys[n+1]=NULL;
	  }


  	  if(user_port>0) serv_conf.port=user_port;
  	  if(user_ssl_port>0) serv_conf.ssl_port=user_ssl_port;
  	  if(user_tsl_port>0) serv_conf.tls_port=user_tsl_port;
	  if(dump_c) {print_server_conf(&serv_conf);}
	  else {
	   check_conf(use_ssl, use_tls);
	   if(use_ssl==0 && use_tls==0){
		strcpy(local_proto, HTTP_PROTO);
		init_server();
	   }else if(use_ssl){
		#ifndef NO_SSL
		strcpy(local_proto, SSL_PROTO);
		init_ssl_server(&serv_conf);
		#endif
		#ifdef NO_SSL
		printf("This Cornelia Web Server was compiled without SSL.(make no_ssl)\n");
		#endif
	   }else if(use_tls){
		#ifndef NO_SSL
		strcpy(local_proto, TLS_PROTO);
		init_tls_server(&serv_conf);
		#endif
		#ifdef NO_SSL
		printf("This Cornelia Web Server was compiled without TLS. (make no_ssl)\n");
		#endif
	   }
          }
	}

	if(user_proxy_target!=NULL) free(user_proxy_target);
	free(bad_request);
	free(internal_server_error);
	free(forbidden);
	free(unauthorized);

 return 0;
}

