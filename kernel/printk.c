//
// formatted console output -- printk, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicking = 0; // printing a panic message
volatile int panicked = 0;  // spinning forever at end of a panic

// lock to avoid interleaving concurrent printk's.
static struct {
  struct spinlock lock;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console.
int
printk(char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  if (panicking == 0)
    acquire(&pr.lock);

  va_start(ap, fmt);
  for (i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      consputc(cx);
      continue;
    }
    i++;
    c0 = fmt[i + 0] & 0xff;
    c1 = c2 = 0;
    if (c0)
      c1 = fmt[i + 1] & 0xff;
    if (c1)
      c2 = fmt[i + 2] & 0xff;
    if (c0 == 'd') {
      printint(va_arg(ap, int), 10, 1);
    } else if (c0 == 'l' && c1 == 'd') {
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if (c0 == 'u') {
      printint(va_arg(ap, uint32), 10, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if (c0 == 'x') {
      printint(va_arg(ap, uint32), 16, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if (c0 == 'p') {
      printptr(va_arg(ap, uint64));
    } else if (c0 == 'c') {
      consputc(va_arg(ap, uint));
    } else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
    } else if (c0 == '%') {
      consputc('%');
    } else if (c0 == 0) {
      break;
    } else {
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c0);
    }
  }
  va_end(ap);

  if (panicking == 0)
    release(&pr.lock);

  return 0;
}

int
kernel_backtrace(uint64 *addresses, int capacity)
{
  uint64 frame = (uint64)__builtin_frame_address(0);
  int count = 0;

  while (count < capacity && frame >= 16 && (frame & 15) == 0) {
    uint64 next = *(uint64 *)(frame - 16);
    addresses[count++] = *(uint64 *)(frame - 8);
    if (next <= frame)
      break;
    frame = next;
  }
  return count;
}

int
user_backtrace(struct proc *process, uint64 *addresses, int capacity)
{
  uint64 frame = process->trapframe->s0;
  int count = 0;

  if (capacity > 0 && process->trapframe->ra != 0)
    addresses[count++] = process->trapframe->ra;
  while (count < capacity && frame >= 16 && frame <= process->sz &&
         (frame & 15) == 0) {
    uint64 physical = walkaddr(process->pagetable, frame - 16);
    uint64 next, address;
    if (physical == 0)
      break;
    physical += (frame - 16) % PGSIZE;
    next = *(uint64 *)physical;
    address = *(uint64 *)(physical + 8);
    if (address == 0 || next <= frame || next > process->sz ||
        (next & 15) != 0)
      break;
    addresses[count++] = address;
    frame = next;
  }
  return count;
}

void
print_backtrace(char *label, uint64 *addresses, int count)
{
  printk("%s backtrace:\n", label);
  for (int index = 0; index < count; index++)
    printk("%p\n", (void *)addresses[index]);
}

void
panic(char *s)
{
  uint64 addresses[32];
  int count;

  panicking = 1;
  printk("panic: ");
  printk("%s\n", s);
  count = kernel_backtrace(addresses, 32);
  print_backtrace("kernel", addresses, count);
  panicked = 1; // freeze uart output from other CPUs
  for (;;)
    ;
}

void
printkinit(void)
{
  initlock(&pr.lock, "pr");
}
