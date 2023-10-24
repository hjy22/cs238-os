/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * jitc.c
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#include "system.h"
#include "jitc.h"

struct jitc
{
  void *handle;
};

int jitc_compile(const char *input, const char *output)
{
  pid_t pid = fork();
  char *args[7];

  if (-1 == pid) {
      TRACE("FORK FAILED");
      return -1;
  }
  else if (0 == pid)
  {
    /* This block will be executed by child process */
    args[0] = "gcc";
    args[1] = "-shared";
    args[2] = "-fPIC";
    args[3] = "-o";
    args[4] = (char *)output;
    args[5] = (char *)input;
    args[6] = NULL;
    execv("/usr/bin/gcc", args);
    
    /*execute end, exit*/
    exit(EXIT_SUCCESS);
  }
  else
  {
    int status;
    /*parent process: Waiting for the child to complete */
    waitpid(pid, &status, 0);
    /*WIFEXITED - check whether a child process has terminated normally*/
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
      /*success*/
      return 0; 
    }
    else
    {
      /*fail*/
      return -1; 
    }
  }
}

struct jitc *jitc_open(const char *pathname)
{
  char relativePath[128] = "./";
  void *handle = NULL;
  struct jitc *jitc = NULL;
  
  strcpy(relativePath+2,pathname);

  if (!(handle= dlopen(relativePath, RTLD_NOW)))
  {
    dlerror();
    return NULL;
  }

  //malloc memory for the struct jitc
  if (!(jitc=(struct jitc *)malloc(sizeof(struct jitc))))
  {
    dlerror();
    dlclose(handle);
    return NULL;
  }

  jitc->handle = handle;
  return jitc;
}

void jitc_close(struct jitc *jitc)
{
  if (dlclose(jitc->handle))
  {
    TRACE("close the library failed");
    return;
  }
  free(jitc);
}

long jitc_lookup(struct jitc *jitc, const char *symbol)
{
  void *sym_address = NULL;

  if (!(sym_address = dlsym(jitc->handle, symbol)))
  {
    TRACE("Search failed");
    return -1;
  }

  return (long)sym_address;
}
