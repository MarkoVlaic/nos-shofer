#include <stdio.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define N_DEVS 6
#define PERIOD 5 //s

int main() {
  struct pollfd pollfds[N_DEVS];
  char filename[20];

  for(int i=0;i<N_DEVS;i++) {
    sprintf(filename, "/dev/shofer%d", i);
    int fd = open(filename, O_WRONLY);

    if(fd > -1) {
      pollfds[i].fd = fd;
      pollfds[i].events = POLLOUT;
      printf("opened device %s\n", filename);
    } else {
      printf("error opening %s\n", filename);
      exit(1);
    }
  }

  srand(time(NULL));

  while(1) {
    int ready = poll(pollfds, N_DEVS, 0);
    if(ready == 0) {
      continue;
    }

    int selected_dev = rand() % ready + 1;
    int i = 0;
    while(selected_dev > 1) {
      if(pollfds[i].revents == POLLOUT) {
        selected_dev--;
      }
      i++;
    }

    struct pollfd fd = pollfds[i];
    sprintf(filename, "/dev/shofer%d", i);
    printf("%s ready\n", filename);

    char c = 'A' + rand() % ('Z' - 'A');
    write(fd.fd, &c, 1);
    printf("written %c to %s\n", c, filename);
    fd.revents = 0;

    sleep(PERIOD);
  }
}