#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define SERVER_PORT 3000
#define MAX_LINE 4096
#define MAX_PROP_LEN 256
#define EXPOSED_FOLDER "public"

// Parts stolen from https://www.youtube.com/watch?v=esXw4bdaZkc
int main(void) {
  int fdserver;
  int fdclient;

  char method[MAX_PROP_LEN];
  char version[MAX_PROP_LEN];
  char uri[MAX_PROP_LEN];

  struct stat sbuf;
  struct sockaddr_in servaddr;
  char recvline[MAX_LINE + 1];

  fdserver = socket(AF_INET, SOCK_STREAM, 0);

  if (!fdserver) {
    fprintf(stderr, "[ERROR] Failed to initialize socket: %m\n");
    exit(EXIT_FAILURE);
  }

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERVER_PORT);

  if (bind(fdserver, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    fprintf(stderr, "[ERROR] Failed to bind socket: %m\n");
    exit(EXIT_FAILURE);
  }

  if (listen(fdserver, 10) < 0) {
    fprintf(stderr, "[ERROR] Failed to listen socket: %m\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    printf("Waiting for connection on port %d\n", SERVER_PORT);
    fflush(stdout);
    fdclient = accept(fdserver, NULL, NULL);

    if (!fdclient) {
      fprintf(stderr, "[ERROR] Failed to connect: %m\n");
      exit(EXIT_FAILURE);
    }

    FILE *stream = fdopen(fdclient, "r+");
    if (!stream) {
      fprintf(stderr, "[ERROR] fdopen: %m\n");
      exit(EXIT_FAILURE);
    }

    memset(recvline, 0, MAX_LINE);

    fgets(recvline, MAX_LINE - 1, stream);
    sscanf(recvline, "%s %s %s\n", method, uri, version);

    if (strcmp(method, "GET") == 0) {
      static char moduri[MAX_PROP_LEN];

      memset(moduri, 0, MAX_PROP_LEN);
      strcpy(moduri, "./" EXPOSED_FOLDER);
      strcat(moduri, uri);

      int statuscode = 200;

      // check if file or directory does exist
      if (stat(moduri, &sbuf) < 0) {
        // if it doesn't try with an html extension
        memset(moduri, 0, MAX_PROP_LEN);
        strcpy(moduri, "./" EXPOSED_FOLDER);
        strcat(moduri, uri);
        strcat(moduri, ".html");
      } else {
        // if it does check if is not a file
        if (!S_ISREG(sbuf.st_mode)) {
          // if it is not a directory append an index.html at the end
          if (moduri[strlen(moduri) - 1] != '/')
            strcat(moduri, "/");
          strcat(moduri, "index.html");
        }
      }

      // check again whether the file exists
      if (stat(moduri, &sbuf) < 0) {
        // if it doesn't change the path to the 404 page
        statuscode = 404;
        fprintf(stderr, "[ERROR] 404 File %s does not exist\n", moduri);
        memset(moduri, 0, MAX_PROP_LEN);
        strcpy(moduri, "./" EXPOSED_FOLDER);
        strcat(moduri, "/404.html");
      }

      // check if the 404 page exists if not exit the program
      if (stat(moduri, &sbuf) < 0) {
        fprintf(stderr, "[ERROR] 404 page does not exist\n");
        exit(EXIT_FAILURE);
      }

      fprintf(stream, "HTTP/1.1 %d OK\n", statuscode);
      fprintf(stream, "Server: serverfromscratch\n");
      fprintf(stream, "Content-Type: text/html\n");
      fprintf(stream, "\r\n\r\n");
      fflush(stream);

      int fd = open(moduri, O_RDONLY);
      void *p = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
      fwrite(p, 1, sbuf.st_size, stream);
      munmap(p, sbuf.st_size);
    }

    fclose(stream);
    close(fdclient);
  }
}
