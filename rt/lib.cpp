#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <chrono>

#include "../nvm_malloc/src/nvm_malloc.h"
#include <thread>

using namespace std;

#ifdef LLVM_ON_WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

string getNvmVarNameById(const string &threadName, int id) {
  string name;
  stringstream ss;
  ss << "nvm_" << threadName << "_" << id;
  ss >> name;
  return name;
}

std::map<std::thread::id, std::string> *threadNames;
// thread, relptr, length
std::map<std::tuple<string, void*, uint64_t>, string> nvmVarMap;

extern "C" {

DLLEXPORT void alLibInit() {
  threadNames = new std::map<std::thread::id, std::string>;
}

DLLEXPORT void howAreYou() {
  cout << "howareyou" << endl;
}

DLLEXPORT void nvmSetup() {
  nvm_initialize("nvm", 1);
}

DLLEXPORT void threadLocalSetup(const char *name) {
  (*threadNames)[std::this_thread::get_id()] = name;
  nvm_initialize("nvm", 1);
}

DLLEXPORT void threadLocalSetupMain() {
  (*threadNames)[std::this_thread::get_id()] = "main";
  nvm_initialize("nvm", 1);
}

DLLEXPORT const char *getThreadName() {
  return (*threadNames)[std::this_thread::get_id()].c_str();
}

DLLEXPORT int32_t plus(int32_t a, int32_t b) {
  return a + b;
}

DLLEXPORT void putsInt(int32_t i) {
  cout << i << endl;
}

DLLEXPORT void putsInt8Str(const char *str) {
  cout << str << endl;
}

DLLEXPORT void setIntNvmVar(int intId, int value) {
  string name;
  stringstream ss;
  ss << intId;
  ss >> name;
  int *p = (int*) nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = (int *) nvm_reserve_id(name.c_str(), 4);
  }

  *p = value;
  nvm_persist(p, 4);
  nvm_activate_id(name.c_str());
}


DLLEXPORT void *getNvmVar(int id, uint64_t size) {
  auto name = getNvmVarNameById(getThreadName(), id);
  void *p = nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = nvm_reserve_id(name.c_str(), size);
  }
  nvmVarMap[{getThreadName(), p, size}] = name;

  return p;
}
DLLEXPORT int getIntNvmVar(int intId) {
  int *p = (int*)getNvmVar(intId, 4);
  return *p;
}
DLLEXPORT void persistNvmVar(int id, uint64_t size) {
  auto name = getNvmVarNameById(getThreadName(), id);
  void *p = nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = nvm_reserve_id(name.c_str(), size);
  }

  nvm_persist(p, size);
  nvm_activate_id(name.c_str());
}

DLLEXPORT void persistNvmVarByAddr(char *ptr, uint64_t size) {
  nvm_persist(ptr, size);

  if (nvmVarMap.find({getThreadName(), ptr, size}) != nvmVarMap.end()) {
    auto name = nvmVarMap[{getThreadName(), ptr, size}];
    nvm_activate_id(name.c_str());
    return;
  }
  else {
    // TODO, optimize, get name with a sorted list and bin-search
    for (auto item : nvmVarMap) {
      string threadName;
      void *relPtr;
      uint64_t len;
      tie(threadName, relPtr, len) = item.first;
      if (threadName == getThreadName() && relPtr <= ptr && ptr < (char*)relPtr + len) {
        auto name = item.second;
//        cerr << "inner nvm memory found: " << name << ", " << (void*)ptr << ", offset " << (void*)(ptr - (char*)item.first.first) << endl;
        nvm_activate_id(name.c_str());
        return;
      }
    }
  }

//  cerr << "nvm memory nout found: " << (void*)ptr << " " << size << endl;
}

DLLEXPORT void nvAllocInt32(int **i32) {
  *i32 = nullptr;
  auto ptr = nvm_reserve(sizeof(int));
  nvm_persist(ptr, sizeof(int));
  nvm_activate(ptr, (void **)i32, ptr, nullptr, nullptr);
  // TODO i32 is a relative pointer, but in al we assure all pointers are absolute pointers
  *i32 = (int*)nvm_abs(*i32);
}

DLLEXPORT int thread(void (*thread_fn)(int), int val) {
  std::thread t([&thread_fn, val]() -> void {
    thread_fn(val);
  });
  t.detach();
  return 0;
}

DLLEXPORT int *tic() {
  auto tp = std::chrono::high_resolution_clock::now();
  auto ptp = new decltype(tp)(tp);
  return (int*)ptp;
}
DLLEXPORT int toc(int *ticVal) {
  auto end = chrono::high_resolution_clock::now();
  auto start = (decltype(end)*)ticVal;
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - *start).count();
  return (int)ns;
}


}
