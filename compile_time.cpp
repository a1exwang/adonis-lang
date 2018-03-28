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

void al::CompileTime::traverseAll() {
  this->root->traverse(*this, *this->pvarTag);
  this->root->visit(*this);
}


void al::CompileTime::setupMainModule() {
  mainModule = llvm::make_unique<llvm::Module>("main", theContext);

}

al::CompileTime::CompileTime() :theContext(), strCounter(0), pvarTag(make_unique<PersistentVarTaggingPass>()) {
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
  auto alMainEnd = Function::Create(
      FunctionType::get(
          Type::getVoidTy(theContext),
          {},
          false
      ),
      Function::LinkageTypes::ExternalLinkage, "AL__main_end", getMainModule()
  );
  builder.CreateCall(alMainEnd, {});
  builder.CreateRet(ConstantInt::get(Type::getInt32Ty(theContext), 0));
}


al::CompileTime::~CompileTime() {
}

void al::CompileTime::finish1() {
}

llvm::Value *al::CompileTime::createGetIntNvmVar(const std::string &name) {
  if (!this->hasSymbol(name)) {
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
  if (!this->hasSymbol(name)) {
    cerr << "Persistent var not found '" << name << "'" << endl;
    abort();
  }
  int varId = name[name.size() - 1] - '0';
  auto objType = this->getSymbolType(name);
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
}

void al::CompileTime::registerBuiltinTypes() {

  std::vector<std::pair<std::string, llvm::Type*>> types = {
      {"int32", llvm::Type::getInt32Ty(theContext)},
      {"int8", llvm::Type::getInt8Ty(theContext)},
      {"void", llvm::Type::getVoidTy(theContext)},
  };
  for (auto &t1 : types) {
    auto name = t1.first;
    auto node = std::make_shared<al::ast::Type>(
        std::make_shared<ast::Symbol>(name.c_str()),
        ast::Type::None,
        t1.second
    );
    this->registerType(name, node);
  }
}

llvm::Value *al::CompileTime::createGetMemNvmVar(const std::string &name) {
  if (!this->hasSymbol(name)) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto type = this->getSymbolType(name);
  auto t = type->getLlvmType();
  if (t->isPointerTy()) {
    t = PointerType::get(t->getPointerElementType(), PtrAddressSpace::NVM);
  }

  return createGetMemNvmVar(PointerType::get(t, PtrAddressSpace::NVM), varId);
}

void al::CompileTime::createSetMemNvmVar(const std::string &name, llvm::Value *ptr) {
  if (!this->hasSymbol(name)) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto t = this->getSymbolType(name)->getLlvmType();
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
  if (!name.empty() && name[0] == '*') {
    auto derefType = name.substr(1, name.size() - 1);
    auto a = getType(derefType);
    auto llvmType = PointerType::get(a->getLlvmType(), al::PtrAddressSpace::Volatile);
    auto newType = std::make_shared<al::ast::Type>(std::make_shared<ast::Symbol>(name), ast::Type::Ptr, llvmType);
    this->typeTable[name] = newType;
  }
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

void al::CompileTime::createCommitPersistentVarIfOk(llvm::Value *nvmPtr, llvm::Value *size, llvm::Value *ok) {
  if (!nvmPtr->getType()->isPointerTy() || !size->getType()->isIntegerTy()) {
    cerr << "nvmPtr must be a ptr and size must be an integer" << endl;
    abort();
  }
  auto fn = getMainModule()->getOrInsertFunction(
      "persistNvmVarByAddr",
      FunctionType::get(
          Type::getVoidTy(theContext), {
              Type::getInt32PtrTy(theContext),
              Type::getInt64Ty(theContext),
              Type::getInt32Ty(theContext)
          },
          false
      )
  );

  getCompilerContext().builder->CreateCall(
      fn, {
          getCompilerContext().builder->CreatePointerCast(nvmPtr, Type::getInt32PtrTy(theContext)),
          size,
          ok,
      }
  );
}

void al::CompileTime::createAssignment(
    llvm::Type *elementType,
    llvm::Value *lhsPtr,
    llvm::Value *rhsVal,
    llvm::Value *rhsPtr,
    llvm::Value *persistNvm
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
    elementType->print(llvm::errs());
    abort();
  }

  if (persistNvm && lhsPtr->getType()->getPointerAddressSpace() == PtrAddressSpace::NVM) {
    createCommitPersistentVarIfOk(
        lhsPtr,
        getTypeSize(*builder, elementType),
        persistNvm
    );
  }
}

llvm::Value *al::CompileTime::getFunctionStackVariable(const std::string &functionName, const std::string &varName) {
  return this->functionStackVariables[functionName][varName];
}

bool al::CompileTime::hasFunctionStackVariable(const std::string &functionName, const std::string &varName) {
  if (this->functionStackVariables.find(functionName) == this->functionStackVariables.end()) {
    return false;
  }
  return this->functionStackVariables[functionName].find(varName) != this->functionStackVariables[functionName].end();
}

void al::CompileTime::setFunctionStackVariable(const std::string &functionName, const std::string &varName,
                                               llvm::Value *val) {
  if (this->functionStackVariables.find(functionName) == this->functionStackVariables.end()) {
    this->functionStackVariables[functionName] = std::map<std::string, llvm::Value*>();
  }
  this->functionStackVariables[functionName][varName] = val;
}

void al::CompileTime::registerSymbol(const std::string &name, std::shared_ptr<al::ast::Type> type) {
  if (type == nullptr || type->getLlvmType() == nullptr) {
    cerr << "type of '" << name << "' is nullptr" << endl;
    cerr << type->toString() << endl;
    abort();
  }
  this->globalSymbolTable[name] = type;
}

void al::CompileTime::registerType(const std::string &name, std::shared_ptr<al::ast::Type> type) {
  this->typeTable[name] = type;
}

void al::CompileTime::unsetFunctionStackVariable(const std::string &functionName, const std::string &varName) {

  auto it = this->functionStackVariables.find(functionName);
  if (it != this->functionStackVariables.end()) {
    auto it2 = it->second.find(varName);
    it->second.erase(it2);
  }
}

