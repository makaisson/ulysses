// 
// task.c - Implements the functionality needed to multitask.
//          Written for JamesM's kernel development tutorials.
//

#include "task.hpp"
#include "paging.hpp"
#include "kheap.hpp"
#include "descriptor_tables.hpp"
#include "cpu.hpp"
#include "console.hpp"
#include "timer.hpp"

Scheduler scheduler;

Task::Task(int pid)
  : id(pid)
  , alarm_at(0)
  , exit_code(0)
{}

void Task::sleep_til(int secs) {
  alarm_at = timer.ticks + timer.secs_to_ticks(secs);
}

bool Task::alarm_expired() {
  return state == Task::eWaiting && 
         alarm_at != 0 &&
         alarm_at <= timer.ticks;
}

extern "C" void* save_registers(volatile Task::SavedRegisters*);
extern "C" void restore_registers(volatile Task::SavedRegisters*, u32);
extern "C" void second_return();

void Scheduler::init() {
  // Rather important stuff happening, no interrupts please!
  cpu::disable_interrupts();

  next_pid = 0;
  current = 0;
  ready_queue.init();
  cleanup_queue.init();
  waiting_queue.init();

  // Initialise the first task (kernel task)
  current = new(kheap) Task(next_pid++);
  current->directory = vmem.current_directory;
  current->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);

  make_ready(current);

  // Reenable interrupts.
  cpu::enable_interrupts();
}

void Scheduler::cleanup() {
  Task::RunList::Iterator i = cleanup_queue.begin();

  while(i.more_p()) {
    Task* task = i.advance();
    cleanup_queue.unlink(task);

    vmem.free_directory(task->directory);

    kfree((void*)task->kernel_stack);
    kfree(task);
  }
}

void Scheduler::on_tick() {
  Task::RunList::Iterator i = waiting_queue.begin();

  bool schedule = false;
  while(i.more_p()) {
    Task* task = i.advance();

    if(task->alarm_expired()) {
      waiting_queue.unlink(task);
      make_ready(task);
      schedule = true;
    }
  }

  if(schedule) switch_task();
}

void Scheduler::switch_task() {
  // If we haven't initialised tasking yet, just return.
  if(!current) return;

  // Save the current registers into the current.
  //
  // This routine will return twice. Once after we initially call it
  // and other time when the current task is being resumed. We detect
  // that happening by checking if the returned value is &second_return;
  void* val = save_registers((Task::SavedRegisters*)&current->regs);

  // Have we just switched tasks?
  if(val == &second_return) return;

  // Get the next task to run.
  Task* next = current->next_runnable();

  // If we fell off the end of the linked list start again at the beginning.
  if(!next) next = ready_queue.head();

  if(next == current) return;

  current = next;

  // Make sure the memory manager knows we've changed page directory.
  vmem.current_directory = current->directory;

  // Change our kernel stack over.
  set_kernel_stack(current->kernel_stack + KERNEL_STACK_SIZE);

  // Copy the registers in current->regs back to the machine
  // and jump to the eip held in regs.eip.
  //
  // This will cause save_registers to return for a second time and
  // the return value will be &second_return;
  restore_registers(&current->regs, vmem.current_directory->physicalAddr);
}

void Scheduler::exit(int code) {
  // We are modifying kernel structures, and so cannot be interrupted.
  int st = cpu::disable_interrupts();

  ready_queue.unlink(current);

  current->exit_code = code;
  current->state = Task::eDead;
  cleanup_queue.prepend(current);

  cpu::restore_interrupts(st);

  switch_task();
}

int Scheduler::wait_any(int *status) {
  // We are modifying kernel structures, and so cannot be interrupted.
  int st = cpu::disable_interrupts();

  cpu::restore_interrupts(st);
  return -1;

}

void Scheduler::sleep(int secs) {
  // We are modifying kernel structures, and so cannot be interrupted.
  int st = cpu::disable_interrupts();

  current->sleep_til(secs);

  ready_queue.unlink(current);
  make_wait(current);

  cpu::restore_interrupts(st);

  switch_task();
}

int Scheduler::fork() {
  // We are modifying kernel structures, and so cannot be interrupted.
  int st = cpu::disable_interrupts();

  // Clone the address space.
  page_directory* directory = vmem.clone_current();

  // Create a new process.
  Task* new_task = new(kheap) Task(next_pid++);
  new_task->directory = directory;
  new_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);

  ready_queue.append(new_task);

  if(save_registers(&new_task->regs) != &second_return) {
    // All finished: Reenable interrupts.
    cpu::restore_interrupts(st);

    // And by convention return the PID of the child.
    return new_task->id;
  } else {
    // We are the child - by convention return 0.
    return 0;
  }
}

int Scheduler::getpid() {
  return current->id;
}

void Scheduler::switch_to_user_mode() {
  // Set up our kernel stack.
  set_kernel_stack(current->kernel_stack + KERNEL_STACK_SIZE);

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