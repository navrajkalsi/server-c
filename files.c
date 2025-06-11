#include <stdio.h>
#include <stdlib.h>
int main(void) {

  FILE *file = fopen("demo2.c", "r");

  // char buffer[2048];
  // fread(buffer, sizeof(char), sizeof(buffer) - 1, file);
  // buffer[2048] = "/0";

  // Going to the end of the file
  fseek(file, 0, SEEK_END);
  long pos = ftell(file);
  rewind(file);

  printf("Size of file: %ld\n", pos);
  char buffer[pos + 1];
  fread(buffer, sizeof(char), pos, file);
  buffer[pos] = '\0';
  // buffer[pos + 1] = "/0";
  printf("Buffer: %s", buffer);
  fclose(file);

  return 0;
};
