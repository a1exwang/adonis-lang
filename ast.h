#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include "passes/pv_tagging.h"


namespace al {
  class CompileTime;
  namespace ast {
    template <typename T> using sp = std::shared_ptr<T>;

    struct VisitResult {
      VisitResult() :value() { }
      VisitResult(const std::nullptr_t &nptr) :value(nullptr) {}
      llvm::Value *value = nullptr;
      llvm::Value *gepResult = nullptr;
    };

    template <typename ContextType, typename Myself>
    class Traversable {
    public:
      virtual void preTraverse(CompileTime &rt, ContextType &context) { }
      virtual void postTraverse(CompileTime &rt, ContextType &context) { }
      virtual void traverse(CompileTime &rt, ContextType &context) {
        this->preTraverse(rt, context);
        for (auto &child : getChildren()) {
          child->traverse(rt, context);
        }
        this->postTraverse(rt, context);
      }
      virtual std::vector<std::shared_ptr<Myself>> &getChildren() = 0;
    };


    // Visitor design pattern
    class ASTNode :public Traversable<PersistentVarTaggingPass, ASTNode> {
    public:
      ASTNode() :vr() { }
      virtual void preVisit(CompileTime &rt) { }
      virtual void postVisit(CompileTime &rt) { }
      virtual VisitResult genVisitResult(CompileTime &ct) { return vr; }
      virtual VisitResult visit(CompileTime &rt) {
        preVisit(rt);
        for (auto &child : getChildren()) {
          vr = child->visit(rt);
        }
        postVisit(rt);
        return genVisitResult(rt);
      }
      virtual std::vector<std::shared_ptr<ASTNode>> &getChildren() {
        return children;
      }


      void appendChild(const std::shared_ptr<ASTNode> &node) {
        this->children.push_back(node);
      }
      void prependChild(const std::shared_ptr<ASTNode> &node) {
        this->children.insert(this->children.begin(), node);
      }
      VisitResult getVR() const { return vr; }
    protected:
      VisitResult vr;

      static int indent;
      static void incIndent() { indent++; }
      static void decIndent() {
        indent--;
        if (indent < 0)
          indent = 0;
      }
    private:
      std::vector<std::shared_ptr<ASTNode>> children;
    };

    class Block :public ASTNode {
    };
    class Blocks :public ASTNode {
    };

    class Symbol;
    class Type;
    class Annotation;
    class Exp;
    class Decl :public ASTNode {
    };
    class Decls :public ASTNode { };
    class VarDecl :public Decl{
    public:
      VarDecl(const std::shared_ptr<Symbol> &symbol, const std::shared_ptr<Type> &type);
      std::string getName();
      sp<Type> getType();
      llvm::Type *getLlvmType();
      void markPersistent();
    };
    class VarDecls :public ASTNode {
    };
    class FnDecl :public Decl {
    public:
      explicit FnDecl(
          sp<Symbol> name,
          sp<Type> ret = std::make_shared<Type>(),
          sp<VarDecls> args = std::make_shared<VarDecls>());
      std::string getName() const;
      Type getRetType();
      std::vector<llvm::Type*> getArgTypes(CompileTime &ct) const;
      std::vector<std::string> getArgNames(CompileTime &ct) const;
    private:
      sp<Symbol> name;
      sp<Type> ret;
      sp<VarDecls> args;
    };
    class PersistentBlock :public Block {
    public:
      explicit PersistentBlock(const std::shared_ptr<VarDecls> &varDecls) {
        appendChild(varDecls);
      }
      void postVisit(CompileTime &ct) override;
    };
    class StructBlock :public Block {
    public:
      StructBlock(sp<Symbol> name, const sp<VarDecls> &varDecls)
          :name(std::move(name)), varDecls(varDecls) {
        appendChild(varDecls);
      }
      void postVisit(CompileTime &ct) override;
    private:
      sp<Symbol> name;
      sp<VarDecls> varDecls;
    };
    class ExternBlock :public Block {
    public:
      ExternBlock(sp<Decls> decls) { appendChild(decls); }
      void postVisit(CompileTime &ct) override;
    };
    class Type :public ASTNode {
    public:
      enum { None = 1, Ptr = 2, Persistent = 4, Fn = 8, Array = 16 };

      /**
       * Create a symbol ref pre-defined type
       * @param symbol
       * @param attrs
       * @param llvmType
       */
      explicit Type(
          std::shared_ptr<Symbol> symbol = std::make_shared<Symbol>("void"),
          int attrs = None,
          llvm::Type *llvmType = nullptr,
          std::shared_ptr<Exp> arraySizeVal = nullptr
      );

      /**
       * Create a pointer or persistent type
       * @param originalType
       * @param attrs
       */
      explicit Type(
          const sp<Type> &originalType,
          int attrs = None,
          std::shared_ptr<Exp> arraySizeVal = nullptr
      );

      /**
       * Create a function type
       * @param args
       * @param retType
       * @param attrs
       */
      explicit Type(
          sp<al::ast::VarDecls> args,
          sp<al::ast::Type> retType = std::make_shared<ast::Type>(std::make_shared<ast::Symbol>("void")),
          int attrs = Fn
      );

      /**
       * Create a struct type
       * @param symbol
       * @param llvmType
       * @param memberNames
       */
      explicit Type(
          sp<Symbol> symbol,
          llvm::Type *llvmType,
          std::vector<std::string> memberNames
      );
      /**
       * For *int32 returns int32
       */
      std::string getOriginalTypeName() const;
      bool isVoid();
      void markPersistent();
      bool isPersistent() const { return bPersistent; }
      bool same(const Type &rhs) const;
      llvm::Type *getLlvmType() const;
      sp<VarDecls> getArgs() { return fnTypeArgs; }
      std::vector<std::string> getMembers() { return memberNames; }
      std::string toString() const;

      static llvm::PointerType *getArrayPtrType(llvm::Type *elementType);
      static llvm::Value *sizeOfArray(llvm::IRBuilder<> &builder, llvm::PointerType *arrayPtrType, llvm::Value *len);
      static void arrayCopy(llvm::IRBuilder<> &builder, llvm::Value *dst, llvm::Value *src);
      static llvm::Value *getArrayElementPtr(llvm::IRBuilder<> &builder, llvm::Value *arrStruct, llvm::Value *index);
      static llvm::Value *createArrayByAlloca(llvm::IRBuilder<> &builder, llvm::PointerType *arrPtrType, llvm::Value *len);

      void parseLlvmType(CompileTime &ct);
      llvm::Value *getArraySizeVal() const;

      void postVisit(CompileTime &ct) override;
      int getAttrs() const { return attrs; }
    private:
      sp<Symbol> symbol;
      sp<Type> originalType;
      int attrs;
      bool bPersistent = false;
      sp<VarDecls> fnTypeArgs;
      llvm::Type *llvmType{};
      std::vector<std::string> memberNames;
      sp<Exp> arraySizeVal;
    };

    class Stmt :public ASTNode {
//      VisitResult visit(CompileTime &ct) override;
    };
    class Stmts :public ASTNode {
    };
    class StmtBlock :public ASTNode {
    public:
      explicit StmtBlock(const std::shared_ptr<Stmts> &stmts) {
        appendChild(stmts);
      }
    };

    class FnDef :public Block {
    public:
      FnDef(sp<FnDecl> decl, sp<StmtBlock> stmtBlock) :decl(std::move(decl)) {
        appendChild(stmtBlock);
      }

      VisitResult visit(CompileTime &rt) override;
      std::string getName() const;
      std::string getLinkageName() const;

      void postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) override;
      void preTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) override;
    private:
      sp<FnDecl> decl;
    };

    class Exp :public Stmt {
    };

    class ExpCall :public Exp {
    public:
      ExpCall(std::string name, std::vector<std::shared_ptr<Exp>> exps) :name(std::move(name)) {
        for (const auto &exp : exps) {
          appendChild(exp);
        }
      }
      ExpCall(const std::shared_ptr<Symbol> &name, std::vector<std::shared_ptr<Exp>> exps);
      void postVisit(CompileTime &ct) override;

    private:
      std::string name;
    };
    class ExpVarRef :public Exp {
    public:
      enum VarRefType {
        Invalid = 0,
        StackVolatile = 1,
        FunctionPersistent = 2,
        GlobalPersistent = 3,
        Function = 4,
        External = 5
      };
      explicit ExpVarRef(sp<Symbol> name) :name(std::move(name)), varRefType(Invalid) {}
      void postVisit(CompileTime &ct) override;
      std::string getName() const;
      void postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) override;
      VarRefType getVarRefType() const { return varRefType; }
    private:
      sp<Symbol> name;
      VarRefType varRefType;
    };

    class ExpAssign :public Exp {
    public:
      ExpAssign(const sp<Exp> &lhs, const sp<Exp> &rhs) {
        appendChild(lhs); appendChild(rhs);
      }
      void postVisit(CompileTime &ct) override;
      void traverse(CompileTime &ct, PersistentVarTaggingPass &pass) override;
    };
    class ExpMove :public Exp {
    public:
      ExpMove(const sp<ExpVarRef> &lhs, const sp<ExpVarRef> &rhs) :lhs(lhs), rhs(rhs) {
        appendChild(lhs);
        appendChild(rhs);
      }
      void postVisit(CompileTime &ct) override;
    private:
      sp<ExpVarRef> lhs;
      sp<ExpVarRef> rhs;
    };

    class ExpStackVarDef :public Exp {
    public:
      ExpStackVarDef(sp<VarDecl> decl, sp<Exp> exp) :decl(decl), exp(exp) { appendChild(decl); appendChild(exp); }
      void postVisit(CompileTime &ct) override;
      void postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) override;
    private:
      sp<VarDecl> decl;
      sp<Exp> exp;
    };
    class ExpMemberAccess :public Exp {
    public:
      ExpMemberAccess(sp<Exp> obj, sp<Symbol> member) :obj(obj), member(std::move(member)) {
        appendChild(obj);
      }
      void postVisit(CompileTime &ct) override;
    private:
      sp<Exp> obj;
      sp<Symbol> member;
    };
    class ExpGetAddr :public Exp {
    public:
      explicit ExpGetAddr(sp<Exp> exp) { appendChild(exp); }
      void postVisit(CompileTime &ct) override;
    };
    class ExpDeref :public Exp {
    public:
      explicit ExpDeref(sp<Exp> exp) { appendChild(exp); }
      void postVisit(CompileTime &ct) override;
    };
    class ExpVolatileCast :public Exp {
    public:
      explicit ExpVolatileCast(sp<Exp> exp) { appendChild(exp); }
      void postVisit(CompileTime &ct) override;
    };

    class ExpList :public ASTNode {
    public:
//      VisitResult visit(CompileTime &) override;

      void preVisit(CompileTime &) override;
      void postVisit(CompileTime &) override;
      std::vector<std::shared_ptr<Exp>> toVector() {
        std::vector<sp<Exp>> exps;
        for (auto &exp : this->getChildren()) {
          auto ptr = std::dynamic_pointer_cast<Exp>(exp);
          exps.push_back(ptr);
        }
        return exps;
      }
    };
    class ExpFor :public Exp {
    public:
      ExpFor(sp<Exp> initExp, sp<Exp> judgementExp, sp<Exp> tailExp, sp<StmtBlock> body, sp<Annotation> annotation = nullptr);

      VisitResult visit(CompileTime &ct) override;
    private:
      sp<Exp> initExp;
      sp<Exp> judgementExp;
      sp<Exp> tailExp;
      sp<StmtBlock> body;
      sp<Annotation> annotation;
    };

    class Symbol :public Exp {
    public:
      explicit Symbol(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }

      void preVisit(CompileTime &) override;

      std::string getName() const {
        return this->s;
      }

    private:
      std::string s;
    };
    class Literal :public Exp {
    };
    class StringLiteral :public Literal {
    public:
      explicit StringLiteral(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }

      void preVisit(CompileTime &) override;

    private:
      std::string s;
    };
    class IntLiteral :public Literal {
    public:
      explicit IntLiteral(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }
      void postVisit(CompileTime &ct) override;
    private:
      std::string s;
    };
    class Annotation :public ASTNode {
    public:
      explicit Annotation(sp<Symbol> name, sp<ExpList> exps) :name(name) { appendChild(exps); }
      std::string getName() const {
        return this->name->getName();
      }
      bool isBatchFor(const std::string &nvmVarName) {
        auto args = this->getChildren()[0]->getChildren();
        if (this->getName() != "batch")
          return false;

        auto varNameArg = dynamic_cast<ExpVarRef*>(args[0].get());
        return varNameArg && varNameArg->getName() == nvmVarName;
      }
      void setOnBatchSizeVal(llvm::Value *val) {
        this->onBatchSizeVal = val;
      }
      llvm::Value *getOnBatchSizeVal() {
        return this->onBatchSizeVal;
      }
      int getBatchCount();
    private:
      sp<Symbol> name;
      llvm::Value *onBatchSizeVal;
    };
  }
}