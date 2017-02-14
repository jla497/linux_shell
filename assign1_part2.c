/*split string with | delimiter into separate command tokens

split command tokens with second \n\t delimiters into separate command and arg tokens inside the child process
*/


#include <sys/types.h>
#include <wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <dirent.h>
#include <ctype.h>
#include<signal.h>

#define BUFFER_SIZE 100
#define MAX_ARGS 50


//SIGINT handler. This will catch Ctrl+C and kill foreground process 
void sig_handler(int signo)
{ 
  signal(signo, SIG_IGN);
  
  if(signo == SIGINT)
  { 
    printf("SIGINT received. my pid is: %ld\n",(long)getpid());
  
    kill(getpid(),SIGKILL);
  
  }
}

void checkFd()
{
   //check if any file descriptors are still open
       char path[] = "/proc/self/fd/";
       
       struct dirent* dent;
       
       DIR * dir = opendir(path);
       if(dir != NULL){
         while((dent=readdir(dir))!=NULL)
         {
           printf("%s\n",dent->d_name);
        
         }
         free(dent);
         closedir(dir);
       }
}

void returnWaitStatus(int status, pid_t pid)
{
  printf("Child process exited: PID %ld\n", (long)pid);
  if(WIFEXITED(status))
  {
     printf("child exited normally\n");
  }
            
   if(WIFSIGNALED(status))
   {
       printf("Child was terminated by a signal: %d\n",WTERMSIG(status));
    }
            
    if(WIFSTOPPED(status))
    {
       printf("Child was terminated by a delivery of a signal: %d\n", WSTOPSIG(status));
    }

}

int main()
{ 
  int running = 1;
  int status;
  int res;
  int count;
  int historyIdx = 0;
  int idx = 0;
  int pIdx = 0;
  int  pipes[BUFFER_SIZE][2]; // pipe array
  char history[10][BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  char pipeDelimter[] = "|\n\0";
  char delimiter[] = " \t\n\r";  //delimiter space/tab/newline/return
  char* command;
  char* firstParam;
  char* params[MAX_ARGS]; 
  char currDir[BUFFER_SIZE];
  char msgChild[BUFFER_SIZE];
  char* temp;
  pid_t pid;

  
  //make parent process ignore SIGINT 
  signal(SIGINT, SIG_IGN);
 
  //loop until ctrl+d 
  while(!feof(stdin)){
   
    
    getcwd(currDir,BUFFER_SIZE);
   
    printf("jsh@%s> ",currDir);
     
    memset(buffer,'\0',BUFFER_SIZE);
    
    //reset idx;
    idx = 0;
  
   //read from stdin
   if(fgets(buffer,BUFFER_SIZE,stdin)){
     if(strchr(buffer,'\n') == NULL)
     {
       char c;
       c = getchar();
       while (c != EOF && c != '\n')
       {
         c = getchar();
       }
       
       printf("invalid input. Try again.\n");
     }
   }
   
   idx = strlen(buffer);
   
   //check length of buffer is less than 1. If not, continue
   if(buffer != NULL)
   {
    
    //allocate enough memory for command string
   command = (char*) malloc(idx+1);
    
    if(command == NULL)
    {
      perror("failed to allocate memory\n");
      return -1;
    } 
    
    //copy from buffer to command
    strncpy(command, buffer, idx);
    
    command[idx] = '\0';
    
    //copy command into history array
    for(int i = 0;i<idx;i++)
    {
      history[historyIdx][i] = command[i];
    }
    
     history[historyIdx][idx] = '\0';
    //increment historyIdx
    historyIdx = (historyIdx + 1) % 10;
    	
    //check what the first command is.
    count = 0;
     
    while(isalpha(command[count]) || isdigit(command[count])){
       count++;
     }
     
     firstParam = (char*)malloc(count+1);
     
     if(firstParam == NULL)
     {
        perror("failed to allocate memory\n");
        return -1;
     
     }
   
     for(int i = 0;i<count;i++)
     {
       firstParam[i] = command[i];
     }
    
     firstParam[count] = '\0';
     
     //printf("firstParam:%s\n",firstParam);
    if(strcmp(firstParam,"exit")==0)
    {
      return 1;
    
    }else if(strcmp(firstParam,"pwd")==0)
    {
      getcwd(currDir,BUFFER_SIZE);
      
      printf("%s\n",currDir);
      
    }else if(strcmp(firstParam,"history")==0)
    {
      for(int i = 0;i<historyIdx;i++)
      {
        printf("%s\n",history[i]);
      }
    }else if(strcmp(firstParam,"cd")==0)
    {
      while(params[pIdx] = strtok_r(command, delimiter, &command))
      {
        pIdx++;
      }
      
      if(pIdx<1 || pIdx>1)
       printf("invalid directory\n");
      else{
         res = chdir(params[0]);
        
      }
      
    }else if(strlen(firstParam) < 1){
      // only '\n' was in the input     
      continue;
    
    }else{
      //check if last character in the command string is '&'
      int background = 0;
    
      char pipeDelimiter[] = "|";
      
      //check for &. Bg process
      for(int j = 0;j<strlen(command);j++)
        {
         if(command[j]=='&')
         {
           background = 1;
           command[j] = ' ';
         }
        }
      
      //get rest of params
       while(params[pIdx] = strtok_r(command,pipeDelimiter, &command))
      { 
        //printf("param[%d]: %s\n",pIdx,params[pIdx]);
        pIdx++;
      } 
      
      //malloc pids array to store child processes' ids
      //childrenPids = (pid_t*) malloc(sizeof(pid_t)*pIdx);
      
      //iterate fork() as many times as pIdx 
      for(int i = 0;i<pIdx;i++){
        
        int argNum = 0;
        
        int len;
       
        char* args[BUFFER_SIZE];
      
        char* instruction = malloc(sizeof(params[i]));
        
        pipe(pipes[i]);  //parent->child pipes[i][1] is parent's write. pipes[i][0] is child read
       
        if(instruction == NULL)
        {
          printf("failed to allocate memory to instruction\n");
          return -1;
        }
        strcpy(instruction,params[i]);
        
       //tokenize rest of args 
        while(args[argNum++] = strtok_r(instruction, delimiter, &instruction))
        {
          //printf(" %s\n",args[argNum-1]);
        }
        //set last args[argNum] to end with 0. This is how execvp knows where the end of argument is.
       
       
       
       ////////////////////////////
        args[argNum] = "\0";
         
        //fork child processes
        pid_t pid = fork();
        if(pid<0)
        {
          printf("%s\n",strerror(errno));
          return 1;
        }else if(pid == 0) //child process
        { 
          printf("child pid is:%ld\n",(long)getpid());
          //setpgid(0,0);
          
          char buffer[BUFFER_SIZE];
          
          char msg[BUFFER_SIZE];
          
          //set signal_handler in child process first
           signal(SIGINT, sig_handler);
          
          //close unused pipe ends
          //close(readpipe[0]);
          
          close(pipes[i][0]); 
           
          
          //duplicate stdout to pipe[i][1] and stdin to pipe[i-1][0] before calling execvp
         if( i != pIdx-1){ 
           dup2(pipes[i][1],1);  
         }
         
         close(pipes[i][1]);
         
         dup2(pipes[i-1][0], 0);
           
         close(pipes[i-1][0]);
         
          
          res = execvp(args[0],args);
          
          //if execv failed and old process image not overwritten
          printf("%s\n",strerror(errno));
          
          //signal(getppid(), SIGQUIT);
         
          
          exit(0);
         
        }else if(pid > 0){
        
         close(pipes[i][1]);
         //close(pipes[i][0]);
         
        }
       }
       
       for(int j = 0; j < pIdx;j++)
       { 
         close(pipes[j][0]);
         close(pipes[j][1]);
       }
        
       while (pIdx > 0)
       {
        int status;
          
        pid_t state = wait(&status);
        if(state > -1)
        {
          returnWaitStatus(status, state);
            
        }else if(state <= -1)
        {
          printf("WAIT_ERROR\n");
        }
          
         pIdx--;
        }
      }
       
       //check for open fd
       //checkFd(); 
      
     //if unsuccesful,print error
     
        
    //reset idx
    idx = 0;
    
    //reset pIdx;
    pIdx = 0;
    
    //clear firstParam
    //free(firstParam);
    
    
    }
    
  }
  printf("\n");
  
 }
 



