#include "kernel/types.h"
#include "user/user.h"

__attribute__((noinline)) static int
trace_inner(int value)
{
  return backtrace() + value;
}

__attribute__((noinline)) static int
trace_middle(int value)
{
  return trace_inner(value) + 1;
}

int
main(void)
{
  int result = trace_middle(1);
  printf("backtrace result: %d\n", result);
  exit(0);
}