#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
fail(char *message)
{
  fprintf(2, "pingpong: %s\n", message);
  exit(1);
}

int
main(int argc, char *argv[])
{
  int parent_to_child[2];
  int child_to_parent[2];
  char byte = 'x';
  int pid;

  if(argc != 1){
    fprintf(2, "usage: pingpong\n");
    exit(1);
  }

  if(pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0)
    fail("pipe failed");

  pid = fork();
  if(pid < 0)
    fail("fork failed");

  if(pid == 0){
    close(parent_to_child[1]);
    close(child_to_parent[0]);

    if(read(parent_to_child[0], &byte, 1) != 1)
      fail("child read failed");
    printf("%d: received ping\n", getpid());
    if(write(child_to_parent[1], &byte, 1) != 1)
      fail("child write failed");

    close(parent_to_child[0]);
    close(child_to_parent[1]);
    exit(0);
  }

  close(parent_to_child[0]);
  close(child_to_parent[1]);

  if(write(parent_to_child[1], &byte, 1) != 1)
    fail("parent write failed");
  if(read(child_to_parent[0], &byte, 1) != 1)
    fail("parent read failed");
  printf("%d: received pong\n", getpid());

  close(parent_to_child[1]);
  close(child_to_parent[0]);
  wait(0);
  exit(0);
}
