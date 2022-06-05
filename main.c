#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 3000
#define MAX_LINE 4096

char *read_file(char *file_path) {
  FILE *file = fopen(file_path, "r");

  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(sizeof(char) * (file_size + 1));
  fread(buffer, sizeof(char), file_size, file);
  buffer[file_size] = '\0';

  fclose(file);

  return buffer;
}

// Stolen from
// https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) {
  char *result;  // the return string
  char *ins;     // the next insert point
  char *tmp;     // varies
  int len_rep;   // length of rep (the string to remove)
  int len_with;  // length of with (the string to replace rep with)
  int len_front; // distance between rep and end of last rep
  int count;     // number of replacements

  // sanity checks and initialization
  if (!orig || !rep)
    return NULL;
  len_rep = strlen(rep);
  if (len_rep == 0)
    return NULL; // empty rep causes infinite loop during count
  if (!with)
    with = "";
  len_with = strlen(with);

  // count the number of replacements needed
  ins = orig;
  for (count = 0; tmp = strstr(ins, rep); ++count) {
    ins = tmp + len_rep;
  }

  tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

  if (!result)
    return NULL;

  // first time through the loop, all the variable are set correctly
  // from here on,
  //    tmp points to the end of the result string
  //    ins points to the next occurrence of rep in orig
  //    orig points to the remainder of orig after "end of rep"
  while (count--) {
    ins = strstr(orig, rep);
    len_front = ins - orig;
    tmp = strncpy(tmp, orig, len_front) + len_front;
    tmp = strcpy(tmp, with) + len_with;
    orig += len_front + len_rep; // move to next "end of rep"
  }
  strcpy(tmp, orig);
  return result;
}

// Parts stolen from https://www.youtube.com/watch?v=esXw4bdaZkc
int main(void) {
  int listenfd;
  int connfd;
  int n;
  const char *resheader = ""
                          "HTTP/1.1 200 OK\n"
                          "Server: serverfromscratch\n"
                          "Content-Type: text/html\n";
  char *pgtemp = read_file("./index.html");

  struct sockaddr_in servaddr;
  uint8_t buff[MAX_LINE + 1];
  uint8_t recvline[MAX_LINE + 1];

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error initializing socket.");
    exit(EXIT_FAILURE);
  }

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERVER_PORT);

  if ((bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
    printf("Error while binding socket.");
    exit(EXIT_FAILURE);
  }

  if ((listen(listenfd, 10)) < 0) {
    printf("Error while listening socket.");
    exit(EXIT_FAILURE);
  }

  int visited = 0;

  while (1) {
    printf("Waiting for connection on port %d\n", SERVER_PORT);
    fflush(stdout);
    connfd = accept(listenfd, NULL, NULL);

    memset(recvline, 0, MAX_LINE);

    while ((n = read(connfd, recvline, MAX_LINE - 1)) > 0) {
      printf((char *)recvline);
      if (recvline[n - 1] == '\n') {
        break;
      }
      memset(recvline, 0, MAX_LINE);
    }

    if (n < 0) {
      printf("Error reading message");
      exit(EXIT_FAILURE);
    }

    visited++;
    char numstr[32];
    sprintf(numstr, "%d", visited);

    char *pghtml = str_replace(pgtemp, "{{ count }}", numstr);
    snprintf((char *)buff, sizeof(buff), "%s\r\n\r\n%s", resheader, pghtml);
    write(connfd, (char *)buff, strlen((char *)buff));
    close(connfd);
  }
}