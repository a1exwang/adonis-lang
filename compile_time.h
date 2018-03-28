#pragma once
#include <memory>
#include <utility>
#include <llvm/IR/LLVMContext.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include <map>
#include "ast.h"


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
    class Type;
    class Annotation;
  }

//  struct ObjType {
//    std::string name;
////    bool persistent;
//    llvm::Type *llvmType;
//    std::vector<std::string> elementNames;
//  };

  enum PtrAddressSpace {
    Volatile = 0,
    NVM = 1
  };

  struct CompilerContext {
  public:
    CompilerContext(llvm::LLVMContext &c, llvm::Function *function, llvm::BasicBlock *bb, std::shared_ptr<ast::Annotation> annotation = nullptr)
        :basicBlock(bb), function(function), builder(new llvm::IRBuilder<>(c)), annotation(annotation)
    { builder->SetInsertPoint(bb); }
    CompilerContext(const CompilerContext &cc)
        :basicBlock(cc.basicBlock), builder(cc.builder), function(cc.function), annotation(cc.annotation) {}
    llvm::BasicBlock *basicBlock;
    llvm::Function *function;
    std::shared_ptr<llvm::IRBuilder<>> builder;
    std::shared_ptr<ast::Annotation> annotation;
//    std::map<std::string, llvm::Value*> stackVariables;
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
    void setupMainModule();
    void createMainFunc();
    void traverseAll();

    template<typename T>
    void traverse(T &t);

    void registerBuiltinTypes();
    llvm::Module* getMainModule() const;

    std::unique_ptr<llvm::Module> &&moveMainModule() { return std::move(mainModule); }

    llvm::Function *getHowAreYouFn() const { return this->howAreYou; }

    void pushContext(CompilerContext cc) {
      this->compilerContextStack.push_back(std::move(cc));
    }
    CompilerContext &getCompilerContext() { return *(compilerContextStack.end()-1); }
    void popContext() { compilerContextStack.pop_back(); }
    void pushCurrentBlock(llvm::BasicBlock *b) { this->currentBlocks.push_back(b); }
    void popCurrentBlock() { this->currentBlocks.pop_back(); }
    llvm::BasicBlock *getCurrentBlock() const { return *(this->currentBlocks.end() - 1); }
    llvm::Function *getMainFunc() const { return mainFunction; }

    llvm::LLVMContext &getContext() { return theContext; }

    llvm::Value *createGetIntNvmVar(const std::string &name);
    llvm::Value *createGetMemNvmVar(const std::string &name);
    llvm::Value *createGetMemNvmVar(llvm::PointerType *nvmPtrType, int id);
    void createSetMemNvmVar(const std::string &name, llvm::Value *ptr);
    void createSetPersistentVar(const std::string &name, llvm::Value*);
    void createCommitPersistentVarIfOk(llvm::Value *nvmPtr, llvm::Value *size, llvm::Value *ok);
    void registerType(const std::string &name, std::shared_ptr<al::ast::Type> type);
    bool hasType(const std::string &name) const;
    std::shared_ptr<ast::Type> getType(const std::string &name);

    void registerSymbol(const std::string &name, std::shared_ptr<al::ast::Type> type);
    bool hasSymbol(const std::string &name) const {
      return this->globalSymbolTable.find(name) != this->globalSymbolTable.end();
    }
    std::shared_ptr<const ast::Type> getSymbolType(const std::string &name) {
      return this->globalSymbolTable[name];
    }
    void createAssignment(
        llvm::Type *elementType,
        llvm::Value *lhsPtr,
        llvm::Value *rhsVal,
        llvm::Value *rhsPtr,
        llvm::Value *persistNvm = nullptr
    );
    llvm::Value* getFunctionStackVariable(const std::string &functionName, const std::string &varName);
    bool hasFunctionStackVariable(const std::string &functionName, const std::string &varName);
    void setFunctionStackVariable(const std::string &functionName, const std::string &varName, llvm::Value *val);
    void unsetFunctionStackVariable(const std::string &functionName, const std::string &varName);
    PersistentVarTaggingPass &getPvarTagPass() { return *this->pvarTag; }

  public:
    static llvm::Value *getTypeSize(llvm::IRBuilder<> &builder, llvm::Type *s);
  private:

    llvm::Function *mainFunction;
    llvm::Function *howAreYou;
    std::shared_ptr<ast::ASTNode> root;

    llvm::LLVMContext theContext;
    std::unique_ptr<llvm::Module> mainModule;

    std::vector<llvm::BasicBlock*> currentBlocks;
    int strCounter;

    std::vector<CompilerContext> compilerContextStack;
    std::map<std::string, std::shared_ptr<ast::Type>> typeTable;
    std::map<std::string, std::shared_ptr<ast::Type>> globalSymbolTable;

    std::map<std::string, std::map<std::string, llvm::Value*>> functionStackVariables;
    std::unique_ptr<PersistentVarTaggingPass> pvarTag;
  };

  template<typename T>
  void CompileTime::traverse(T &t) {
    this->root->traverse(*this, t);
  }
}