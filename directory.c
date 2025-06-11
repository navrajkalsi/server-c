#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

int main(void) {

  struct dirent *ep;

  DIR *dp = opendir("./");

  if (dp) {
    while (ep = readdir(dp))
      puts(ep->d_name);
    (void)closedir(dp);
  }

  return 0;
}
