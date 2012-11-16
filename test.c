#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


// O_RDONLY		O_WRONLY		O_RDWR
int main(void)
{
  int fh;
  char buffer[100];
  char writer[100] = "world";
  char writer2[100] = "hello";
  int result;  

  printf("FILE\n");
  fh = open("read.txt", O_RDONLY);
  result = read(fh, buffer, 20);
  printf("read: %d\n", result);
  close(fh);

  fh = open("write.txt", O_WRONLY);
  result = write(fh,  writer,  strlen(writer));
  printf("write: %d\n", result);
  close(fh);
  
  fh = open("write.txt", O_RDONLY);
  result = read(fh, buffer, strlen(writer));
  printf("read: %d (%s)\n", result, buffer);  
  close(fh);

  
  printf("\n\nSECRET\n");

  memset(&buffer[0], 0, sizeof(buffer));
  fh = open("/dev/secret", O_RDONLY);
  result = read(fh, buffer, 3);
  printf("Read 3 \nresults: %d \tbuffer:%s\n", result, buffer);
  close(fh);  

  fh = open("/dev/secret", O_WRONLY);
  result = write(fh,  writer,  strlen(writer));
  printf("Write %d \nresult: %d\n",strlen(writer), result);
  result = write(fh,  writer,  strlen(writer2));
  printf("Write %d \nresult: %d\n",strlen(writer2), result);
  close(fh);  

  memset(&buffer[0], 0, sizeof(buffer));
  fh = open("/dev/secret", O_RDONLY);
  result = read(fh, buffer, 5);
  printf("Read 5   \nresults: %d \tbuffer:%s\n", result, buffer);
  memset(&buffer[0], 0, sizeof(buffer));
  result = read(fh, buffer, 5);
  printf("read: 5 \nresult: %d \t buffer:%s\n", result, buffer);
  close(fh);  

  
  return 1;
}