#ifndef PROCESS_HPP
#define PROCESS_HPP

#include "thread.hpp"
#include "paging.hpp"
#include "list.hpp"
#include "fs.hpp"

class Process {
public:
  typedef sys::ExternalList<MemoryMapping> MMapList;

  enum Lists {
    cAll = 0,
    cTotal = 1
  };

  typedef sys::List<Process, cAll> AllList;

  sys::ListNode<Process> lists[cTotal];

private:
  Thread::ProcessList threads_;
  int pid_;

  MMapList mmaps_;

  MemoryMapping* break_mapping_;

  fs::File* fds_[16];
  ipc::Channel* channels_[16];

public:
  int pid() {
    return pid_;
  }

  int find_fd() {
    for(int i = 0; i < 16; i++) {
      if(fds_[i] == 0) return i;
    }

    return -1;
  }

  int add_channel(ipc::Channel* chan) {
    for(int i = 0; i < 16; i++) {
      if(channels_[i] == 0) {
        channels_[i] = chan;
        return i;
      }
    }

    return -1;
  }

  Process(int pid);

  void add_mmap(fs::Node* node, u32 offset, u32 size, u32 addr,
                u32 mem_size, int flags);
  MemoryMapping* find_mapping(u32 addr);
  u32 change_heap(int bytes);

  int open_file(const char* name, int mode);
  fs::File* get_file(int fd);
};

#endif
