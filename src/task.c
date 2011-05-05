// 
// task.c - Implements the functionality needed to multitask.
//          Written for JamesM's kernel development tutorials.
//

#include "task.h"
#include "paging.h"
#include "kheap.h"
#include "descriptor_tables.h"
#include "cpu.h"
#include "console.h"

Scheduler scheduler = {0, 0, 0};

// Some externs are needed to access members in paging.c...
extern u32 initial_esp;
extern "C" u32 read_eip();

extern "C" void* save_registers(volatile Task::SavedRegisters*);
extern "C" void restore_registers(volatile Task::SavedRegisters*, u32);
extern "C" void second_return();

static void move_stack(void *new_stack_start, u32 size) {
  u32 i;
  // Allocate some space for the new stack.
  for(u32 i = (u32)new_stack_start;
      i >= ((u32)new_stack_start-size);
      i -= 0x1000)
  {
    // General-purpose stack is in user-mode.
    vmem.alloc_frame(
        vmem.get_current_page(i, 1),
        0 /* User mode */,
        1 /* Is writable */ );
  }

  cpu::flush_tbl();

  // Old ESP and EBP, read from registers.
  u32 old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
  u32 old_base_pointer;  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));

  // Offset to add to old stack addresses to get a new stack address.
  u32 offset            = (u32)new_stack_start - initial_esp;

  // New ESP and EBP.
  u32 new_stack_pointer = old_stack_pointer + offset;
  u32 new_base_pointer  = old_base_pointer  + offset;

  int copy_size = initial_esp - old_stack_pointer;

  // Copy the stack.
  memcpy((u8*)new_stack_pointer, (u8*)old_stack_pointer, copy_size);
}

void Scheduler::init() {
    // Rather important stuff happening, no interrupts please!
    cpu::disable_interrupts();

    next_pid = 1;

    // Relocate the stack so we know where it is.
    move_stack((void*)0xE0000000, 0x2000);

    // Initialise the first task (kernel task)
    current_task = ready_queue = (Task*)kmalloc(sizeof(Task));
    current_task->id = next_pid++;
    current_task->regs.esp = 0;
    current_task->regs.ebp = 0;
    current_task->regs.eip = 0;
    current_task->directory = vmem.current_directory;
    current_task->next = 0;
    current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);

    // Reenable interrupts.
    cpu::enable_interrupts();
}

void Scheduler::switch_task() {
  // If we haven't initialised tasking yet, just return.
  if(!current_task) return;

  // Save the current registers into the current_task.
  //
  // This routine will return twice. Once after we initially call it
  // and other time when the current task is being resumed. We detect
  // that happening by checking if the returned value is &second_return;
  void* val = save_registers((Task::SavedRegisters*)&current_task->regs);

  // Have we just switched tasks?
  if(val == &second_return) return;

  // Get the next task to run.
  current_task = current_task->next;

  // If we fell off the end of the linked list start again at the beginning.
  if(!current_task) current_task = ready_queue;

  /* u32 eip = current_task->regs.eip; */
  /* u32 esp = current_task->regs.esp; */
  /* u32 ebp = current_task->regs.ebp; */

  /* console.printf("new task| eip: %x, esp: %x, ebp: %x\n", eip, esp, ebp); */

  // Make sure the memory manager knows we've changed page directory.
  vmem.current_directory = current_task->directory;

  // Change our kernel stack over.
  set_kernel_stack(current_task->kernel_stack+KERNEL_STACK_SIZE);

  // Copy the registers in current_task->regs back to the machine
  // and jump to the eip held in regs.eip.
  //
  // This will cause save_registers to return for a second time and
  // the return value will be &second_return;
  restore_registers(&current_task->regs, vmem.current_directory->physicalAddr);
}

void Scheduler::exit() {
  console.printf("reg pd: %x\n", cpu::page_directory());
  console.printf(" ct pd: %x\n", current_task->directory);
  console.printf("cur pd: %x\n", vmem.current_directory);
  console.printf("krn pd: %x\n", vmem.kernel_directory);
}

int Scheduler::fork() {
  // We are modifying kernel structures, and so cannot be interrupted.
  cpu::disable_interrupts();

  // Take a pointer to this process' task struct for later reference.
  Task* parent_task = (Task*)current_task;

  // Clone the address space.
  page_directory* directory = vmem.clone_current();

  // Create a new process.
  Task* new_task = (Task*)kmalloc(sizeof(Task));
  new_task->id = next_pid++;
  new_task->regs.esp = new_task->regs.ebp = 0;
  new_task->regs.eip = 0;
  new_task->directory = directory;
  current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
  new_task->next = 0;

  // Add it to the end of the ready queue.
  // Find the end of the ready queue...
  Task *tmp_task = (Task*)ready_queue;
  while(tmp_task->next) {
    tmp_task = tmp_task->next;
  }

  // ...And extend it.
  tmp_task->next = new_task;

  if(save_registers(&new_task->regs) != &second_return) {
    // All finished: Reenable interrupts.
    cpu::enable_interrupts();

    // And by convention return the PID of the child.
    return new_task->id;
  } else {
    // We are the child - by convention return 0.
    return 0;
  }
}

int Scheduler::getpid() {
  return current_task->id;
}

void Scheduler::switch_to_user_mode() {
  // Set up our kernel stack.
  set_kernel_stack(current_task->kernel_stack+KERNEL_STACK_SIZE);

  // Set up a stack structure for switching to user mode.
  asm volatile("  \
      cli; \
      mov $0x23, %ax; \
      mov %ax, %ds; \
      mov %ax, %es; \
      mov %ax, %fs; \
      mov %ax, %gs; \
      \
      \
      mov %esp, %eax; \
      pushl $0x23; \
      pushl %esp; \
      pushf; \
      pop %eax; \
      mov $0x200, %ecx; \
      or %ecx, %eax; \
      push %eax; \
      pushl $0x1B; \
      push $1f; \
      iret; \
      1: \
      "); 

}
