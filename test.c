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
  int result;
  printf("Len: %d\n", strlen(writer));
  

  printf("FILE\n");
  fh = open("read.txt", O_RDONLY);
  result = read(fh, buffer, 20);
  printf("read: %d\n", result);

  fh = open("write.txt", O_WRONLY);
  result = write(fh,  writer,  strlen(writer));
  printf("write: %d\n", result);
  
  fh = open("write.txt", O_RDONLY);
  result = read(fh, buffer, strlen(writer));
  printf("read: %d (%s)\n", result, buffer);  

  
  printf("\n\nSECRET\n");

  fh = open("/dev/secret", O_RDONLY);
  result = read(fh, buffer, 3);
  printf("read: %d (%s)\n", result, buffer);

  fh = open("/dev/secret", O_WRONLY);
  result = write(fh,  writer,  strlen(writer));
  printf("fh: %d\twrite: %d\n", fh, result);

  fh = open("/dev/secret", O_RDONLY);
  result = read(fh, buffer, strlen(writer));
  printf("read: %d (%s)\n", result, buffer);

  fh = open("/dev/secret", O_RDONLY);
  result = read(fh, buffer, 3);
  printf("read: %d (%s)\n", result, buffer);

  
  return 1;
}