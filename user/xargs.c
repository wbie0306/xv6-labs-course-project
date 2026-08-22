#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

static void
run(char **command, char *line)
{
  if(fork() == 0){
    exec(command[0], command);
    fprintf(2, "xargs: exec %s failed\n", command[0]);
    exit(1);
  }
  wait(0);
}

int
main(int argc, char *argv[])
{
  char line[512];
  char *command[MAXARG];
  int command_count;
  int length = 0;
  char ch;

  if(argc < 2 || argc >= MAXARG){
    fprintf(2, "usage: xargs command [arguments ...]\n");
    exit(1);
  }

  for(command_count = 0; command_count < argc - 1; command_count++)
    command[command_count] = argv[command_count + 1];
  command[command_count] = line;
  command[command_count + 1] = 0;

  while(read(0, &ch, 1) == 1){
    if(ch == '\n'){
      line[length] = 0;
      run(command, line);
      length = 0;
    } else if(length + 1 < sizeof(line)) {
      line[length++] = ch;
    } else {
      fprintf(2, "xargs: input line too long\n");
      exit(1);
    }
  }
  if(length > 0){
    line[length] = 0;
    run(command, line);
  }
  exit(0);
}
