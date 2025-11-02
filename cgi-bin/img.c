
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* getenv(const char* name);

int main(int args, char* argv[]){


   printf("Set-Cookie: crazedout=soft\n");
   printf("Content-Type: image/png\r\n\r\n");
//   printf("Content-Type: text/text\r\n\r\n");


   char* buffer = NULL;
   FILE* fd = fopen("/home/nrkfrr/GIT/corneliaws/www/corn2.png","rb");
   if(fd!=NULL){
     fseek(fd,0,SEEK_END);
     int size = ftell(fd);
//     printf("%d\n",size);
     fseek(fd,0,SEEK_SET);
     buffer = (char*)malloc(size);
     fread(buffer,1,size,fd);
     fwrite(buffer,1,size,stdout);
     fclose(fd);
     free(buffer);
   }


 return 0;
}
