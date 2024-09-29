#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <stdbool.h>
#include <ragnar/api.h>

#define INOTIFY_BUFFER_SIZE 8192

enum {
  FD_POLL_INOTIFY = 0,
  FD_POLL_MAX
};

#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))

static int32_t initlisten(int32_t* monitor, const char* dirpath);
static void terminatelisten(int32_t fd, int32_t monitor);

static bool eventvalid(struct inotify_event *ev, size_t len);
static struct inotify_event* nextevent(struct inotify_event* ev, size_t* len);
static void processevent(struct inotify_event *event);

static void* configmodifylistener(void* arg);

void 
terminatelisten(int32_t fd, int32_t monitor) {
  inotify_rm_watch(fd, monitor);
  close (fd);
}

int32_t 
initlisten(int32_t* monitor, const char* dirpath) {
  int fd;

  if ((fd = inotify_init()) < 0) {
    return -1;
  }

  *monitor = inotify_add_watch(fd, dirpath, IN_MODIFY | IN_CLOSE_WRITE);
  return fd;
}

bool 
eventvalid(struct inotify_event *ev, size_t len) {
  return len >= IN_EVENT_DATA_LEN && 
  ev->len >= IN_EVENT_DATA_LEN &&
  ev->len <= len;
}

struct inotify_event* 
nextevent(struct inotify_event* ev, size_t* len) {
  *len -= ev->len;
  return (struct inotify_event*)(((char*)ev) + ev->len);
}

void 
processevent(struct inotify_event *event) {
  if(event->len > 0 &&
    strcmp(event->name, "ragnar.cfg") == 0 && 
    event->mask & IN_CLOSE_WRITE) {
    rg_cmd_reload_config();
  }
  return;
}

void* 
configmodifylistener(void* arg) {
  (void)arg;

  int inotify_fd;
  struct pollfd fds[FD_POLL_MAX];

  static int32_t monitor;

  const char* home = getenv("HOME");
  const char* relpath = "/.config/ragnarwm/";
  char* configdir = malloc(strlen(home) + strlen(relpath) + 2);
  sprintf(configdir, "%s%s", home, relpath);

  if ((inotify_fd = initlisten(&monitor, configdir)) < 0) {
    fprintf (stderr, "error: could not initialize inotify.\n");
    exit (EXIT_FAILURE);
  }


  fds[FD_POLL_INOTIFY].fd = inotify_fd;
  fds[FD_POLL_INOTIFY].events = POLLIN;

  while(1) {
    if (poll(fds, FD_POLL_MAX, -1) < 0) {
      fprintf (stderr, "error: could not poll(): '%s'.\n", strerror(errno));
      exit (EXIT_FAILURE);
    }

    if (fds[FD_POLL_INOTIFY].revents & POLLIN) {
      char buf[INOTIFY_BUFFER_SIZE];
      size_t len = read(fds[FD_POLL_INOTIFY].fd, buf, INOTIFY_BUFFER_SIZE);

      if (len > 0) {
        struct inotify_event *event;
        event = (struct inotify_event *)buf;
        while (eventvalid(event, len)) {
          processevent(event);
          event = nextevent(event, &len);
        }
      }
    }
  }
  terminatelisten(inotify_fd, monitor);
  pthread_exit(NULL);  
}

int main (void)
{
  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, configmodifylistener, NULL)) {
    fprintf(stderr, "error: failed to created listener thread.\n");
    exit(EXIT_FAILURE);
  }  

  while (1);

  return EXIT_SUCCESS;
}


