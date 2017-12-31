#pragma once
#include <memory>
#include <utility>
#include <llvm/IR/LLVMContext.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include <map>


namespace al {
  struct Value;
  struct StringValue {
    uint32_t len;
    uint8_t *data;
  };
  struct ArrayValue {
    uint32_t n;
    Value *pVal;
  };
  enum ValueType {
    Null = 0,
    Integer = 0,
    String,
    Array
  };
  struct Value {
    uint32_t type;

    StringValue sVal;
    uint32_t iVal;
    ArrayValue aVal;
  };
  namespace ast {
    class ASTNode;
  }

  struct ObjType {
    std::string name;
//    bool persistent;
    llvm::Type *llvmType;
    std::vector<std::string> elementNames;
  };

  struct CompilerContext {
  public:
    CompilerContext(llvm::LLVMContext &c, llvm::Function *function, llvm::BasicBlock *bb)
        :basicBlock(bb), function(function), builder(new llvm::IRBuilder<>(c)) { builder->SetInsertPoint(bb); }
    CompilerContext(const CompilerContext &cc)
        :basicBlock(cc.basicBlock), builder(cc.builder), function(cc.function) {}
    llvm::BasicBlock *basicBlock;
    llvm::Function *function;
    std::shared_ptr<llvm::IRBuilder<>> builder;
  };
  class CompileTime {
  public:
    CompileTime();
    ~CompileTime();

    void setASTRoot(std::shared_ptr<ast::ASTNode> root) {
      this->root = std::move(root);
    }
    void init1();
    void finish1();
    void createFunction(llvm::Module &module, const std::string &name, std::vector<std::string> paramNames);
    void setupMainModule();
    void createFnFunc();
    void createMainFunc();
    void createLibFunc();
    void createPrimitiveTypes();
    void createPlaceHolderFunc(const std::string &name, int n);
    void createPutsFunc();
    void traverse1();
    void registerBuiltinTypes();
    llvm::BasicBlock* createFunctionBody(llvm::Module &module, const std::string &name);
    llvm::Module* getMainModule() const;

    llvm::Value *createNullValuePtr();
    llvm::Value *createStringValuePtr(const std::string &s, llvm::IRBuilder<> &builder);
    llvm::Value *castStringToValuePtr(llvm::Value *);

    llvm::PointerType *getStringPtrType() const;
    llvm::StructType *getStringType() const;
    llvm::StructType *getValueType() const { return valueType; }
    llvm::PointerType *getValuePtrType();
    llvm::StructType *getArrayType() const { return arrayType; }
    std::unique_ptr<llvm::Module> &&moveMainModule() { return std::move(mainModule); }

    llvm::Function *getHowAreYouFn() const { return this->howAreYou; }

    void pushContext(CompilerContext cc) {
      this->compilerContextStack.push_back(std::move(cc));
    }
    CompilerContext getCompilerContext() const { return *(compilerContextStack.end()-1); }
    void popContext() { compilerContextStack.pop_back(); }
    void pushCurrentBlock(llvm::BasicBlock *b) { this->currentBlocks.push_back(b); }
    void popCurrentBlock() { this->currentBlocks.pop_back(); }
    llvm::BasicBlock *getCurrentBlock() const { return *(this->currentBlocks.end() - 1); }
    llvm::Function *getMainFunc() const { return mainFunction; }

    llvm::IRBuilder<> &getBuilder() { return builder; }

    llvm::LLVMContext &getContext() { return theContext; }

    llvm::Value *createGetIntNvmVar(const std::string &name);
    llvm::Value *createGetMemNvmVar(const std::string &name);
    void createSetMemNvmVar(const std::string &name, llvm::Value *ptr);
    bool hasPersistentVar(const std::string &name) const {
      return this->persistentSymbolTable.find(name) != persistentSymbolTable.end();
    }
    void createSetPersistentVar(const std::string &name, llvm::Value*);
    void registerPersistentVar(const std::string &name, const std::string &type);
    std::string getPersistentVarType(const std::string &name) { return this->persistentSymbolTable[name]; }

    void registerType(const std::string &name, ObjType type) {
      this->typeTable[name] = type;
    }
    bool hasType(const std::string &name) const {
      return this->typeTable.find(name) != this->typeTable.end();
    }
    ObjType getType(const std::string &name) {
      return this->typeTable[name];
    }

//  private:
  public:
    std::string nextConstVarName();

    struct {
      llvm::Function *printf;
    } fns;
    llvm::Function *mainFunction;
    llvm::Function *howAreYou;

    llvm::StructType *valueType;
    llvm::PointerType *valuePtrType;
    llvm::StructType *stringType;
    llvm::StructType *arrayType;

    std::shared_ptr<ast::ASTNode> root;

    llvm::LLVMContext theContext;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> mainModule;

    std::vector<llvm::BasicBlock*> currentBlocks;
    int strCounter;

    std::map<std::string, llvm::Module*> symbolTable;
    std::vector<CompilerContext> compilerContextStack;
    std::map<std::string, std::string> persistentSymbolTable;
    std::map<std::string, ObjType> typeTable;
  };
}