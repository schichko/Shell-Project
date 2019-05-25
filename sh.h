
#include "get_path.h"

int pid;
int sh( int argc, char **argv, char **envp);
char *which(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
void list ( char *dir );
void printenv(char **envp);
int is_file_exec(char *command);
void freeList(struct pathelement* head);
void *watchmail(void *arg);
void *watchuser(void *arg);

void* pthread_exec_path(void *arg);
void* pthread_exec_external(void *arg);

/* struct for sending args to pthreads */
struct threadargs {
  char** args; /* cli args */
  char* command; /* path to command */
  char** envp;
};

struct maillist{
  char *str;
  pthread_t id;
  struct maillist *next;
};
  
struct strlist{
  char *str;
  int status;
  struct strlist *next;
  struct strlist *prev;
};


#define PROMPTMAX 32
#define MAXARGS 10
#define MAXLINE  128
#define PATH_MAX 4096
