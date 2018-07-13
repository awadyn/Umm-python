//          Copyright Boston University SESA Group 2013 - 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef UMM_UM_MANAGER_H_
#define UMM_UM_MANAGER_H_

#include <ebbrt/EbbId.h> 
#include <ebbrt/GlobalStaticIds.h>
#include <ebbrt/MulticoreEbb.h>
#include <ebbrt/native/VMemAllocator.h>

#include "umm-common.h"
#include "umm-x86_64.h"
#include "UmInstance.h"
#include "UmState.h"

namespace umm {

/* Execution slot constants */ 
const uintptr_t kSlotStartVAddr = 0xFFFFC00000000000;
const uintptr_t kSlotEndVAddr = 0xFFFFC07FFFFFFFFF;
const uint64_t kSlotPageLength = 0x7FFFFFF;
const uint16_t kSlotPML4Offset = 0x180;

/**
 *  UmManager - MultiCore Ebb that manages per-core executions of SV instances
 */
class UmManager : public ebbrt::MulticoreEbb<UmManager> {
public:
  static const ebbrt::EbbId global_id = ebbrt::GenerateStaticEbbId("UmManager");
  /** Execution slot status */
  enum Status : uint8_t { empty = 0, loaded, running, snapshot, finished };

  /** Class-wide static initialization logic */
  static void Init(); 

  /** Load Instances into the core */
  void Load(std::unique_ptr<UmInstance>);

  /** Start execution of loaded instance */
  void Start(); // TODO(jmcadden): Rename as Enter?

  /** Extract an SV at a given symbol */
  ebbrt::Future<UmState> SetCheckpoint(uintptr_t vaddr);

  /** Remove instance from core */
  std::unique_ptr<UmInstance> Unload();

  /* Utility methods */ 
  void process_pagefault(ExceptionFrame *ef, uintptr_t addr);
  void process_resume(ExceptionFrame *ef);
  void process_checkpoint(ExceptionFrame *ef);
  Status status() { return status_.get(); } ; 
  void set_status( Status s ) { return status_.set(s);}

private:
  /** 
    * PageFaultHandler for the EbbRT VMem subsystem
    */
  class PageFaultHandler : public ebbrt::VMemAllocator::PageFaultHandler {
  public:
    void HandleFault(ebbrt::idt::ExceptionFrame *ef, uintptr_t addr) override;
  };

  /**  //TODO: Rename SlotStatus 
    * Status tracks the status and runtime of slot exection
    */
  class UmmStatus {
  public:
    UmManager::Status get() { return s_; }
    void set(UmManager::Status);
    uint64_t time() { return runtime_; /* in milliseconds */ }
  private:
    UmManager::Status s_ = empty;
    ebbrt::clock::Wall::time_point clock_;
    uint64_t runtime_ = 0;
  }; // UmmStatus

  /* Internals */
  void trigger_entry_exception() { __asm__ __volatile__("int3"); };
  inline bool valid_address(uintptr_t vaddr) {
    return ((vaddr >= umm::kSlotStartVAddr) && (vaddr < umm::kSlotEndVAddr));
  }
  /* Session specific values */
  UmmStatus status_; // TODO: SlotStatus
  // TODO: Move some of these into the Instance ??? 
  ExceptionFrame caller_restore_frame_; 
  ExceptionFrame snap_restore_frame_; 
  std::unique_ptr<UmInstance> umi_;
  // TODO: Reusables or multi-promises 
  ebbrt::Promise<UmState> umi_snapshot_;
};

constexpr auto manager = 
    ebbrt::EbbRef<UmManager>(UmManager::global_id);
}

#endif // UMM_UM_MANAGER_H_
