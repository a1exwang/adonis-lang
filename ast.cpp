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


    void Symbol::preVisit(CompileTime &) {
//      std::cout << std::string((uint32_t)this->indent, '\t')
//                << "sym<'" << this->s << "'>"
//                << std::endl;
    }

    Type::Type(std::shared_ptr<Symbol> symbol) :symbol(symbol) {
    }

    std::string Type::getName() const { return symbol->getName(); }

    bool Type::isVoid() { return symbol->getName() == "void"; }

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
      /**
       * Assignment Operator is a special operator
       */
      if (this->name == "=") {
        auto rhs = dynamic_pointer_cast<Exp>(exps[1]);
        if (args.size() != 2) { cerr << "'=' only accepts 2 args" << endl; abort(); }
        auto ptr = dynamic_pointer_cast<ExpVarRef>(exps[0]);
        if (ptr == nullptr) {
          auto lhs = dynamic_pointer_cast<ExpMemberAccess>(exps[0]);
          if (lhs == nullptr) {
            cerr << "'=' operator accepts a symbol as the first arg" << endl;
            abort();
          }
          auto elementType =
              static_cast<llvm::PointerType*>(lhs->getVR().gepResult->getType())->getElementType();

          // TODO support more types
          if (elementType->isIntegerTy(32)) {
            ct.getCompilerContext().builder->CreateStore(
                rhs->getVR().value,
                lhs->getVR().gepResult
            );
          }
          else if (elementType->isStructTy()) {
            auto lhsPtr = lhs->getVR().gepResult;
            auto rhsPtr = rhs->getVR().gepResult;
            ct.getCompilerContext().builder->CreateMemCpy(
                lhsPtr,
                rhsPtr,
                CompileTime::getStructSize(
                    *ct.getCompilerContext().builder,
                    static_cast<PointerType*>(rhsPtr->getType())->getElementType()),
                8
            );
            vr.gepResult = rhsPtr;
          }
          else {
            cerr << "type not supported" << endl;
            elementType->dump();
            abort();
          }
          vr.value = rhs->getVR().value;

          // commit
          ct.createSetMemNvmVar(lhs->getObjName(), lhs->getVR().gepResult);
          return vr;
        } else {
          auto varName = ptr->getName();

          if (ct.hasPersistentVar(varName)) {
            if (ct.getPersistentVarType(varName) == "int32") {
              ct.createSetPersistentVar(varName, args[1]);
            }
            else {
              auto lhsPtr = ct.createGetMemNvmVar(varName);
              auto lhsType = ct.getType(ct.getPersistentVarType(varName));
              auto rhsPtr = rhs->getVR().gepResult;
              ct.getCompilerContext().builder->CreateMemCpy(
                  lhsPtr,
                  rhsPtr,
                  CompileTime::getStructSize(
                      *ct.getCompilerContext().builder,
                      lhsType.llvmType
                  ),
                  8
              );
              vr.gepResult = rhsPtr;
            }
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
      return decl->getName();
    }

    std::string FnDef::getLinkageName() const {
      return decl->getName();
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
        auto type = ct.getPersistentVarType(this->name->getName());
        if (ct.getType(type).llvmType->isStructTy()) {
          vr.gepResult = ct.createGetMemNvmVar(this->name->getName());
        }
        else if (ct.getType(type).name == "int32") {
          vr.value = ct.createGetIntNvmVar(this->name->getName());
        }
        else {
          cerr << "persistent var type not supported '" << this->name->getName() << "'" << endl;
        }
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
        ct.registerPersistentVar(varDecl->getName(), varDecl->getType()->getName());
      }
      return vr;
    }

    VarDecl::VarDecl(const std::shared_ptr<Symbol> &symbol, const std::shared_ptr<Type> &type) {
      appendChild(type);
      appendChild(symbol);
    }

    std::string VarDecl::getName() { return  std::dynamic_pointer_cast<Symbol>(getChildren()[1])->getName();  }

    sp<Type> VarDecl::getType() { return std::dynamic_pointer_cast<Type>(getChildren()[0]); }

    VisitResult ExpMemberAccess::visit(CompileTime &ct) {
      auto &builder = *ct.getCompilerContext().builder;
      if (ct.hasPersistentVar(obj->getName())) {
        auto nvmPtr = ct.createGetMemNvmVar(obj->getName());

        auto structObjType = ct.getType(ct.getPersistentVarType(obj->getName()));
        uint64_t idx = structObjType.elementNames.size();
        for (uint64_t i = 0; i < structObjType.elementNames.size(); ++i) {
          if (structObjType.elementNames[i] == member->getName()) {
            idx = i;
            break;
          }
        }
        if (idx >= structObjType.elementNames.size()) {
          cerr << "object member does not exist '" << member->getName() << "'" << endl;
          abort();
        }
        auto structType = structObjType.llvmType;
        auto structPtr = builder.CreatePointerCast(
            nvmPtr, llvm::PointerType::get(structType, 0)
        );
        auto elementPtr = ct.getCompilerContext().builder->CreateGEP(
            structType,
            structPtr,
            {ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), 0),
             ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), idx)}
        );
        // TODO Add more types
        if (structObjType.llvmType->getStructElementType(idx)->isIntegerTy(32)) {
          vr.gepResult = builder.CreateBitCast(
              elementPtr,
              llvm::Type::getInt32PtrTy(ct.getContext())
          );
          vr.value = builder.CreateLoad(vr.gepResult);
        }
        else {
          cerr << "memberAccess element type not supported " << structObjType.elementNames[idx] << endl;
        }
        return vr;
      }
      else {
        cerr << "persistent var not found" << obj->getName();
        abort();
      }

    }

    std::string ExpMemberAccess::getObjName() { return this->obj->getName(); }

    void StructBlock::postVisit(CompileTime &ct) {
      ObjType objType;
      objType.name = this->name->getName();
      vector<llvm::Type*> elements;

      for (const auto &_varDecl : this->varDecls->getChildren()) {
        auto varDecl = dynamic_pointer_cast<VarDecl>(_varDecl);
        auto elementObjType = ct.getType(varDecl->getType()->getName());
        elements.push_back(elementObjType.llvmType);
        objType.elementNames.push_back(varDecl->getName());
      }

      objType.llvmType = llvm::StructType::create(ct.getContext(), elements, objType.name);
      ct.registerType(objType.name, objType);
    }

    FnDecl::FnDecl(sp<Symbol> name, sp<Type> ret, sp<VarDecls> args) :name(move(name)), ret(ret), args(move(args)) { }

    std::string FnDecl::getName() const { return name->getName(); }

    std::vector<llvm::Type *> FnDecl::getArgTypes(CompileTime &ct) const {
      std::vector<llvm::Type*> ts;
      for (auto arg : args->getChildren()) {
        auto tName = dynamic_cast<VarDecl*>(arg.get())->getType()->getName();
        ts.push_back(ct.getType(tName).llvmType);
      }
      return ts;
    }

    std::vector<string> FnDecl::getArgNames(CompileTime &ct) const {
      std::vector<string> ts;
      for (auto arg : args->getChildren()) {
        auto name = dynamic_cast<VarDecl*>(arg.get())->getName();
        ts.push_back(name);
      }
      return ts;
    }

    Type FnDecl::getRetType() const { return *ret; }

    void ExternBlock::postVisit(CompileTime &ct) {
      for (const auto &child : getChildren()[0]->getChildren()) {
        auto fnDecl = dynamic_pointer_cast<FnDecl>(child);
        if (fnDecl != nullptr) {
          auto fn = ct.getMainModule()->getFunction(fnDecl->getName());
          if (fn != nullptr) {
            cerr << "function already exists" << endl;
            abort();
          }
          auto retType = ct.getType(fnDecl->getRetType().getName());
          vector<llvm::Type*> args;
          for (auto arg : fnDecl->getArgTypes(ct)) {
            args.push_back(arg);
          }
          fn = Function::Create(
              FunctionType::get(retType.llvmType, args, false),
              Function::ExternalLinkage,
              fnDecl->getName(),
              ct.getMainModule()
          );
          auto argNames = fnDecl->getArgNames(ct);
          int i = 0;
          for (auto &a : fn->args()) {
            a.setName(argNames[i++]);
          }
        }
        else {
          cerr << "extern only supports function type" << endl;
          abort();
        }
      }
    }
  }
}

