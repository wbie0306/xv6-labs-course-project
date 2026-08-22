#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
sieve(int input_fd)
{
  int prime;
  int candidate;
  int next[2];

  if(read(input_fd, &prime, sizeof(prime)) != sizeof(prime)){
    close(input_fd);
    exit(0);
  }

  printf("prime %d\n", prime);
  if(pipe(next) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  if(fork() == 0){
    close(input_fd);
    close(next[1]);
    sieve(next[0]);
  }

  close(next[0]);
  while(read(input_fd, &candidate, sizeof(candidate)) == sizeof(candidate)){
    if(candidate % prime != 0)
      write(next[1], &candidate, sizeof(candidate));
  }
  close(input_fd);
  close(next[1]);
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int numbers[2];
  int value;

  if(argc != 1 || pipe(numbers) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  if(fork() == 0){
    close(numbers[1]);
    sieve(numbers[0]);
  }

  close(numbers[0]);
  for(value = 2; value <= 35; value++)
    write(numbers[1], &value, sizeof(value));
  close(numbers[1]);
  wait(0);
  exit(0);
}
