#include <stdio.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define N_DEVS 6

int main() {
  struct pollfd pollfds[N_DEVS];
  char filename[20];

  for(int i=0;i<N_DEVS;i++) {
    sprintf(filename, "/dev/shofer%d", i);
    int fd = open(filename, O_RDONLY);

    if(fd > -1) {
      pollfds[i].fd = fd;
      pollfds[i].events = POLLIN;
      printf("opened device %s\n", filename);
    } else {
      printf("error opening %s\n", filename);
      exit(1);
    }
  }

  while(1) {
    poll(pollfds, N_DEVS, -1);

    for(int i=0;i<N_DEVS;i++) {
      struct pollfd fd = pollfds[i];
      if(fd.revents == POLLIN) {
        sprintf(filename, "/dev/shofer%d", i);
        printf("%s ready\n", filename);

        char c;
        read(fd.fd, &c, 1);
        printf("got %c from %s\n", c, filename);
        fd.revents = 0;
      }
    }
  }
}