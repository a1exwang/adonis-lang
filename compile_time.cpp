#include "compile_time.h"
#include "ast.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "compile_time.h"
#include <nvm_malloc.h>

using namespace llvm;
using namespace std;

void al::CompileTime::traverse1() {
  this->root->visit(*this);
}


void al::CompileTime::setupMainModule() {
  mainModule = llvm::make_unique<llvm::Module>("main", theContext);

}

al::CompileTime::CompileTime() :theContext(), strCounter(0) {
}

llvm::Module* al::CompileTime::getMainModule() const { return mainModule.get(); }

void al::CompileTime::init1() {
  setupMainModule();
  registerBuiltinTypes();
  createMainFunc();
}

void al::CompileTime::createMainFunc() {
  Function *func = Function::Create(
      FunctionType::get(Type::getInt32Ty(theContext), {}),
      Function::ExternalLinkage,
      "main",
      getMainModule()
  );
  mainFunction = func;

  BasicBlock *BB = BasicBlock::Create(theContext, "entry", func);
  IRBuilder<> builder(theContext);
  builder.SetInsertPoint(BB);

  builder.CreateCall(
      Function::Create(
          FunctionType::get(
              llvm::Type::getVoidTy(theContext),
              {},
              false
          ),
          Function::LinkageTypes::ExternalLinkage, "alLibInit", getMainModule()
      ),
      {}
  );

  builder.CreateCall(
      Function::Create(
          FunctionType::get(
              llvm::Type::getVoidTy(theContext),
              {},
              false
          ),
          Function::LinkageTypes::ExternalLinkage, "threadLocalSetupMain", getMainModule()
      ),
      {}
  );

  builder.CreateCall(Function::Create(
      FunctionType::get(
          Type::getVoidTy(theContext),
          {},
          false
      ),
      Function::LinkageTypes::ExternalLinkage, "nvmSetup", getMainModule()
  ), {});

  auto userFn = Function::Create(
      FunctionType::get(
          Type::getVoidTy(theContext),
          {},
          false
      ),
      Function::LinkageTypes::ExternalLinkage, "AL__main", getMainModule()
  );
  builder.CreateCall(userFn, {});
  builder.CreateRet(ConstantInt::get(Type::getInt32Ty(theContext), 0));
}


al::CompileTime::~CompileTime() {
}

void al::CompileTime::finish1() {
}

llvm::Value *al::CompileTime::createGetIntNvmVar(const std::string &name) {
  if (this->persistentSymbolTable.find(name) == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';

  auto fn = getMainModule()->getOrInsertFunction(
      "getIntNvmVar",
      FunctionType::get(
          Type::getInt32Ty(theContext), {Type::getInt32Ty(theContext)}, false
      )
  );
  return getCompilerContext().builder->CreateCall(fn, {ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)varId)});
}

void al::CompileTime::createSetPersistentVar(const std::string &name, llvm::Value *value) {
  auto pPvType = this->persistentSymbolTable.find(name);
  if (pPvType == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found '" << name << "'" << endl;
    abort();
  }
  auto pvType = pPvType->second;

  int varId = name[name.size() - 1] - '0';
  if (this->hasType(pvType)) {
    auto objType = this->getType(pvType);
    if (objType->getLlvmType()->isIntegerTy(32)) {
      auto fn = getMainModule()->getOrInsertFunction(
          "setIntNvmVar",
          FunctionType::get(
              Type::getVoidTy(theContext), {Type::getInt32Ty(theContext), Type::getInt32Ty(theContext)}, false
          )
      );
      getCompilerContext().builder->CreateCall(fn, {ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)varId), value});
    }
    else if (objType->getLlvmType()->isStructTy()) {
      cerr << "unimplemented for struct type" << endl;
      abort();
    }
    else {
      cerr << "unsupported type" << endl;
      objType->getLlvmType()->dump();
      abort();
    }

  }
}

void al::CompileTime::registerPersistentVar(const std::string &name, const std::string &type) {
  this->persistentSymbolTable[name] = type;
}

void al::CompileTime::registerBuiltinTypes() {

  std::vector<std::pair<std::string, llvm::Type*>> types = {
      {"int32", llvm::Type::getInt32Ty(theContext)},
      {"*int32", llvm::Type::getInt32PtrTy(theContext)},
      {"int8", llvm::Type::getInt8Ty(theContext)},
      {"*int8", llvm::Type::getInt8PtrTy(theContext)},
      {"void", llvm::Type::getVoidTy(theContext)},
  };
  for (auto &t1 : types) {
    auto name = t1.first;

    bool isPtr = t1.second->isPointerTy();
    auto node = std::make_shared<al::ast::Type>(
        std::make_shared<ast::Symbol>(name.c_str()),
        isPtr ? ast::Type::Ptr : ast::Type::None,
        t1.second
    );
    this->registerType(name, node);
  }
}

llvm::Value *al::CompileTime::createGetMemNvmVar(const std::string &name) {
  if (this->persistentSymbolTable.find(name) == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto t = this->getType(this->getPersistentVarType(name))->getLlvmType();
  if (t->isPointerTy()) {
    t = PointerType::get(t->getPointerElementType(), PtrAddressSpace::NVM);
  }

  return createGetMemNvmVar(PointerType::get(t, PtrAddressSpace::NVM), varId);
}

void al::CompileTime::createSetMemNvmVar(const std::string &name, llvm::Value *ptr) {
  if (this->persistentSymbolTable.find(name) == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto t = this->getType(this->getPersistentVarType(name))->getLlvmType();
  auto sizeVal = this->getCompilerContext().builder->CreatePtrToInt(
      this->getCompilerContext().builder->CreateGEP(
          t,
          ConstantPointerNull::get(PointerType::get(t, 0)),
          ConstantInt::get(Type::getInt32Ty(theContext), 1)
      ),
      Type::getInt64Ty(theContext)
  );


  auto fn = getMainModule()->getOrInsertFunction(
      "persistNvmVar",
      FunctionType::get(
          PointerType::get(Type::getInt8Ty(theContext), 0),
          {Type::getInt32Ty(theContext), Type::getInt64Ty(theContext)},
          false
      )
  );
  getCompilerContext().builder->CreateCall(
      fn,
      {
          ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)varId),
          sizeVal,
      }
  );
}

llvm::Value *al::CompileTime::getTypeSize(IRBuilder<> &builder, llvm::Type *s) {
  auto sizeVal = builder.CreatePtrToInt(
      builder.CreateGEP(
          s,
          ConstantPointerNull::get(PointerType::get(s, 0)),
          ConstantInt::get(Type::getInt32Ty(builder.getContext()), 1)
      ),
      Type::getInt64Ty(builder.getContext())
  );
  return sizeVal;
}

llvm::Value *al::CompileTime::createGetMemNvmVar(llvm::PointerType *nvmPtrType, int id) {
  auto sizeVal = this->getCompilerContext().builder->CreatePtrToInt(
      this->getCompilerContext().builder->CreateGEP(
          nvmPtrType->getElementType(),
          ConstantPointerNull::get(nvmPtrType),
          ConstantInt::get(Type::getInt32Ty(theContext), 1)
      ),
      Type::getInt64Ty(theContext)
  );

  auto fn = getMainModule()->getOrInsertFunction(
      "getNvmVar",
      FunctionType::get(
          PointerType::get(Type::getInt8Ty(theContext), 0),
          {Type::getInt32Ty(theContext), Type::getInt64Ty(theContext)},
          false
      )
  );
  return getCompilerContext().builder->CreatePointerCast(
      getCompilerContext().builder->CreateCall(
          fn,
          {
              ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)id),
              sizeVal,
          }
      ),
      nvmPtrType
  );
}

std::shared_ptr<al::ast::Type> al::CompileTime::getType(const std::string &name) {
//  if (!name.empty() && name[0] == '*') {
//    ObjType ret;
//    auto derefType = name.substr(1, name.size() - 1);
//    auto a = getType(derefType);
//    ret.llvmType = PointerType::get(a.llvmType, 0);
//    ret.name = name;
//    return ret;
//  }
  return this->typeTable[name];
}

bool al::CompileTime::hasType(const std::string &name) const {
  if (!name.empty() && name[0] == '*') {
    auto derefType = name.substr(1, name.size() - 1);
    return hasType(derefType);
  }
  else {
    return this->typeTable.find(name) != this->typeTable.end();
  }
}

void al::CompileTime::createCommitPersistentVar(llvm::Value *nvmPtr, llvm::Value *size) {
  if (!nvmPtr->getType()->isPointerTy() || !size->getType()->isIntegerTy()) {
    cerr << "nvmPtr must be a ptr and size must be an integer" << endl;
    abort();
  }
  auto fn = getMainModule()->getOrInsertFunction(
      "persistNvmVarByAddr",
      FunctionType::get(
          Type::getVoidTy(theContext), {
              Type::getInt32PtrTy(theContext),
              Type::getInt64Ty(theContext)
          },
          false
      )
  );

  getCompilerContext().builder->CreateCall(
      fn, {
          getCompilerContext().builder->CreatePointerCast(nvmPtr, Type::getInt32PtrTy(theContext)),
          size
      }
  );
}

void al::CompileTime::createAssignment(
    llvm::Type *elementType,
    llvm::Value *lhsPtr,
    llvm::Value *rhsVal,
    llvm::Value *rhsPtr
) {
  auto builder = getCompilerContext().builder;
  if (elementType->isIntegerTy(32) ||
      elementType->isPointerTy() ||
      elementType->isStructTy()) {
    auto a = static_cast<llvm::PointerType*>(lhsPtr->getType());
    auto vPtr = llvm::PointerType::get(a->getElementType(), PtrAddressSpace::Volatile);
    auto lhsNewPtr = builder->CreatePointerCast(lhsPtr, vPtr);

    builder->CreateStore(rhsVal, lhsPtr);
  }
  else {
    cerr << "type not supported" << endl;
    elementType->dump();
    abort();
  }

  // TODO support more types and do this only on nvm
//      if (lhsPtr->getType()->getPointerAddressSpace() == PtrAddressSpace::NVM) {
  createCommitPersistentVar(
      lhsPtr,
      getTypeSize(*builder, elementType)
  );
//      }
}
