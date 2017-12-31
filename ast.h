#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <iostream>


namespace al {
  class CompileTime;
  namespace ast {
    template <typename T> using sp = std::shared_ptr<T>;

    struct VisitResult {
      VisitResult() :value() { }
      VisitResult(const std::nullptr_t &nptr) :value(nullptr) {}
      llvm::Value *value;
      llvm::Value *gepResult;
    };

    // Visitor design pattern
    class ASTNode {
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
      virtual std::vector<std::shared_ptr<ASTNode>> getChildren() {
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
    class VarDecl :public ASTNode{
    public:
      VarDecl(const std::shared_ptr<Symbol> &symbol, const std::shared_ptr<Type> &type);
      std::string getName();
      sp<Type> getType();
    };
    class VarDecls :public ASTNode {
    };
    class PersistentBlock :public Block {
    public:
      explicit PersistentBlock(const std::shared_ptr<VarDecls> &varDecls) {
        appendChild(varDecls);
      }
      VisitResult visit(CompileTime &ct) override;
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
    class Type :public ASTNode {
    public:
      Type(std::shared_ptr<Symbol> symbol);
      std::string getName() const;
    private:
      sp<Symbol> symbol;
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
      FnDef(std::shared_ptr<Symbol> symbol, sp<StmtBlock> stmtBlock)
        :symbol(std::move(symbol)) {
        appendChild(stmtBlock);
      }

      void preVisit(CompileTime &rt) override;
      void postVisit(CompileTime &) override;
      std::string getName() const;
      std::string getLinkageName() const;
    private:
      sp<Symbol> symbol;
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
      VisitResult visit(CompileTime &ct) override;

    private:
      std::string name;
    };
    class ExpVarRef :public Exp {
    public:
      explicit ExpVarRef(sp<Symbol> name) :name(std::move(name)) {}
      VisitResult visit(CompileTime &ct) override;
      std::string getName() const;
    private:
      sp<Symbol> name;
    };

    class ExpMemberAccess :public Exp {
    public:
      ExpMemberAccess(sp<Symbol> obj, sp<Symbol> member) :obj(std::move(obj)), member(std::move(member)) {}
      VisitResult visit(CompileTime &ct) override;
      std::string getObjName();
    private:
      sp<Symbol> obj;
      sp<Symbol> member;
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
      VisitResult genVisitResult(CompileTime &ct) override;

    private:
      std::string s;
    };
    class IntLiteral :public Literal {
    public:
      explicit IntLiteral(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }
      VisitResult visit(CompileTime &ct) override;
    private:
      std::string s;
    };
  }
}