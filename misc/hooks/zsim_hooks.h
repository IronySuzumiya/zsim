#ifndef __ZSIM_HOOKS_H__
#define __ZSIM_HOOKS_H__

#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fmt/format.h>
#include <functional>

//Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory");}

//These need to be in sync with the simulator
#define ZSIM_MAGIC_OP_ROI_BEGIN         (1025)
#define ZSIM_MAGIC_OP_ROI_END           (1026)
#define ZSIM_MAGIC_OP_REGISTER_THREAD   (1027)
#define ZSIM_MAGIC_OP_HEARTBEAT         (1028)
#define ZSIM_MAGIC_OP_WORK_BEGIN        (1029) //ubik
#define ZSIM_MAGIC_OP_WORK_END          (1030) //ubik

#ifdef __x86_64__
#define HOOKS_STR  "HOOKS"
static inline void zsim_magic_op(uint64_t op) {
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
}
#else
#define HOOKS_STR  "NOP-HOOKS"
static inline void zsim_magic_op(uint64_t op) {
    //NOP
}
#endif

static inline void zsim_roi_begin() {
    printf("[" HOOKS_STR "] ROI begin\n");
    zsim_magic_op(ZSIM_MAGIC_OP_ROI_BEGIN);
}

static inline void zsim_roi_end() {
    zsim_magic_op(ZSIM_MAGIC_OP_ROI_END);
    printf("[" HOOKS_STR  "] ROI end\n");
}

static inline void zsim_heartbeat() {
    zsim_magic_op(ZSIM_MAGIC_OP_HEARTBEAT);
}

static inline void zsim_work_begin() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_BEGIN); }
static inline void zsim_work_end() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_END); }

// nfp 2023-6-7
struct FlashAddress {
  uint32_t channel;
  uint32_t chip;
  uint32_t die;
  uint32_t plane;
  uint32_t block;
  uint32_t page;

  std::string to_string() const {
    return fmt::format("{}, {}, {}, {}, {}, {}", channel, chip, die, plane, block, page);
  }

  template<typename OStream>
  friend OStream &operator<<(OStream &os, const FlashAddress& addr) {
    return os << addr.channel << ", " << addr.chip << ", " << addr.die << ", " << addr.plane << ", " << addr.block << ", " << addr.page;
  }
};

enum class SSDRequestType {
  READ,
  WRITE
};

struct SSDRequest {
  SSDRequestType type;
  std::vector<FlashAddress> addrs;
  uint32_t bytes;
  bool finished;
  std::function<void(void)> callback;

  uint32_t thread_id;
  uint64_t issued_cycle;
};

static inline void mqsim_read_flash(SSDRequest* req) {
  COMPILER_BARRIER();
  __asm__ __volatile__("clflush %0" : "+m" (*(volatile SSDRequest*)req));
  COMPILER_BARRIER();
}

#endif /*__ZSIM_HOOKS_H__*/
