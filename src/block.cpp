#include "block.hpp"
#include "kheap.hpp"
#include "console.hpp"
#include "fs/devfs.hpp"

#include "block_buffer.hpp"

namespace block {
  Registry registry;

  void Registry::init() {
    used_ = 0;

    for(int i = 0; i < max_devices; i++) {
      devices_[i] = 0;
    }
  }

  int Registry::add(Device* dev) {
    ASSERT(used_ < max_devices);
    dev->id_ = ++used_;

    devices_[dev->id_] = dev;

    devfs::main.add_block_device(dev, dev->name());

    return dev->id_;
  }

  void Registry::print() {
    for(int i = 1; i < max_devices; i++) {
      if(Device* dev = devices_[i]) {
        console.printf("%4d: %s\n", i, dev->name());
      }
    }
  }

  block::Buffer* Device::request(block::RegionRange range) {
    block::Buffer* buffer;
    if(cache_.fetch(range, &buffer)) {
      return buffer;
    }

    u32 num_bytes = range.num_bytes();
    buffer = block::Buffer::for_size(this, num_bytes);
    buffer->set_range(range);

    cache_.store(range, buffer);

    return buffer;
  }

  u32 Device::read_bytes(u32 offset, u32 size, u8* out_buffer) {
    u32 cur_region = offset / cRegionSize;

    u32 initial_offset = offset % cRegionSize;

    u32 left = size;

    // Do the first block by itself to deal with the
    // initial offset here.
    block::Buffer* buffer = request(block::RegionRange(cur_region, 1));
    buffer->wait();

    if(left > BlockSize) {
      u32 copy_bytes = cRegionSize - initial_offset;
      memcpy(out_buffer, buffer->data() + initial_offset, copy_bytes);
      out_buffer += copy_bytes;
      left -= copy_bytes;
    } else {
      // Satisfied all out of the first block! Woo!
      memcpy(out_buffer, buffer->data() + initial_offset, left);
      return size;
    }

    while(left > 0) {
      cur_region++;

      buffer = request(block::RegionRange(cur_region, 1));
      buffer->wait();

      if(left > cRegionSize) {
        memcpy(out_buffer, buffer->data(), cRegionSize);
        left -= cRegionSize;
        out_buffer += BlockSize;
      } else {
        memcpy(out_buffer, buffer->data(), left);
        left = 0;
      }
    }

    return size;
  }

  struct DosPartionEntry {
    u8 status;
    u8 chs0;
    u8 chs1;
    u8 chs2;
    u8 type;
    u8 chs_end0;
    u8 chs_end1;
    u8 chs_end2;
    u32 lba;
    u32 sectors;
  };

  void Device::detect_partitions() {
    block::RegionRange range(0, 1);

    block::Buffer* buffer = request(range);

    buffer->busy_wait();

    u8* buf = buffer->data();

    if(buf[510] == 0x55 && buf[511] == 0xAA) {
      signature_ = *(u32*)(buf + 440);
      DosPartionEntry* entry = (DosPartionEntry*)(buf + 446);
      for(int i = 0; i < 4; i++) {
        if(entry->type != 0) {
          sys::FixedString<8> name = name_.c_str();
          name << ('0' + i);

          console.printf("%s: type=%x %dM @ %x\n",
                         name.c_str(), entry->type,
                         entry->sectors / 2048, entry->lba);

          block::SubDevice* dev = new(kheap) block::SubDevice(
              name.c_str(), this, entry->lba, entry->sectors);

          block::registry.add(dev);
        }
        entry++;
      }
    }

    kfree(buf);
  }

  void SubDevice::fulfill(Buffer* buf) {
    parent_->fulfill(buf);
  }
}
