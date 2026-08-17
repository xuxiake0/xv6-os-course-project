#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define LINE_SIZE 512

static int
is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\r';
}

static void
run_line(int argc, char *argv[], char *line)
{
  char *args[MAXARG];
  char *p;
  int base_count;
  int arg_count;
  int pid;
  int i;

  base_count = argc - 1;
  if(base_count >= MAXARG - 1){
    fprintf(2, "xargs: too many command arguments\n");
    exit(1);
  }

  for(i = 0; i < base_count; i++)
    args[i] = argv[i + 1];
  arg_count = base_count;

  p = line;
  while(*p){
    while(is_space(*p))
      p++;
    if(*p == 0)
      break;
    if(arg_count >= MAXARG - 1){
      fprintf(2, "xargs: too many arguments in input line\n");
      return;
    }

    args[arg_count++] = p;
    while(*p && !is_space(*p))
      p++;
    if(*p)
      *p++ = 0;
  }

  if(arg_count == base_count)
    return;
  args[arg_count] = 0;

  pid = fork();
  if(pid < 0){
    fprintf(2, "xargs: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    exec(args[0], args);
    fprintf(2, "xargs: exec %s failed\n", args[0]);
    exit(1);
  }
  wait(0);
}

int
main(int argc, char *argv[])
{
  char line[LINE_SIZE];
  char c;
  int length = 0;
  int overflow = 0;
  int n;

  if(argc < 2){
    fprintf(2, "usage: xargs command [arguments ...]\n");
    exit(1);
  }

  while((n = read(0, &c, 1)) == 1){
    if(c == '\n'){
      if(overflow){
        fprintf(2, "xargs: input line too long\n");
      } else {
        line[length] = 0;
        run_line(argc, argv, line);
      }
      length = 0;
      overflow = 0;
    } else if(!overflow) {
      if(length < LINE_SIZE - 1)
        line[length++] = c;
      else
        overflow = 1;
    }
  }

  if(n < 0){
    fprintf(2, "xargs: read failed\n");
    exit(1);
  }
  if(overflow){
    fprintf(2, "xargs: input line too long\n");
  } else if(length > 0) {
    line[length] = 0;
    run_line(argc, argv, line);
  }

  exit(0);
}
