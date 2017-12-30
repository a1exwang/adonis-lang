#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>


namespace al {
  class CompileTime;
  namespace ast {
    template <typename T> using sp = std::shared_ptr<T>;

    struct VisitResult {
      VisitResult() :value() { }
      VisitResult(const std::nullptr_t &nptr) :value(nullptr) {}
      llvm::Value *value;
    };

    // Visitor design pattern
    class ASTNode {
    public:
      ASTNode() :vr() { }
      virtual void pre_visit(CompileTime &rt) { }
      virtual void post_visit(CompileTime &rt) { }
      virtual VisitResult gen_visit_result(CompileTime &ct) { return vr; }
      virtual VisitResult visit(CompileTime &rt) {
        pre_visit(rt);
        for (auto &child : getChildren()) {
          child->visit(rt);
        }
        post_visit(rt);
        return gen_visit_result(rt);
      }
      virtual std::vector<std::shared_ptr<ASTNode>> getChildren() {
        return children;
      }
      VisitResult getVR() const { return vr; }

    protected:
      void appendChild(const std::shared_ptr<ASTNode> &node) {
        this->children.push_back(node);
      }
      void prependChild(const std::shared_ptr<ASTNode> &node) {
        this->children.insert(this->children.begin(), node);
      }

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
    public:
      std::shared_ptr<Blocks> append(const std::shared_ptr<Block> p) {
        sp<Blocks> ret = std::make_shared<Blocks>();
        *ret = *this;
        ret->blocks.push_back(p);
        return ret;
      }
    private:
      std::vector<sp<Block>> blocks;
    };

    class Symbol;
    class Type;
//    class StructBlock :public Block {
//    public:
//      StructBlock(std::shared_ptr<Symbol> sym,
//                  std::vector<std::shared_ptr<Symbol>, std::shared_ptr<Type>> items) {
//
//      }
//    };
    class VarDecl {
    public:
      VarDecl(std::shared_ptr<Symbol> symbol, std::shared_ptr<Type> type) {

      }
    private:

    };
    class VarDecls :public ASTNode {
    public:
      std::shared_ptr<VarDecls> append(std::shared_ptr<VarDecl> child);

    private:
      std::vector<sp<VarDecl>> decls;
    };
    class PersistentBlock :public Block {
    public:
      explicit PersistentBlock(std::shared_ptr<VarDecls> varDecls) :varDecls(varDecls) {
      }

    private:
      std::shared_ptr<VarDecls> varDecls;
    };
    class Type :public ASTNode {
    public:
      Type(std::shared_ptr<Symbol> symbol);
    private:
      sp<Symbol> symbol;
    };

    class Stmt :public ASTNode {
    };
    class Stmts :public ASTNode {
    public:
      std::shared_ptr<Stmts> append(std::shared_ptr<Stmt> child) {
        sp<Stmts> ret = std::make_shared<Stmts>();
        *ret = *this;
        ret->stmts.push_back(child);
        return ret;
      }
    private:
      std::vector<sp<Stmt>> stmts;
    };
    class StmtBlock :public ASTNode {
    public:
      explicit StmtBlock(std::shared_ptr<Stmts> stmts) :stmts(std::move(stmts)) {
      }

    private:
      sp<Stmts> stmts;
    };

    class FnDef :public Block {
    public:
      FnDef(std::shared_ptr<Symbol> symbol, sp<StmtBlock> stmtBlock)
        :symbol(std::move(symbol)), stmtBlock(stmtBlock) {}
    private:
      sp<Symbol> symbol;
      sp<StmtBlock> stmtBlock;
    };

    class Exp :public Stmt {
    };

    class ExpCall :public Exp {
    public:
      ExpCall(const std::string &name, std::vector<std::shared_ptr<Exp>> exps) :name(name), exps(exps){}
      ExpCall(std::shared_ptr<Symbol> name, std::vector<std::shared_ptr<Exp>> exps);
    private:
      std::string name;
      std::vector<sp<Exp>> exps;
    };
    class ExpVarRef :public Exp {
    public:
      ExpVarRef(sp<Symbol> name) :name(name) {}
    private:
      sp<Symbol> name;
    };

    class ExpList :public ASTNode {
    public:
      std::shared_ptr<ExpList> append(std::shared_ptr<Exp> node) {
        auto ret = std::make_shared<ExpList>(*this);
        ret->appendChild(std::move(node));
        return ret;
      }
      std::shared_ptr<ExpList> prepend(std::shared_ptr<Exp> node) {
        auto ret = std::make_shared<ExpList>(*this);
        ret->prependChild(std::move(node));
        return ret;
      }
      VisitResult visit(CompileTime &) override;

      void pre_visit(CompileTime &) override;
      void post_visit(CompileTime &) override;
      std::vector<std::shared_ptr<Exp>> toVector() {
        std::vector<sp<Exp>> exps;
        for (auto &exp : this->getChildren()) {
          auto ptr = std::dynamic_pointer_cast<Exp>(exp);
          exps.push_back(ptr);
        }
        return exps;
      }
    };

    class List :public Exp {
    public:
      explicit List(const std::shared_ptr<ExpList> &p) : list(p) {
        this->appendChild(p);
      }
      void pre_visit(CompileTime &) override;
      void post_visit(CompileTime &) override;
      VisitResult gen_visit_result(CompileTime &) override {
        return getChildren()[0]->getVR();
      }

    private:
      std::shared_ptr<ExpList> list;
    };

    class Symbol :public Exp {
    public:
      explicit Symbol(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }

      void pre_visit(CompileTime &) override;

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

      void pre_visit(CompileTime &) override;
      VisitResult gen_visit_result(CompileTime &ct) override;

    private:
      std::string s;
    };
    class IntLiteral :public Literal {
    public:
      explicit IntLiteral(std::string s): s(std::move(s)) { }
      std::string getValue() const {
        return this->s;
      }
    private:
      std::string s;
    };
  }
}