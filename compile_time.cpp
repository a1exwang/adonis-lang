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

void al::CompileTime::createFunction(Module &module, const string &name, vector<string> paramNames) {
  IRBuilder<> builder(theContext);
  // Declare function
  vector<Type *> params(paramNames.size(), Type::getDoubleTy(theContext));
  FunctionType *ft =
      FunctionType::get(Type::getDoubleTy(theContext), params, false);
  Function *func = Function::Create(ft, Function::ExternalLinkage, name, &module);

  int i = 0;
  for (auto &arg : func->args()) {
    arg.setName(paramNames[i]);
    i++;
  }
}

void al::CompileTime::setupMainModule() {
  mainModule = llvm::make_unique<llvm::Module>("main", theContext);

}

al::CompileTime::CompileTime() :theContext(), builder(theContext), strCounter(0) {
}

llvm::BasicBlock *al::CompileTime::createFunctionBody(llvm::Module &module, const std::string &name) {
  Function *fn = module.getFunction(name);
  return BasicBlock::Create(theContext, "__entry", fn);
}

llvm::Module* al::CompileTime::getMainModule() const { return mainModule.get(); }

void al::CompileTime::createFnFunc() {
  IRBuilder<> builder(theContext);
  // Declare function
  vector<Type *> params = vector<Type*>(3, getValuePtrType()); //name
  FunctionType *ft = FunctionType::get(getValuePtrType(), params, false);

  Function *func = Function::Create(ft, Function::ExternalLinkage, "fn", getMainModule());

  string argNames[] = {"name", "args", "statements"};

  int i = 0;
  for (auto &arg : func->args()) {
    if (i < 2) {
      arg.setName(argNames[i]);
    }
    i++;
  }

  BasicBlock *bb = BasicBlock::Create(theContext, "entry", func);
  IRBuilder<> builder1(bb);
  auto s = createStringValuePtr("return", builder1);
  auto a = builder1.CreatePointerCast(s, getValuePtrType());
  builder1.CreateRet(a);
  verifyFunction(*func);
}

llvm::StructType *al::CompileTime::getStringType() const {
  return stringType;
}

void al::CompileTime::createPlaceHolderFunc(const std::string &name, int n) {
  vector<llvm::Type*> types(n, getValuePtrType());
  FunctionType *ft = FunctionType::get(getValuePtrType(), types, false);
  Function *func = Function::Create(ft, Function::ExternalLinkage, name, getMainModule());

  BasicBlock *BB = BasicBlock::Create(theContext, "entry", func);
  IRBuilder<> builder1(theContext);
  builder1.SetInsertPoint(BB);
  builder1.CreateRet(llvm::ConstantPointerNull::get(getValuePtrType()));
  verifyFunction(*func);
}

void al::CompileTime::createPrimitiveTypes() {
  // int len; char *data;
  /**
   * Create 'String' Type
   * */
  // Value type consists of
  //  String, Int
  valueType = StructType::create(
      theContext,
      "Value");
  valuePtrType = PointerType::get(valueType, 0);

  stringType = llvm::StructType::create(theContext, {
      llvm::Type::getInt32Ty(theContext),
      llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(theContext)),
  }, "String");

  arrayType = StructType::create(
      theContext,
      {
          Type::getInt32Ty(theContext), // length,
          PointerType::getUnqual(valueType)
      },
      "Array"
  );

  valueType->setBody(
      {
          Type::getInt32Ty(theContext), // object type

          getStringType(),
          Type::getInt32Ty(theContext),
          arrayType,
      });

}

void al::CompileTime::init1() {
  setupMainModule();
//  createPrimitiveTypes();
  registerBuiltinTypes();
//  createLibFunc();
//  createFnFunc();
//  createPutsFunc();

//  createPlaceHolderFunc("statements", 3);
//  createPlaceHolderFunc("take-last", 3);
//  createPlaceHolderFunc("puts", 1);
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
  builder.SetInsertPoint(BB);
  pushCurrentBlock(BB);

  /* declares stdlib functions */
  /* main function starts */
//  auto ft = FunctionType::get(Type::getVoidTy(theContext), {});
  this->howAreYou = Function::Create(
      FunctionType::get(
          Type::getVoidTy(theContext),
          {},
          false
      ),
      Function::LinkageTypes::ExternalLinkage, "howAreYou", getMainModule()
  );

  builder.CreateCall(this->howAreYou, {});

  builder.CreateCall(Function::Create(
      FunctionType::get(
          IntegerType::getVoidTy(theContext),
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
}

llvm::Value *al::CompileTime::createStringValuePtr(const std::string &s, IRBuilder<> &builder) {

  auto strObj = builder.CreateAlloca(
      getStringType(),
      0,
      ConstantInt::get(Type::getInt32Ty(theContext), 1)
  );
  auto dataPtr = builder.CreateGEP(
      getStringType(),
      strObj,
      {ConstantInt::get(Type::getInt32Ty(theContext), 0),
       ConstantInt::get(Type::getInt32Ty(theContext), 1)}
  );
  // strObj.length = s.size()
  builder.CreateStore(
      ConstantInt::get(Type::getInt32Ty(theContext), s.size()),
      builder.CreateGEP(
          getStringType(),
          strObj,
          {ConstantInt::get(Type::getInt32Ty(theContext), 0),
           ConstantInt::get(Type::getInt32Ty(theContext), 0)}
      )
  );
  // global
  string name = nextConstVarName();
  auto gVar = builder.CreateGlobalString(s, "a");
  // strObj.data = (int8*)@.strXX
  builder.CreateStore(
      builder.CreateBitCast(
          gVar, PointerType::get(Type::getInt8Ty(theContext), 0)),
      dataPtr
  );
  return strObj;
}

llvm::PointerType *al::CompileTime::getStringPtrType() const {
  return llvm::PointerType::get(stringType, 0);
}

llvm::PointerType *al::CompileTime::getValuePtrType() {
  return valuePtrType;
}

al::CompileTime::~CompileTime() {
}

void al::CompileTime::finish1() {
  /* Generate call entry function */
//  FunctionType *ft = FunctionType::get(
//      Type::getVoidTy(theContext), {
//          getValuePtrType(), getValuePtrType()
//      }, false
//  );
//  auto entryFn = getMainModule()->getOrInsertFunction("entry", ft);
//  builder.CreateCall(entryFn, {
//      builder.CreatePointerCast(createStringValuePtr("1", builder), getValuePtrType()),
//      builder.CreatePointerCast(createStringValuePtr("hello", builder), getValuePtrType()),
//  });

  builder.CreateRet(ConstantInt::get(Type::getInt32Ty(theContext), 0));
}

std::string al::CompileTime::nextConstVarName() {
  stringstream ss;
  ss << strCounter++;
  string name = ".local_" + ss.str();
  return name;
}

void al::CompileTime::createPutsFunc() {
  vector<llvm::Type*> types(1, getValuePtrType());
  FunctionType *ft = FunctionType::get(getValuePtrType(), types, false);
  Function *func = Function::Create(ft, Function::ExternalLinkage, "puts", getMainModule());
  func->args().begin()->setName("s");

  BasicBlock *BB = BasicBlock::Create(theContext, "entry", func);
  IRBuilder<> builder1(theContext);
  builder1.SetInsertPoint(BB);

  // function body
  llvm::Value *_objPtr = func->arg_begin();
  auto objPtr = builder1.CreatePointerCast(_objPtr, getValuePtrType());
  auto sPtr = builder1.CreateGEP(
      objPtr->getType()->getPointerElementType(),
      objPtr,
      {ConstantInt::get(Type::getInt32Ty(theContext),0),
       ConstantInt::get(Type::getInt32Ty(theContext),1)}
  );
  auto bytesDataPtr = builder1.CreateGEP(
      sPtr->getType()->getPointerElementType(),
      sPtr,
      {ConstantInt::get(Type::getInt32Ty(theContext),0),
       ConstantInt::get(Type::getInt32Ty(theContext),1)}
  );
  auto bytesData = builder1.CreateLoad(bytesDataPtr);
  builder1.CreateCall(
      this->fns.printf,
      {bytesData}
  );
  builder1.CreateRet(ConstantPointerNull::get(getValuePtrType()));

  verifyFunction(*func);
}

void al::CompileTime::createLibFunc() {
  // int printf(const char *, ...)
  this->fns.printf = Function::Create(
      FunctionType::get(
          Type::getInt32Ty(theContext),
          {PointerType::get(Type::getInt8Ty(theContext), 0)},
          true
      ),
      Function::LinkageTypes::ExternalLinkage, "printf", getMainModule()
  );
}

llvm::Value *al::CompileTime::castStringToValuePtr(llvm::Value *strVal) {
  auto valObj = builder.CreateAlloca(
      getValueType(),
      0,
      ConstantInt::get(Type::getInt32Ty(theContext), 1)
  );
  // val.type = ValueType::String
  builder.CreateStore(
      ConstantInt::get(Type::getInt32Ty(theContext), al::ValueType::String),
      builder.CreateGEP(
          getValueType(),
          valObj,
          {ConstantInt::get(Type::getInt32Ty(theContext), 0),
           ConstantInt::get(Type::getInt32Ty(theContext), 0)}
      )
  );
  // val.sVal = *sPtr
  builder.CreateMemCpy(
      builder.CreateGEP(
          getValueType(),
          valObj,
          {ConstantInt::get(Type::getInt32Ty(theContext), 0),
           ConstantInt::get(Type::getInt32Ty(theContext), 1)}
      ),
      strVal,
      sizeof(al::StringValue),
      1
  );
  return valObj;
}

llvm::Value *al::CompileTime::createNullValuePtr() {
  auto valObj = builder.CreateAlloca(
      getValueType(),
      0,
      ConstantInt::get(Type::getInt32Ty(theContext), 1)
  );
  // val.type = ValueType::Null
  builder.CreateStore(
      ConstantInt::get(Type::getInt32Ty(theContext), al::ValueType::Null),
      builder.CreateGEP(
          getValueType(),
          valObj,
          {ConstantInt::get(Type::getInt32Ty(theContext), 0),
           ConstantInt::get(Type::getInt32Ty(theContext), 0)}
      )
  );
  return valObj;
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
    if (objType.llvmType->isIntegerTy(32)) {
      auto fn = getMainModule()->getOrInsertFunction(
          "setIntNvmVar",
          FunctionType::get(
              Type::getVoidTy(theContext), {Type::getInt32Ty(theContext), Type::getInt32Ty(theContext)}, false
          )
      );
      getCompilerContext().builder->CreateCall(fn, {ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)varId), value});
    }
    else if (objType.llvmType->isStructTy()) {
      cerr << "unimplemented for struct type" << endl;
      abort();
    }
    else {
      cerr << "unsupported type" << endl;
      objType.llvmType->dump();
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
      {"void", llvm::Type::getVoidTy(theContext)},
  };
  for (auto &t1 : types) {
    ObjType ty;
    ty.name = t1.first;
    ty.llvmType = t1.second;
    this->registerType(ty.name, ty);
  }
}

llvm::Value *al::CompileTime::createGetMemNvmVar(const std::string &name) {
  if (this->persistentSymbolTable.find(name) == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto t = this->typeTable.find(this->persistentSymbolTable.find(name)->second)->second.llvmType;
  auto sizeVal = this->getCompilerContext().builder->CreatePtrToInt(
      this->getCompilerContext().builder->CreateGEP(
          t,
          ConstantPointerNull::get(PointerType::get(t, 0)),
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
  return getCompilerContext().builder->CreateCall(
      fn,
      {
          ConstantInt::get(Type::getInt32Ty(theContext), (uint64_t)varId),
          sizeVal,
      }
  );
}

void al::CompileTime::createSetMemNvmVar(const std::string &name, llvm::Value *ptr) {
  if (this->persistentSymbolTable.find(name) == this->persistentSymbolTable.end()) {
    cerr << "Persistent var not found " << name << endl;
    abort();
  }

  int varId = name[name.size() - 1] - '0';
  auto t = this->typeTable.find(this->persistentSymbolTable.find(name)->second)->second.llvmType;
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

llvm::Value *al::CompileTime::getStructSize(IRBuilder<> &builder, llvm::Type *s) {
  if (!s->isStructTy()) {
    cerr << "not a struct" << endl;
    abort();
  }
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

//llvm::Value *al::CompileTime::castToValuePtr(llvm::Value *val) {
//  return BitCastInst::CreatePointerCast(val, getValuePtrType());
//}
