#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utmpx.h>
#include<fcntl.h> 
#include<errno.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include "sh.h"
#include <glob.h>

struct strlist *watchuserhead;
struct maillist *watchmailhead;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int sh( int argc, char **argv, char **envp )
{
  glob_t glob_buffer;                                 
  char *prompt = calloc(PROMPTMAX, sizeof(char));     
  char *commandline = calloc(MAXLINE, sizeof(char));   
  char *command, *arg, *commandpath, *p, *pwd, *owd, *statusL;
  char **args = calloc(MAXARGS, sizeof(char*));
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  char *prevdir;
  char *tmp = NULL;
  struct pathelement *pathlist;
  char	buf[MAXLINE];
  char *ptr;
  const char s[2] = " ";
  char *token;
  char* filepath ;

  int watchthread = 0;
  int mailthread = 0;

  int noclobber = 0;

	watchuserhead = NULL;
  watchmailhead = NULL;


  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						  out with*/
  prevdir = getcwd(NULL, PATH_MAX+1);
  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
  {
    perror("getcwd");
    exit(2);
  }
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';
  /* Put PATH into a linked list */
  pathlist = get_path();
  printf("%s:%s» ", prompt, owd);   //Prints original command line (First one)
  fflush(NULL);

  //Starts Command Prompt (Infinite loop until exit)
  //again:
  
  while (1) {
    
        statusL = fgets(buf, MAXLINE, stdin) != NULL;
        if (buf[strlen(buf) - 1] == '\n'){
          buf[strlen(buf) - 1] = 0; /* replace newline with null */
        }
        // if (statusL == NULL){
        //   printf("^D\n");
        //   goto again;
        // }else{
          
        // }
          
        //Gets the buffer aka commands being passed in
      
      
        //Reinitilizes our argument count, tokens and array (for some reason i did not use command or prompt but I made a new 
        //array called array, i realize i couldve used one of the ones from above however i did array and I could have gone
        //Back and replaced it but it has the same functionality)
        int argcount = 0;
        char *p = strtok (buf, " "); //tokens are going to be based on spaces inbetween cmds 
        char *array[PROMPTMAX];
        i = 0; //Makes sure i is set back to 0
        //globfree(&glob_buffer);
        for(i; i<PROMPTMAX;i++){
          array[i] = NULL;  //Initialize array
        }

        argcount = 0; //saftey for setting arg count to 0

        while (p != NULL)
        {
            array[argcount++] = p;  //Tolkenizes the array as well as gives us the count of arguments 
            p = strtok (NULL, " ");
        }

        i = 0;  //Makes sure i is set back to 0
        int match_count; //The number of matches to our wild card (Ex *.c with g.c, f.c and h.h would be 2)
        while(array[i] != NULL){
          if (strstr(array[i], "*") != NULL || strstr(array[i], "?") != NULL) { //If we have one of our wild card characters in our string we go on
            glob(array[i] , 0 , NULL , &glob_buffer );  //Glob the token in our line that has one or both of the wildcard characters
            match_count = glob_buffer.gl_pathc; //Set the amount of matches
            int j=0;
            for(j;j<match_count;j++){
              array[i] = glob_buffer.gl_pathv[j]; //Add those files to our array acting like they were placed there previously
              i++; 
            }
          }
          i++; 
        }
        i=0; //Set i back to 0

        if(argcount == 0){
          //If we have no arguments we return (spamming return on the keyboard)
        }

        else if (strcmp(buf, "exit") == 0) {   /* built-in command exit */
          printf("Executing built-in command %s\n", array[0]);
          //Frees all stuff on the heap
          free(prompt);
          free(commandline);
          free(args);
          free(pwd);
          freeList(pathlist);
          if(prevdir != NULL){
            free(prevdir);
          }
          free(owd);
          //globfree(&glob_buffer);
          if(tmp != NULL){
            free(tmp);
          }
          if(filepath != NULL){
            free(filepath);
          }
          exit(0);
        }

        else if (strcmp(array[0],"which") == 0){  /* built-in command which */
          printf("Executing built-in command %s\n", array[0]);
          if(array[1] == NULL || strcmp(array[1],"") ==0 || strcmp(array[1]," ") ==0){  //If we don't pass which any args we do nothing
            printf("which: Too few arguments\n");
          }
          else if((strcmp(array[1],"exit")) ==0 || (strcmp(array[1],"which")) ==0 || (strcmp(array[1],"where")) ==0 || (strcmp(array[1],"cd")) ==0 || (strcmp(array[1],"pwd")) ==0 || (strcmp(array[1],"list"))==0 || (strcmp(array[1],"pid"))==0 || (strcmp(array[1],"kill"))==0 || (strcmp(array[1],"prompt"))==0 || (strcmp(array[1],"printenv"))==0 || (strcmp(array[1],"setenv"))==0 ){
            printf("%s: shell built in command\n",array[1]);  //This is checking if any of the commands we have built in are being passed in as an argument for which
          }
          else{
            i = 1;
            while(array[i] != NULL){ //This is for checking for multiple arguments (for example which ls gdb) we want to which all of our arguments
              command = which(array[i],pathlist); //Does the actual which and prints
              if(command != NULL){  //Check for a nullity 
                printf("%s\n",command); //Prints it
                free(command);
              }
              i++;
            }
          }
        }

        else if (strcmp(array[0],"where") == 0){ /* built-in command where */
          printf("Executing built-in command %s\n", array[0]);
          if(array[1] == NULL || strcmp(array[1],"") ==0 || strcmp(array[1]," ") ==0){  //This is checking for nullity or an empty space as an argument for where which is not a valid argument
              printf("where: Too few arguments\n");
          }
          else if((strcmp(array[1],"exit"))==0 || (strcmp(array[1],"which"))==0 || (strcmp(array[1],"where"))==0 || (strcmp(array[1],"cd"))==0 || (strcmp(array[1],"pwd"))==0 || (strcmp(array[1],"list"))==0 || (strcmp(array[1],"pid"))==0 || (strcmp(array[1],"kill"))==0 || (strcmp(array[1],"prompt"))==0 || (strcmp(array[1],"printenv"))==0 || (strcmp(array[1],"setenv"))==0){
             printf("%s: shell built in command\n",array[1]); //This is checking if any of the commands we have built in are being passed in as an argument for where
          }
          else{
            i = 1;
            while(array[i] != NULL){  //Similar to which we want to do a safty check to make sure our arugment is not equal to null while we are looping through all of the arguments
              command = where(array[i],pathlist);
              if(command != NULL){  //Safety check
                printf("%s\n",command);
                free(command);
              }
              i++;
            }
          }
        }

        else if (strcmp(array[0],"cd") == 0){ /* built-in command cd */
          printf("Executing built-in command %s\n", array[0]);
          if(array[2] != NULL){ //Check for too many arguments, we can not cd to multiple directorys at once
              printf(" Too many Arguments\n");
              for(int i = 0; i<PROMPTMAX;i++){
                array[i] = NULL;  //Resets arguments to NULL
              }
          }
          else{
            free(owd);  //Since we are changing the directory we want to free owd and pwd since they will be realocated 
            free(pwd);
            fflush(stdout);
            if(array[1] == NULL || strcmp(array[1],"") ==0 || strcmp(array[1]," ") ==0){  //case for cd (goes to home directory which has been previously set)
              if(prevdir != NULL ){
                free(prevdir);
              }
              prevdir = getcwd(NULL, PATH_MAX+1);
              chdir(homedir);
            }
            else if(strcmp(array[1],"-") == 0 ){   //Goes to the PREVIOUS directory 
              if(prevdir == NULL){
                tmp = getcwd(NULL, PATH_MAX+1);
                prevdir = tmp;
              }
              else{
                tmp = getcwd(NULL, PATH_MAX+1);;
                chdir(prevdir); 
                free(prevdir);
                //free(tmp);
                prevdir = tmp;
              }
            }
            else{
              fflush(stdout);
              if(prevdir != NULL ){
                free(prevdir);
              }
              fflush(stdout);
              prevdir = getcwd(NULL, PATH_MAX+1);
              if(chdir(array[1]) != 0){ //Checks for going to a directory, this will check if the directory even exists 
                printf(" Directory Does not Exist\n");
              }
            }
            if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL ){  //Used for printing out the new directory and prompoted
                  perror("getcwd");
                  exit(2);
            }
            owd = calloc(strlen(pwd) + 1, sizeof(char));  //Prints out the new prompt
            memcpy(owd, pwd, strlen(pwd));
          }
        }

        else if (strcmp(buf, "pwd") == 0) {   /* built-in command pwd */
          printf("Executing built-in command %s\n", array[0]);
          ptr = getcwd(NULL, 0);  //Gets the current working directory 
          printf("CWD = [%s]\n", ptr);  //Prints it out with the format specified 
          free(ptr);
        }

        else if(strcmp(array[0],"list")==0){    /* built-in command list */
          printf("Executing built-in command %s\n", array[0]);
          if (array[1] == NULL) { /* no args */
            list(".");  //If we have no arugments we print the current directory 
          }
          else{
            int i =1;
            while (array[i] != NULL){ //This is the case for us having 1 or more arugments
              printf("%s: \n",array[i]);  //We print the name of the directory that we are attemping to list
              list(array[i]); //We list the directoy
              printf("\n");
              i++;  //We move on to the next argument
            }
          }
        }

        else if (strcmp(array[0], "pid") == 0) {   /* built-in command pid */
          printf("Executing built-in command %s\n", array[0]);
          printf("PID: [%d]\n",getpid()); //Gets the process ID of the current process that is runninng
        }

         else if (strcmp(array[0], "kill") == 0) {   /* built-in command kill */
          printf("Executing built-in command %s\n", array[0]);  
          if (array[1] == NULL) { /* no args */
            printf("Not enough arguments\n"); //We need to specify what process we are trying to kill
          }
          else if(array[1][0] == '-'){  //In this case we are specifying what signal we are sending to the process
            if((array[1] +1 )!=NULL && (atoi(array[1] + 1)!=0)) { //Check that we have a value after -
              int killcmd = atoi(array[1] +1);  //Turing our command into an int
              if(killcmd > 64 || killcmd <1){ //there are only 64 kill commands so we must choose one of these
                printf("Not Valid Kill Command\n");
              }
              else{
                printf("%d",killcmd);
                int i = 1;
                while(array[i] != NULL){  //The case for us having multiple processes to kill, we loop through this, if we only have one this will only happen one time
                  int killpid = atoi(array[i]); //Turns our argument into an integer so we can use it for the kill command
                  if(kill(killpid, killcmd) == -1){
                    printf("kill: %s: no such pid\n", array[i]);
                  }
                  i++;  //Go to the next value in the array
                }
              }
            }
            else{
              printf("Not Valid Kill Command\n");
            }
          }
          else{ //In this case we are just killing a process with kill command 9...
              int i = 1; 
              while(array[i] != NULL){  //However we are still looping through our arguments like we have done for the previous situatio
                int killpid = atoi(array[i]);
                if(kill(killpid, 9) == -1){
                  printf("kill: %s: no such pid\n", array[i]);
                }
                i++;  //Goes to the next value in our argument array
              }
          }
        }
       
        else if(strcmp(array[0], "prompt") == 0) {  /* built-in command prompt */
          printf("Executing built-in command %s\n", array[0]);
          if (array[1] == NULL) { /* no args */
            printf("input prompt prefix: "); //If we have no inputs we do not want to ignore it we want to ask the user what it wants its prompt to be
            fgets(prompt, PROMPTMAX, stdin);  //Get the prompt
            prompt[strlen(prompt)-1] = '\0';  //Change the prompt
          } 
          else{
            strcpy(prompt, array[1]); //If the user gives us a prompt along with its argument we use that prompt
          }
        }

        else if (strcmp(array[0], "printenv") == 0) {   /* built-in command printenv */
          printf("Executing built-in command %s\n", array[0]);
          if(array[1] == NULL){ //If our next is equal to null we print our env
              char **env;
              for (env = envp; *env != 0; env++){ 
                printf("%s\n", *env);
              }
          }
          else {  
              char *env;
              int i = 1;
              while((array[i] != NULL) && (((env = getenv(array[i])) != NULL))) { //We make sure that the env that it is asking for is not NULL then we print it
                printf("%s\n", env);
                i++;
              }
          }
        }

        else if(strcmp(array[0], "setenv") == 0) {  /* built-in command setenv */
          printf("Executing built-in command %s\n", array[0]);
          char **env;
          if(array[1] == NULL){ //In the case that there is no argument we can not set the env so we print out the current enviornment
            for (env = envp; *env != 0; env++){
              printf("%s\n", *env);
            }
          }
          else if(array[2] == NULL){
            setenv(array[1], "", 1);  //In the case that we only have 1 argument we set the enviornment provided to ""
            /* special case for home and path */
            if (strcmp(array[1], "PATH") == 0) {
              freeList(pathlist);
              pathlist = get_path();
            } else if(strcmp(array[1], "HOME")) {
              homedir = strcpy(homedir, "/");
            }
          }
          else if(array[3] == NULL){
            setenv(array[1],array[2],1);  //In the case we have 2 arguments we set argument 1 to the enviornment with the value of argument 2 
            /* special case for home and path */
            if (strcmp(array[1], "PATH") == 0) {
              freeList(pathlist);
              pathlist = get_path(); // update path list
            } else if(strcmp(array[1], "HOME")) {
              homedir = strcpy(homedir, array[1]);
            }
          }
          else{ //we can not have more than 2 arguments for set env
            printf("setenv: Too many arguments.\n");
          }
        }


        else if(strcmp(array[0], "watchmail") == 0){  //Watchmail command
          printf("Watch mail initiated\n");
          if(array[2] == NULL){ 
            //two arguemnts, meaning start watching one file
            struct stat buff; //Struct described in the header file
            int exists = stat(array[1], &buff); //Checks if the file exists
            printf("exists? %d\n", exists); //For testing
            if(exists == 0){  //If our file exists we start watching it
              pthread_t mail_t;

              filepath = (char *)malloc(strlen(array[1]));  //The file name
              strcpy(filepath, array[1]);
              printf("%s\n", filepath);
              pthread_create(&mail_t, NULL, watchmail, (void *)filepath);//We call the watchmail function (later in this file) with the filepath argument
              
              if(mailthread == 0 || watchmailhead == NULL){ //If this is the case then we know this is the first mail we are watching so we make it our head
                mailthread = 1;
                watchmailhead = malloc(sizeof(struct maillist));
                watchmailhead->str = malloc(sizeof(strlen(filepath)));
                strcpy(watchmailhead->str, filepath); //Copy the file path and Id to our head of the linked list
                watchmailhead->id = mail_t;
              }
              else{ //This means that we already are watching at least one file
                struct maillist *tmp = watchmailhead; //We use this for traversing the linked list
                while(tmp->next != NULL){ //While we have more values in our linked list we continue
                  tmp = tmp->next;
                }
                tmp->next = malloc(sizeof(struct maillist));  //Normal linked list traversal 
                tmp->next->str = malloc(sizeof(strlen(filepath)));
                strcpy(tmp->next->str, filepath);
                tmp->next->id = mail_t;
              }
            }
          }
          else if(array[2] != NULL){  //This means that we are no longer choosing to watch a certain file
            //Remove head from watchlist
            if(strcmp(watchmailhead->str, array[1]) == 0){
              struct maillist *tmp = watchmailhead;
              watchmailhead = watchmailhead->next;  //Assigns new head
              pthread_cancel(tmp->id); 
              int pj = pthread_join(tmp->id, NULL);
              printf("joined? %d\n", pj);
            }
            else{
              //Remove another node from watchlist
              struct maillist *tmp2 = watchmailhead;
              while(strcmp(tmp2->next->str, args[1]) != 0){
                tmp2 = tmp2->next;
              }
              if(strcmp(tmp2->next->str, args[1]) == 0){
                pthread_cancel(tmp2->next->id);   //Cancels the thread
                printf("joining thread\n");
                int j = pthread_join(tmp2->next->id, NULL); //Joins thread
                printf("joined? %d\n", j);
                tmp2->next = tmp2->next->next;  //Reasigns linked list
              }
              else{ //This is the case that we ask for a file that is not being watched anyway
                printf("File not being watched\n");
              }
            }
          }
        }

        else if(strcmp(array[0], "watchuser") == 0){  //Watch user function
	        printf("Executing built-in command watchuser\n");
          if(watchthread == 0){ 
            printf("Starting watchuser thread...\n");
            watchthread = 1;
            pthread_t watchuser_t;
            pthread_create(&watchuser_t, NULL, watchuser, array[1]);
          }

          if(array[1] == NULL){ //Happens when we have an incorrect number of arguments 
            printf("Usage for watchuser: watchuser [user] [off (optional)]\n");
          }
          else{
            pthread_mutex_lock(&mutex); //Locks for the critical section
            if(array[2] != NULL && strcmp(array[2], "off") == 0){//remove from linked list of users to watch
              struct strlist *tmp = watchuserhead;

              while(tmp != NULL){
                if(strcmp(tmp->str, array[1]) == 0){
            if(tmp->prev == NULL){//deleting the head of the list
              printf("Deleting head %s\n", tmp->str);
              if(tmp->next == NULL){
                watchuserhead = NULL;   
              }
              else{
                watchuserhead = tmp->next;  //Deletes head and reassigns 
                watchuserhead->prev = NULL;
              }
              free(tmp->str); //Frees temps string
              free(tmp);  //Frees temp
              tmp = watchuserhead;
            }
            else{
              printf("Deleting %s\n", tmp->str);
              if(tmp->next == NULL){
                tmp->prev->next = NULL;
              }
              else{
                tmp->prev->next = tmp->next;
              }
              free(tmp->str);
              free(tmp);
              tmp = watchuserhead;
            }
                }
                else{
            tmp = tmp->next;
                }
              }

              printf("Watchuser list is now..\n");
              tmp = watchuserhead;
              while(tmp != NULL){
                printf("User: %s\n", tmp->str);
                tmp = tmp->next;
              }

            }
            else{//add to linked list of users to watch

              //TO-DO: add mutex locks so that watchuser_t doesn't write tmp->status

              if(watchuserhead == NULL){
                printf("Adding new head: %s\n", array[1]);
                struct strlist *tmp;
                tmp = malloc(sizeof(struct strlist));
                tmp->next = NULL;
                tmp->prev = NULL;
                tmp->status = 0;
                tmp->str = malloc((sizeof(char) * strlen(array[1])) + 1);
                strcpy(tmp->str, array[1]);
                watchuserhead = tmp;
              }
              else{
                printf("Adding to list: %s\n", array[1]);
                struct strlist *tmp;
                tmp = malloc(sizeof(struct strlist));
                tmp->str = malloc((sizeof(char) * strlen(array[1])) + 1);
                strcpy(tmp->str, array[1]);
                tmp->next = watchuserhead;
                tmp->prev = NULL;
                tmp->status = 0;
                watchuserhead->prev = tmp;
                watchuserhead = tmp;
              }
            }
            pthread_mutex_unlock(&mutex);
          }
      }

      else if (strcmp(buf, "noclobber") == 0) {   /* built-in command pwd */
          printf("Executing built-in command %s\n", array[0]);
          if(noclobber == 0){ //Switches noclobber on or off
            noclobber = 1;
          }
          else {
            noclobber = 0;
          }
      }

     //For piping someting that has argument | argument(s)
	  else if (array[1] != NULL && strcmp(array[1], "|") == 0) {
		  printf("exectuing built-in command pipe \n");
      int p[2]; //Sets up pipe
      pid_t pid1, pid2;
      char **leftargs  = calloc(MAXARGS, sizeof(char*));  //Left argument calloc, just does to max args size
      char **rightargs  = calloc(MAXARGS, sizeof(char*)); //Right argument calloc, just does to max args size

      leftargs[0] = malloc(1 + (sizeof(char) * strlen(array[0])));  //Left malloc to array[0]
      strcpy(leftargs[0],array[0]); //Copy
      if(array[3] == NULL){ // If the right side only has one argument
        rightargs[0] = malloc(1 + (sizeof(char) * strlen(array[2])));
        strcpy(rightargs[0],array[2]);
      }
      else{//If the right side has two arguments
        rightargs[0] = malloc(1 + (sizeof(char) * strlen(array[2])));
        rightargs[1] = malloc(1 + (sizeof(char) * strlen(array[3])));
        strcpy(rightargs[0],array[2]);
        strcpy(rightargs[1],array[3]);
      }
      if(pipe(p)!=0){
        printf("Error while parsing (pipe)");
        exit(0);
      }
      pid1 = fork();  //Fork left side for doing left side command
      if(pid1==0){
        dup2(p[1],STDOUT_FILENO); //Set up output
        close(p[0]);
        execvp(leftargs[0], leftargs);    
      }
      pid2 = fork();  //Fork rigt side for doing right side command
      if(pid2 == 0){
        dup2(p[0],STDIN_FILENO);  //Set up communication
        close(p[1]);
        execvp(rightargs[0], rightargs);   //Execute command
      }
      close(p[0]);  //Code pipes
      close(p[1]);
      waitpid(pid1,0,WNOHANG|WUNTRACED);  //Processes wait for eachother
      waitpid(pid2,0,WNOHANG|WUNTRACED);
	  }

    else if (array[1] != NULL && strcmp(array[1], "|&") == 0) { //Very similar to | above
		  printf("exectuing built-in command pipe \n");
      int p[2];
      pid_t pid1, pid2;
      char **leftargs  = calloc(MAXARGS, sizeof(char*)); //Left argument calloc, just does to max args size
      char **rightargs  = calloc(MAXARGS, sizeof(char*));//Right argument calloc, just does to max args size

      leftargs[0] = malloc(1 + (sizeof(char) * strlen(array[0])));//Left malloc to array[0]
      strcpy(leftargs[0],array[0]);
      if(array[3] == NULL){
        rightargs[0] = malloc(1 + (sizeof(char) * strlen(array[2])));
        strcpy(rightargs[0],array[2]);
      }
      else{
        rightargs[0] = malloc(1 + (sizeof(char) * strlen(array[2])));
        rightargs[1] = malloc(1 + (sizeof(char) * strlen(array[3])));
        strcpy(rightargs[0],array[2]);
        strcpy(rightargs[1],array[3]);
      }
      if(pipe(p)!=0){
        printf("Error while parsing (pipe)");
        exit(0);
      }
      pid1 = fork();  //Fork process 1
      if(pid1==0){
        dup2(p[1],STDOUT_FILENO); //Set up for output
        dup2(p[1],STDERR_FILENO); // Only difference from | is that we add this
        close(p[0]);
        execvp(leftargs[0], leftargs);    
      }
      pid2 = fork();  //Fork Process 2
      if(pid2 == 0){
        dup2(p[0],STDIN_FILENO);
        close(p[1]);
        execvp(rightargs[0], rightargs);
      }
      close(p[0]);
      close(p[1]);
      waitpid(pid1,0,WNOHANG|WUNTRACED);
      waitpid(pid2,0,WNOHANG|WUNTRACED);
	  }

        else{                           /* external command */
          if(is_file_exec(array[0]) == 0) { /* file is is one that begins with /,. or ./ */
            if(!access(array[0], F_OK)) { /* file exists */
              if(!access(array[0], X_OK)) { /* usr has exec perms */
                	if(strcmp(array[argcount-1], "&") == 0) { /* background execution */
                    printf("Forking into background\n");
                    /* blank '&' arg */
                    // free(array[argcount - 1]);
                    // array[argcount - 1] = NULL;
                    // argsct--;

                    /* build up thread argument struct */
                    struct threadargs *targ = malloc(sizeof(struct threadargs));  //New Struct defined in sh.h
                    
                    targ->args = array; //Gives the array
                    targ->command = command;  //Command
                    targ->envp = envp; //Enviornement
                    
                    /* create pthread */
                    pthread_t td; 
                    int status = pthread_create(&td, NULL, pthread_exec_path, (void*)targ); //Starts running that pthread
                    //waitpid(td, &status, 0);
                    //pthread_join(td, NULL);		  
		            }
                else{
                if ((pid = fork()) < 0) { //<0 will cause a fork error
                  printf("fork error\n");
                  exit(1);
                } 
                else if (pid == 0) {		/* child */
                  if(array[0] != NULL && array[1] != NULL && array[2] != NULL && array[3] != NULL){
                  if(strcmp(array[2],">") == 0){  //Redirection with 4 arguments
                    if(noclobber==1 && access(array[3], F_OK ) != -1){  //Checks for file existing 
                      printf("%s : File exists.\n",array[3]);
                      exit(1);
                    }
                    else{
                      char *filename = array[3];
                      int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC); //Creates a file with truncation on
                      close(1); //Commands for >
                      dup(fid);
                      close(fid);
                      array[2] = NULL;
                      array[3] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[2],">&") == 0){ 
                     if(noclobber==1 && access(array[3], F_OK ) != -1){
                      printf("%s : File exists.\n",array[3]);
                      exit(1);
                    }
                    else{
                      char *filename = array[3];
                      int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC); //Truncation on
                      close(1); //Commands specificially for >&
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      array[2] = NULL;
                      array[3] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[2],">>") == 0){
                     if(noclobber==1 && access(array[3], F_OK ) == -1){
                      printf("%s : No Such File or Directory.\n",array[3]);
                      exit(1);
                    }
                    else{
                      char *filename = array[3];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);  //Truncation not on so It should not clear the file
                      close(1); //Commands specifically for >>
                      dup(fid);
                      close(fid);
                      array[2] = NULL;
                      array[3] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                   if(strcmp(array[2],">>&") == 0){
                   if(noclobber==1 && access(array[3], F_OK ) == -1){
                      printf("%s : No Such File or Directory.\n",array[3]);
                      exit(1);
                    }
                    else{
                      char *filename = array[3];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);  //Truncation is turned off
                      close(1); //Commands Specifically for >>&
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      array[2] = NULL;
                      array[3] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[1],"<") == 0){
                      char *filename = array[3];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);  
                      close(0);//Commands specificaly for <
                      dup(fid);
                      close(fid);
                      array[2] = NULL;
                      array[3] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                  }
                }
                else if(array[0] != NULL && array[1] != NULL && array[2] != NULL){  //This is when we are doing redirection with only 3 arguments
                  if(strcmp(array[1],">") == 0){
                    if(noclobber==1 && access(array[2], F_OK ) != -1){
                      printf("%s : File exists.\n",array[2]);
                      exit(1);
                    }
                    else{
                      char *filename = array[2];
                      int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC); //Truncation turned on
                      close(1); //For >
                      dup(fid);
                      close(fid);
                      array[1] = NULL;
                      array[2] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[1],">&") == 0){
                    if(noclobber==1 && access(array[2], F_OK ) != -1){
                      printf("%s : File exists.\n",array[2]);
                      exit(1);
                    }
                    else{
                      char *filename = array[2];
                      int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC); //Truncation turned on
                      close(1); //Commands for >&
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      array[1] = NULL;
                      array[2] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[1],">>") == 0){
                    if(noclobber==1 && access(array[2], F_OK ) == -1){
                      printf("%s : No Such File or Directory.\n",array[2]);
                      exit(1);
                    }
                    else{
                      char *filename = array[2];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);  //Truncation turned off
                      close(1);//Commands for >>
                      dup(fid);
                      close(fid);
                      array[1] = NULL;
                      array[2] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[1],">>&") == 0){
                   if(noclobber==1 && access(array[2], F_OK ) == -1){
                      printf("%s : No Such File or Directory.\n",array[2]);
                      exit(1);
                    }
                    else{
                      char *filename = array[2];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);  //Truncation turned off
                      close(1);//Commands for >>&
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      array[1] = NULL;
                      array[2] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                    }
                  }
                  if(strcmp(array[1],"<") == 0){
                      char *filename = array[2];
                      int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                      close(0);//Commands for <
                      dup(fid);
                      close(fid);
                      array[1] = NULL;
                      array[2] = NULL;
                      execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                      printf("couldn't execute: %s\n", buf);
                      exit(127);
                  }
                }
                else{
                  printf("Executing command %s\n", buf);
                  execve(buf, &array[0], envp); //Executes the command in our buffer on a file that is an outside command such as something in our directory
                  printf("couldn't execute: %s\n", buf);
                  exit(127);
                }
                }
                /* parent */
                if ((pid = waitpid(pid, &status, 0)) < 0){
                  printf("waitpid error\n");
                }
                }
              }
              else{
                printf("Does not have executable privileges\n")  ; 
              }
            }
            else{
              printf("File or executable does not exits\n");
            }
          }
          else{ //Very similar to above except for internal commands such as ls
                if(strcmp(array[argcount-1], "&") == 0) { /* background execution */
                  printf("Forking into background\n");
                  /* blank '&' arg */
                  // free(array[argcount - 1]);
                  array[argcount - 1] = NULL;
                  // argsct--;

                  /* build up thread argument struct */
                  struct threadargs *targ = malloc(sizeof(struct threadargs));
                    
                  targ->args = array;
                  targ->command = command;
                  targ->envp = envp;
                    
                  /* create pthread */
                  pthread_t td;
                  int status = pthread_create(&td, NULL, pthread_exec_external, (void*)targ);
                  //waitpid(td, &status, 0);
                  //pthread_join(td, NULL);		  
		            }
                else{ //All redirection commands same as above for external commands
                  if ((pid = fork()) < 0) {
                        printf("fork error\n");
                        exit(1);
                  } 
                  else if (pid == 0) {		/* child */
                  if(array[0] != NULL && array[1] != NULL && array[2] != NULL && array[3] != NULL){ 
                    if(strcmp(array[2],">") == 0){
                      if(noclobber==1 && access(array[3], F_OK ) != -1){
                        printf("%s : File exists.\n",array[3]);
                        exit(1);
                      }
                      else{
                        char *filename = array[3];
                        int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(fid);
                        array[2] = NULL;
                        array[3] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[2],">&") == 0){
                      if(noclobber==1 && access(array[3], F_OK ) != -1){
                        printf("%s : File exists.\n",array[3]);
                        exit(1);
                      }
                      else {
                        char *filename = array[3];
                        int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(2);
                        dup(fid);
                        close(fid);
                        array[2] = NULL;
                        array[3] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[2],">>") == 0){
                    if(noclobber==1 && access(array[3], F_OK ) == -1){
                        printf("%s : No Such File or Directory.\n",array[3]);
                        exit(1);
                      }
                      else {
                        char *filename = array[3];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(fid);
                        array[2] = NULL;
                        array[3] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[2],">>&") == 0){
                    if(noclobber==1 && access(array[3], F_OK ) == -1){
                        printf("%s : No Such File or Directory.\n",array[3]);
                        exit(1);
                      }
                      else{
                        char *filename = array[3];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(2);
                        dup(fid);
                        close(fid);
                        array[2] = NULL;
                        array[3] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[2],"<") == 0){
                        char *filename = array[3];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(0);
                        dup(fid);
                        close(fid);
                        array[2] = NULL;
                        array[3] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                    }
                  }
                  else if(array[0] != NULL && array[1] != NULL && array[2] != NULL){
                    if(strcmp(array[1],">") == 0){
                      if(noclobber==1 && access(array[2], F_OK ) != -1){
                        printf("%s : File exists.\n",array[2]);
                        exit(1);
                      }
                      else{
                        char *filename = array[2];
                        int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(fid);
                        array[1] = NULL;
                        array[2] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[1],">&") == 0){
                      if(noclobber==1 && access(array[2], F_OK ) != -1){
                        printf("%s : File exists.\n",array[2]);
                        exit(1);
                      }
                      else{
                        char *filename = array[2];
                        int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(2);
                        dup(fid);
                        close(fid);
                        array[1] = NULL;
                        array[2] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[1],">>") == 0){
                    if(noclobber==1 && access(array[2], F_OK ) == -1){
                        printf("%s : No Such File or Directory.\n",array[2]);
                        exit(1);
                      }
                      else{
                        char *filename = array[2];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(fid);
                        array[1] = NULL;
                        array[2] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[1],">>&") == 0){
                    if(noclobber==1 && access(array[2], F_OK ) == -1){
                        printf("%s : No Such File or Directory.\n",array[2]);
                        exit(1);
                      }
                      else{
                        char *filename = array[2];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(1);
                        dup(fid);
                        close(2);
                        dup(fid);
                        close(fid);
                        array[1] = NULL;
                        array[2] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                      }
                    }
                    if(strcmp(array[1],"<") == 0){
                        char *filename = array[2];
                        int fid = open(filename, O_WRONLY|O_APPEND|O_CREAT,S_IRWXU,S_IRWXG,S_IRWXO|O_TRUNC);
                        close(0);
                        dup(fid);
                        close(fid);
                        array[1] = NULL;
                        array[2] = NULL;
                        execvp(buf, &array[0]); 
                        printf("couldn't execute: %s\n", buf);
                        exit(127);
                    }
                  }
                  else{
                    printf("Executing external command %s\n", array[0]);
                    execvp(buf, &array[0]); 
                    printf("couldn't execute: %s\n", buf);
                    exit(127);
                  }
                  }
                  /* parent */
                  if ((pid = waitpid(pid, &status, 0)) < 0){
                    printf("waitpid error\n");
                  } 
                }
          }
        }
        printf("%s:%s» ", prompt, owd); //Prints out the prompt for the next input
        fflush(NULL);
  }
    //Although we should never get here it is important to free what we should be freeing just in case we do get to this critical point
    fflush(NULL);
    free(prompt);
    free(commandline);
    free(args);
    free(pwd);
    free(owd);
    freeList(pathlist);
    free(prevdir);
    exit(0);
  
  }
  
   /* sh() */

char *which(char *command, struct pathelement *pathlist )
{
  if(pathlist == NULL){
    return NULL; /* null pathlist */
  }

  char *cmd;

  while(pathlist != NULL) {
    cmd = malloc(sizeof(char)*(strlen(command)+strlen(pathlist->element)+2));
    /* build a path then see if it can be accessed */
    sprintf(cmd, "%s/%s", pathlist->element,command);

    if(access(cmd, F_OK | X_OK) == 0){
      return cmd; //Can't use break beacuse we must return
    }
    pathlist = pathlist->next;
    free(cmd);  //Free
  }
return NULL;
} /* which() */

char *where(char *command, struct pathelement *pathlist )
{
  
   //Safty check
   if(pathlist == NULL){
    return NULL; /* null pathlist */
   }

  char *cmd;  
  while (pathlist) {         // where (While we have more in our list
    cmd = malloc(sizeof(char)*(strlen(command)+strlen(pathlist->element)+2)); //We cmod our command
    sprintf(cmd, "%s/%s", pathlist->element,command);
    if (access(cmd,F_OK | X_OK) == 0){  //If we have access to this file and it is an executiable
       printf("[%s]\n", cmd);   //We print out what it is and keep going
       fflush(stdout);
    }
    pathlist = pathlist->next;  //Go to the next path avaible to us that we want to look at
    free(cmd);
  }

return NULL;
} /* where() */

void list ( char *dir )
{
  DIR	 *dp;
	struct dirent	*dirp;
	if((dp = opendir(dir)) == NULL){  //If we can open the directory we open it, otherwise we print out a message saying we cannot open this directory and we return it
		printf("Can't open %s\n", dir);
		return;
	}
  while ((dirp = readdir(dp)) != NULL){ //While we have more to read in the directory we do so and we print out the file, when this is over we will return
		printf("%s \n", dirp->d_name);
	}
  //Closes the directory
  closedir(dp);
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
} /* list() */

//Frees a linked list
void freeList(struct pathelement* head)
{
   struct pathelement* tmp;
  //While we are not done, we free our last child, and make our way back up to the root node
   while (head != NULL)
    {
       tmp = head;  //Used for switching in order to not lose any nodes
       head = head->next;
       free(tmp->element);
       free(tmp);
    }

}


int is_file_exec(char *command) {
  /* if path starts at root or is a dot-slash path, return 0 */ 
  if((command[0] == '/') || ((command[0] == '.') && (command[1] == '/'))) {
    return 0;
  } 
  else{
    return 1;
  }
}


//watchmail
 void *watchmail(void *arg){
   char* file = (char*)arg;
   struct stat path;
   
   stat(file, &path); //stats of the ti,e
   long old = (long)path.st_size; //We get the file size
   time_t start;
   while(1){
     time(&start);  //for tracking 
     stat(file, &path);
     if((long)path.st_size != old){   //If the size has changed we notify our user
       printf("\nBEEP! You got mail in %s at time %s\n", file, ctime(&start));
       fflush(stdout);
       old = (long)path.st_size;
     }
     sleep(1);
   }
 }

 void *watchuser(void *arg){
  
  struct utmpx *up;

  while(1){
    setutxent();
    
    while((up = getutxent())){
      if(up->ut_type == USER_PROCESS){

	pthread_mutex_lock(&mutex); //Begin critical section
	struct strlist *tmp;
	tmp = watchuserhead;  //Modification of tmp
	while(tmp != NULL){
	  if((tmp->status == 0) && strcmp(tmp->str, up->ut_user) == 0){
	    tmp->status = 1;
	    printf("\n%s has logged on [%s] from [%s]\n", up->ut_user, up->ut_line, up->ut_host); //prints our user has loged on message
	  }
	  tmp = tmp->next;
	}
	pthread_mutex_unlock(&mutex); //End critical section
      }
    }
	}
}

void* pthread_exec_path(void *arg) {  //Almost identical as exec external ecept it uses execve isntead because it needs the enviornment
  struct threadargs *targ = (struct threadargs*)arg; //Creates thread argument struct
  pid_t cpid = fork(); //Forks inside of the thread
   if(cpid == 0) { //If child, runs the command
     //printf("Executing %s\n", targ->args[0]); 
     if(execve(targ->args[0], &targ->args[0], targ->envp) == -1) {  //Execute
       printf("%s: Command not found.\n", targ->args[0]); 
     } 
   } else if(cpid > 0) { 
     int status; 
     waitpid(cpid, &status, 0); 
     if(WEXITSTATUS(status) != 0) { 
       printf("%s: Command exited with status: %d\n", targ->args[0], WEXITSTATUS(status)); 
     } 
   } else {
     printf("%s: Unable to fork child process.\n", targ->args[0]); 
   } 
  
  free(targ);
  return ((void*)0);
}

void* pthread_exec_external(void *arg) {  //For running external commands in the background
  struct threadargs *targ = (struct threadargs*)arg;  //Creates thread argument struct
  pid_t cpid = fork();  //Forks inside of the thread
  if(cpid == 0) { //If child, runs the command
    if(execvp(targ->args[0], &targ->args[0])) {
      printf("%s: Command not found.\n", targ->command);
    }
  } else if(cpid > 0) { //Otherwise it waits for it
    int status;
    waitpid(cpid, &status, 0);
    if(WEXITSTATUS(status) != 0) {
      printf("%s: Command exited with status: %d\n", targ->command, WEXITSTATUS(status));
    }
  } else {
    printf("%s: Unable to fork child process.\n", targ->command);
  }

  free(targ); //Frees struct
  return ((void*)0);
}