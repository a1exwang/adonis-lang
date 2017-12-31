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

#include "../nvm_malloc/src/nvm_malloc.h"

using namespace std;

#ifdef LLVM_ON_WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif


extern "C" {
DLLEXPORT void howAreYou() {
  cout << "howareyou" << endl;
}

DLLEXPORT void nvmSetup() {
  nvm_initialize("nvm", 1);
}

DLLEXPORT void putsInt(int32_t i) {
  cout << i << endl;
}

DLLEXPORT int getIntNvmVar(int intId) {
  string name;
  stringstream ss;
  ss << intId;
  ss >> name;
  int *p = (int*) nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = (int *) nvm_reserve_id(name.c_str(), 4);
    *p = 0;
    nvm_persist(p, 4);
    nvm_activate_id(name.c_str());
  }
  return *p;
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
  string name;
  stringstream ss;
  ss << "nvm_" << id;
  ss >> name;
  void *p = nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = nvm_reserve_id(name.c_str(), size);
  }

  return p;
}
DLLEXPORT void persistNvmVar(int id, uint64_t size) {
  string name;
  stringstream ss;
  ss << "nvm_" << id;
  ss >> name;
  void *p = nvm_get_id(name.c_str());
  if (p == nullptr) {
    p = nvm_reserve_id(name.c_str(), size);
  }

  nvm_persist(p, size);
  nvm_activate_id(name.c_str());
}
}
