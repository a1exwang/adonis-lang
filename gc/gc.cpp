#include <iostream>
#include <vector>
#include <cstring>
#include <iostream>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/detail/pexceptions.hpp>


#include "gc.h"
#include "misc/log.h"

using namespace std;


pmem::obj::persistent_ptr<AlgcBlock>
Algc::allocate(
    uint64_t size,
    const uint64_t pointerOffsets[],
    int64_t nOffsets) {

  if (this->options == TriggerOptions::OnAllocation) {
    if (*this->poolRoot->blockCount >= this->gcBlockCountThreshold) {
      this->doGc();
    }
  }

  pmem::obj::persistent_ptr<AlgcBlock> ret(nullptr);

  auto totalSize = size + sizeof(PMEMoid);

//  pmem::obj::transaction::exec_tx(this->pool, [&] {
  auto data = pmem::obj::make_persistent<char[]>(totalSize);
  memset(data.get() + sizeof(PMEMoid), 0, size);

  auto persistentPointerOffsets = pmem::obj::make_persistent<uint64_t[]>((uint64_t)nOffsets);
  memcpy(persistentPointerOffsets.get(), pointerOffsets, sizeof(uint64_t) * nOffsets);

  ret = this->poolRoot->rootAllObjs->append(
      totalSize,
      persistentPointerOffsets,
      nOffsets,
      data
  );
  ret->setBackPmemPtr(ret.raw());

  (*poolRoot->blockCount)++;
//  });

  return ret;
}

void Algc::doGc() {
  // mark stage
  this->doMark();

  // sweep stage
  int i = 0;
  auto it = this->poolRoot->rootAllObjs->begin(), itEnd = this->poolRoot->rootAllObjs->end();
  while (it != itEnd) {
    if (!(*it)->mark) {
      pmem::obj::transaction::exec_tx(pool, [&] {
        auto block = *it;
        if (sweepCallback) {
          sweepCallback(block->data);
        }
        it = it.detach();
        pmem::obj::delete_persistent<char[]>(block->data, block->dataSize);
        pmem::obj::delete_persistent<AlgcBlock>(block);
        (*this->poolRoot->blockCount)--;
      });
    } else {
      it++;
    }
  }
  AlLogger::Logger::getStdErrLogger() <<  "Algc::doGc() deleted " << i << " objects\n";
}

Algc::Algc(
    const std::string &poolName,
    const std::string &layoutName,
    uint64_t maxSize,
    Algc::TriggerOptions options,
    int gcBlockCountThreshold) : poolName(poolName),
                                 layoutName(layoutName),
                                 maxSize(maxSize),
                                 options(options),
                                 gcBlockCountThreshold(gcBlockCountThreshold) {

  try {
    this->pool = pmem::obj::pool<AlgcPmemRoot>::create(poolName, layoutName, maxSize);
  } catch(pmem::pool_error &pe) {
    this->pool = pmem::obj::pool<AlgcPmemRoot>::open(poolName, layoutName);
  }

  this->poolRoot = this->pool.get_root();
  pmem::obj::transaction::exec_tx(pool, [&]() -> void {
    if (this->poolRoot->blockCount == nullptr) {
      this->poolRoot->blockCount = pmem::obj::make_persistent<int64_t>(0);
    }
    if (this->poolRoot->rootAllObjs == nullptr) {
      this->poolRoot->rootAllObjs = AlgcBlock::createHead();
    }
    if (this->poolRoot->rootGc == nullptr) {
      this->poolRoot->rootGc = AlgcBlock::createHead();
    }
  });
}

Algc::~Algc() {
  this->pool.close();
}

void Algc::doMark() {
  /**
   * We do not need transactions here, because we only update item->mark, which will not lose any information if lost.
   */
  // mark all nodes as `false`
  for (auto item : *this->poolRoot->rootAllObjs) {
    item->mark = false;
  }

  /** Mark stage */
  // Callback roots
  if (this->gcRootsCallback) {
    uint64_t n;
    auto ptr = this->gcRootsCallback(n);
    for (int i = 0; i < n; ++i) {
      ptr[i]->doMark(this->markCallback);
    }
  }
  // Static roots
//  for (auto item : *this->poolRoot->rootGc) {
//    item->doMark(this->markCallback);
//  }
}


void AlgcBlock::doMark(std::function<void (pmem::obj::persistent_ptr<void>)> markCallback) {
  AlLogger::Logger::getStdErrLogger().d()
      << "AlgcBlock::doMark(): myBlockOid=("
      << std::hex << this->getBackPmemPtr().pool_uuid_lo << ", "
      << std::hex << this->getBackPmemPtr().off
      << ")\n";

  // prevent loop pointers
  if (this->mark)
    return;

  this->mark = true;
  if (markCallback)
    markCallback(this->data);
  for (int64_t i = 0; i < this->nOffsets; i++) {
    char *userDataPtr = this->data.get() + sizeof(PMEMoid);
    auto anotherAlgcPmemObjPtr = userDataPtr + this->pointerOffsets[i];

    PMEMoid childOid = AlgcBlock::getBackPmemPtr(anotherAlgcPmemObjPtr);
    pmem::obj::persistent_ptr<PMEMoid> childUserPtr(childOid);
    if (childUserPtr == nullptr)
      return;
    pmem::obj::persistent_ptr<AlgcBlock> childBlkPtr(*childUserPtr);
    AlLogger::Logger::getStdErrLogger().d()
        << "AlgcBlock::doMark(): childBlock=("
        << std::hex << childBlkPtr.raw().pool_uuid_lo << ", "
        << std::hex << childBlkPtr.raw().off
        << ")\n";
    childBlkPtr->doMark(markCallback);
  }
}

pmem::obj::persistent_ptr<AlgcBlock>
AlgcBlock::append(uint64_t dataSize, pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets, int64_t nOffsets,
    pmem::obj::persistent_ptr<char[]> data) {

  auto last2 = this->prev;
  pmem::obj::persistent_ptr<AlgcBlock> ret = pmem::obj::make_persistent<AlgcBlock>(
      last2,
      this,
      dataSize,
      pointerOffsets,
      nOffsets,
      data
  );

  last2->next = ret;
  this->prev = ret;
  return ret;
}

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::createHead() {
  auto ret = pmem::obj::make_persistent<AlgcBlock>(nullptr, nullptr, 0, nullptr, 0, nullptr);
  ret->next = ret;
  ret->prev = ret;
  return ret;
}

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::detach() {
  auto ret = this->next;
  this->next->prev = this->prev;
  this->prev->next = this->next;
  this->next = nullptr;
  this->prev = nullptr;
  return ret;
}

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::createFromDataPtr(void *p, uint64_t size) {
  auto oid = *(PMEMoid*)((char*)p + size);
  return pmem::obj::persistent_ptr<AlgcBlock>(oid);
}
