#include "ast.h"
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include <llvm/IR/Verifier.h>
#include "compile_time.h"
#include "llvm/ADT/STLExtras.h"

using namespace std;
using namespace llvm;

namespace al {
  namespace ast {
    int ASTNode::indent = 0;

    void StringLiteral::preVisit(CompileTime &) {
      std::cout << std::string((uint32_t)this->indent, '\t')
                << "str<'" << this->s << "'>"
                << std::endl;
    }

    VisitResult StringLiteral::genVisitResult(CompileTime &ct) {
      VisitResult vr;
//        vr.value = createStringTypeObject(this->s);
      vr.value = ct.castStringToValuePtr(ct.createStringValuePtr(getValue(), ct.getBuilder()));
      return vr;
    }

    void ExpList::preVisit(CompileTime &) {
      std::cout << std::string((uint32_t)this->indent, '\t')
                << "exp_list" << std::endl;
      this->incIndent();
    }

    void ExpList::postVisit(CompileTime &) {
      this->decIndent();
    }

//    VisitResult ExpList::visit(CompileTime &rt) {
//      preVisit(rt);
//      auto c = getChildren();
//      if (c.empty()) {
//        // TODO: should return llvm nullptr
//        return nullptr;
//      }
//      auto symbol = dynamic_cast<Symbol*>(c[0].get());
//      if (symbol == nullptr) {
//        // not function call list, maybe value array, string array
//        vector<llvm::Value*> items;
//        for (const auto &item : c) {
//          auto str = dynamic_cast<StringLiteral*>(item.get());
//          if (str != nullptr) {
//            items.push_back(rt.castStringToValuePtr(
//                rt.castStringToValuePtr(rt.createStringValuePtr(str->getValue(), rt.getBuilder()))));
//          }
//        }
//        VisitResult vr;
//        vr.value = llvm::ConstantPointerNull::get(rt.getValuePtrType());
////        vr.value = llvm::Array::get(arr, items);
//        return vr;
////        return llvm::ConstantArray::get(arr, {});
////        cerr << "Compile error, the first element in function call list must be a symbol" << endl;
////        abort();
//      }
//      bool isNative = false;
//      auto fnName = symbol->getValue();
//      llvm::Function *callee = nullptr;
//
//      if (fnName == "wtf") {
//        vector<llvm::Type*> types(3, llvm::Type::getInt64Ty(rt.getContext()));
//        auto ft = llvm::FunctionType::get(
//            llvm::Type::getVoidTy(rt.getContext()),
//            types,
//            true
//        );
//        callee = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "AL__callFunction", rt.getMainModule());
//
//        auto strValuePtr = rt.createStringValuePtr(fnName, rt.getBuilder());
//        auto nameStrValue = rt.castStringToValuePtr(strValuePtr);
//
//        std::vector<llvm::Value *> ArgsV;
//        // void AL__callFunction(uint64_t _prt, uint64_t _pname, uint64_t nargs, ...);
//        ArgsV.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(rt.getContext()), (uint64_t)&rt));
//        ArgsV.push_back(rt.getBuilder().CreatePtrToInt(nameStrValue, llvm::Type::getInt64Ty(rt.getContext())));
//        ArgsV.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(rt.getContext()), (uint64_t)(c.size() - 1)));
//
//        for (unsigned i = 1; i < c.size(); ++i) {
//          auto result = c[i]->visit(rt);
//          if (result.value) {
//            ArgsV.push_back(rt.getBuilder().CreatePointerCast(result.value, rt.getValuePtrType()));
//          }
//          else {
//            ArgsV.push_back(rt.getBuilder().CreatePointerCast(
//                rt.castStringToValuePtr(rt.createStringValuePtr("wtf", rt.getBuilder())), rt.getValuePtrType())
//            );
//          }
//        }
//
//        postVisit(rt);
//
//        VisitResult vr;
//        for (int i = 1; i < ArgsV.size(); ++i)
//          ArgsV[i] = rt.getBuilder().CreatePtrToInt(ArgsV[i], llvm::Type::getInt64Ty(rt.getContext()));
//
//        vr.value = rt.getBuilder().CreateCall(callee, ArgsV);
//        return vr;
//      }
//      else {
//        callee = rt.getMainModule()->getFunction(fnName);
//        if (callee == nullptr) {
//          auto nativeFnName = "AL__" + fnName;
//          uint32_t argc = c.size() - 1;
//          vector<llvm::Type*> ps;
//          ps.push_back(llvm::Type::getInt64Ty(rt.getContext()));
//          for (int i = 0; i < argc; ++i) {
//            ps.push_back(llvm::Type::getInt64Ty(rt.getContext()));
//          }
//          auto ft = llvm::FunctionType::get(llvm::Type::getVoidTy(rt.getContext()), ps, false);
//          callee = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, nativeFnName, rt.getMainModule());
////        callee = rt.getMainModule()->getFunction();
//          isNative = true;
//        }
//      }
//      if (!callee) {
//        cerr << "Compile error, function '" << fnName << "' not defined" << endl;
//        abort();
//      }
//
//
//      std::vector<llvm::Value *> ArgsV;
//      for (unsigned i = 1; i < c.size(); ++i) {
//        auto result = c[i]->visit(rt);
//        if (result.value) {
//          ArgsV.push_back(rt.getBuilder().CreatePointerCast(result.value, rt.getValuePtrType()));
//        }
//        else {
//          ArgsV.push_back(rt.getBuilder().CreatePointerCast(
//              rt.createStringValuePtr("wtf", rt.getBuilder()),
//              rt.getValuePtrType()));
//        }
//      }
//
//      postVisit(rt);
//      VisitResult vr;
//      if (isNative) {
//        ArgsV.insert(ArgsV.begin(), llvm::ConstantInt::get(llvm::Type::getInt64Ty(rt.getContext()), (uint64_t)&rt));
//        for (int i = 1; i < ArgsV.size(); ++i)
//          ArgsV[i] = rt.getBuilder().CreatePtrToInt(ArgsV[i], llvm::Type::getInt64Ty(rt.getContext()));
//      }
//
//
//      vr.value = rt.getBuilder().CreateCall(callee, ArgsV);
//      return vr;
//    }

    void Symbol::preVisit(CompileTime &) {
      std::cout << std::string((uint32_t)this->indent, '\t')
                << "sym<'" << this->s << "'>"
                << std::endl;
    }

    Type::Type(std::shared_ptr<Symbol> symbol) :symbol(symbol) {
    }

    std::string Type::getName() const { return symbol->getName(); }

    ExpCall::ExpCall(const std::shared_ptr<Symbol> &name, std::vector<std::shared_ptr<Exp>> exps)
        :ExpCall(name->getName(), exps) {}

    VisitResult ExpCall::visit(CompileTime &ct) {
      std::vector<llvm::Value*> args;
      auto exps = this->getChildren();
      for (const auto &_exp : exps) {
        auto exp = dynamic_pointer_cast<Exp>(_exp);
        auto vr = exp->visit(ct);
        args.push_back(vr.value);
      }

      Function *fn = nullptr;
      if (this->name == "=") {
        if (args.size() != 2) { cerr << "'=' only accepts 2 args" << endl; abort(); }
        auto _varName = dynamic_pointer_cast<ExpVarRef>(exps[0]);
        if (_varName == nullptr) {
          cerr << "'=' operator accepts a symbol as the first arg" << endl;
        } else {
          auto varName = _varName->getName();

          if (ct.hasPersistentVar(varName)) {
            ct.createSetPersistentVar(varName, args[1]);
          }
          VisitResult vr;
          vr.value = args[1];
          return vr;
        }
      }
      else if (this->name == "+") {
        if (args.size() != 2) { cerr << "'+' only accepts 2 args" << endl; abort(); }
        vr.value = ct.getCompilerContext().builder->CreateBinOp(
            Instruction::BinaryOps::Add,
            args[0],
            args[1]
        );
        return vr;
      }
      else {
        fn = ct.getMainModule()->getFunction(this->name);
      }

      if (fn == nullptr) {
        cerr << "Function not found '" << this->name << "'" << endl;
        abort();
      }
      VisitResult vr;
      vr.value = ct.getCompilerContext().builder->CreateCall(fn, args);
      return vr;
    }

    void FnDef::preVisit(CompileTime &rt) {
      auto fn = rt.getMainModule()->getFunction(this->getLinkageName());
      if (fn == nullptr) {
        fn = Function::Create(
            FunctionType::get(llvm::Type::getVoidTy(rt.getContext()), {}),
            Function::ExternalLinkage,
            this->getLinkageName(),
            rt.getMainModule()
        );
      }
      CompilerContext cc(rt.getContext(), fn, BasicBlock::Create(rt.getContext(), "entry", fn));
//      cc.builder.CreateCall(rt.getHowAreYouFn(), {});

      rt.pushContext(cc);
    }


    std::string FnDef::getName() const {
      return symbol->getName();
    }

    std::string FnDef::getLinkageName() const {
      return "AL__" + symbol->getName();
    }

    void FnDef::postVisit(CompileTime &ct) {
      ct.getCompilerContext().builder->CreateRet(nullptr);
//      if (!verifyFunction(*ct.getCompilerContext().function)) {
//        cout << "failed to verifyFunction " << ct.getCompilerContext().function->getName().str() << endl;
//      }
      ct.popContext();
    }

    VisitResult ExpVarRef::visit(CompileTime &ct) {
      if (ct.hasPersistentVar(this->name->getName())) {
        vr.value = ct.createGetPersistentVar(this->name->getName());
      }
      else {
        vr.value = ct.getMainModule()->getGlobalVariable(this->name->getName());
        if (vr.value == nullptr) {
          cerr << "either global var nor persistent var found named '" << this->name->getName() << "'" << endl;
          abort();
        }
      }
      return vr;
    }

    std::string ExpVarRef::getName() const { return name->getName(); }

    VisitResult IntLiteral::visit(CompileTime &ct) {
      stringstream ss;
      int i;
      ss << this->s;
      ss >> i;
      vr.value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), i);
      return vr;
    }

    VisitResult PersistentBlock::visit(CompileTime &ct) {
      for (const auto &_varDecl : getChildren()[0]->getChildren()) {
        auto varDecl = std::dynamic_pointer_cast<VarDecl>(_varDecl);
        ct.registerSetPersistentVar(varDecl->getName());
      }
      return vr;
    }

    VarDecl::VarDecl(const std::shared_ptr<Symbol> &symbol, const std::shared_ptr<Type> &type) {
      appendChild(type);
      appendChild(symbol);
    }

    std::string VarDecl::getName() { return  std::dynamic_pointer_cast<Symbol>(getChildren()[1])->getName();  }

    sp<Type> VarDecl::getType() { return std::dynamic_pointer_cast<Type>(getChildren()[0]); }

//    VisitResult Stmt::visit(CompileTime &ct) {
//    }
  }
}

