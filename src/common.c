// common.c -- Defines some global functions.
//             From JamesM's kernel development tutorials.

#include "common.h"
#include "monitor.h"

extern "C" {

static int interrupts_on = 1;

int disable_interrupts() {
  if(!interrupts_on) return 0;

  __asm__ volatile("cli;");
  interrupts_on = 0;
  return 1;
}

void restore_interrupts(int val) {
  if(val) {
    interrupts_on = 1;
    __asm__ volatile("sti;");
  }
}

// Write a byte out to the specified port.
void outb(u16int port, u8int value)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

// Write a byte out to the specified port.
void outw(u16int port, u16int value)
{
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (value));
}

// Write a byte out to the specified port.
void outl(u16int port, u32int value)
{
    asm volatile ("outl %1, %0" : : "dN" (port), "a" (value));
}

u8int inb(u16int port)
{
    u8int ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

u16int inw(u16int port)
{
    u16int ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

u32int inl(u16int port) {
    u32int ret;
    asm volatile ("inl %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

// Copy len bytes from src to dest.
void memcpy(u8int *dest, const u8int *src, u32int len)
{
    const u8int *sp = (const u8int *)src;
    u8int *dp = (u8int *)dest;
    for(; len != 0; len--) *dp++ = *sp++;
}

// Write len copies of val into dest.
void memset(u8int *dest, u8int val, u32int len)
{
    u8int *temp = (u8int *)dest;
    for ( ; len != 0; len--) *temp++ = val;
}

// Compare two strings. Should return -1 if 
// str1 < str2, 0 if they are equal or 1 otherwise.
int strcmp(const char *str1, const char *str2)
{
      int i = 0;
      int failed = 0;
      while(str1[i] != '\0' && str2[i] != '\0')
      {
          if(str1[i] != str2[i])
          {
              failed = 1;
              break;
          }
          i++;
      }
      // why did the loop exit?
      if( (str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0') )
          failed = 1;
  
      return failed;
}

// Copy the NULL-terminated string src into dest, and
// return dest.
char *strcpy(char *dest, const char *src)
{
    do
    {
      *dest++ = *src++;
    }
    while (*src != 0);
}

// Concatenate the NULL-terminated string src onto
// the end of dest, and return dest.
char *strcat(char *dest, const char *src)
{
    while (*dest != 0)
    {
        *dest = *dest++;
    }

    do
    {
        *dest++ = *src++;
    }
    while (*src != 0);
    return dest;
}

int strlen(char *src)
{
    int i = 0;
    while (*src++)
        i++;
    return i;
}

extern void panic(const char *message, const char *file, u32int line)
{
    // We encountered a massive problem and have to stop.
    asm volatile("cli"); // Disable interrupts.

    console.printf("PANIC(%s) at %s:%d\n", message, file, line);

    // Halt by going into an infinite loop.
    for(;;) {
      asm volatile("hlt;");
    }
}

extern void panic_assert(const char *file, u32int line, const char *desc)
{
    // An assertion failed, and we have to panic.
    asm volatile("cli"); // Disable interrupts.

    console.printf("ASSERTION-FAILED(%s) at %s:%d\n", desc, file, line);

    // Halt by going into an infinite loop.
    for(;;) {
      asm volatile("hlt;");
    }
}

void kabort() {
  asm volatile("cli"); // Disable interrupts.
  kputs("Your kernel has aborted(). Get some cofffe.\n");
  for(;;) {
    asm volatile("hlt;");
  }
}

}