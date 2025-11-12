
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


int client_connect(char* clientIP, int port){

    int status, client_fd;
    struct sockaddr_in serv_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, clientIP, &serv_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
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

void domain_to_ip(char* dest, const char* domain){

        struct hostent *host_info;
        struct in_addr *address;

        host_info = gethostbyname(domain);
        address = (struct in_addr *) (host_info->h_addr_list[0]);
        strcpy(dest, inet_ntoa(*address));
}

int main(int args, char* argv[]){

	int r = 0;
	char buffer[1025];
	char cmd[256];
	char tmp[256];
	char host[128];
	char path[256];
	char* ptr;
	int port=80;
	char ip[128];

	memset(host,0,sizeof(host));
	memset(path,0,sizeof(path));
	memset(buffer,0,sizeof(buffer));

	if(args<2) {
	 printf("usage <url>\n");
	}

	if((ptr=strstr(argv[1],"http://"))!=NULL){
	  strcpy(cmd,ptr+7);
	}else{
	  strcpy(cmd,argv[1]);
	}

	strcpy(tmp,cmd);
	ptr=strtok(tmp,":/");
	if(ptr!=NULL){

	  strcpy(host,ptr);
	  if((ptr=strtok(NULL,":/"))!=NULL){
	   if(atoi(ptr)!=0) port=atoi(ptr);
	   while((ptr=strtok(NULL,"/"))!=NULL) {
		strcat(path,"/");
		strcat(path,ptr);
	   }
	  }

	}

	strcpy(ip,host);
	strcpy(tmp,host);
	if((ptr=strtok(tmp,"."))!=NULL){
	 if(!atoi(ptr)){
	  domain_to_ip(cmd,host);
  	  strcpy(ip,cmd);
	 }
	}

	if(strlen(path)==0) strcat(path,"/");

	if(port==80){
	  sprintf(buffer,"GET %s HTTP/1.1\nHost: %s\n\n", path,host);
	}else{
	  sprintf(buffer,"GET %s HTTP/1.1\nHost: %s:%d\n\n", path,host,port);
	}

	int fd = client_connect(ip,port);
	if(fd==-1) {
	  printf("Error\n");
	  return -1;
	}

	r=write(fd,buffer,strlen(buffer));

	while((r=read(fd,buffer,1024))){
	  printf("%d\n",r);
 	  buffer[r]='\0';
	  printf("%s",buffer);
	}
	close(fd);

 return 0;
}
