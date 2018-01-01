#include "ast.h"
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include <utility>
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

    Type::Type(std::shared_ptr<Symbol> symbol, int attrs) :symbol(std::move(symbol)), attrs(attrs) {
    }

    std::string Type::getName() const {
      if (attrs == Type::None) {
        return symbol->getName();
      }
      else if (attrs & Type::Ptr) {
        return "*" + originalType->getName();
      }
      else {
        cerr << "Wrong type attr" << endl;
        abort();
      }
    }

    bool Type::isVoid() { return symbol->getName() == "void"; }

    std::string Type::getOriginalTypeName() const { return symbol->getName(); }

    ExpCall::ExpCall(const std::shared_ptr<Symbol> &name, std::vector<std::shared_ptr<Exp>> exps)
        :ExpCall(name->getName(), exps) {}

    void ExpCall::postVisit(CompileTime &ct) {
      std::vector<llvm::Value*> args;
      auto exps = this->getChildren();
      for (const auto &_exp : exps) {
        auto exp = dynamic_pointer_cast<Exp>(_exp);
        args.push_back(exp->getVR().value);
      }

      Function *fn = nullptr;
      /**
       * Assignment Operator is a special operator
       */
      if (this->name == "+") {
        if (args.size() != 2) { cerr << "'+' only accepts 2 args" << endl; abort(); }
        vr.value = ct.getCompilerContext().builder->CreateBinOp(
            Instruction::BinaryOps::Add,
            args[0],
            args[1]
        );
      }
      else {
        fn = ct.getMainModule()->getFunction(this->name);
        if (fn == nullptr) {
          cerr << "Function not found '" << this->name << "'" << endl;
          abort();
        }
        vr.value = ct.getCompilerContext().builder->CreateCall(fn, args);
      }
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
        else if (ct.getType(type).llvmType->isIntegerTy(32)) {
          vr.value = ct.createGetIntNvmVar(this->name->getName());
          vr.gepResult = ct.createGetMemNvmVar(this->name->getName());
        }
        else if (ct.getType(type).llvmType->isPointerTy()) {
          auto ptr = ct.createGetMemNvmVar(this->name->getName());
          vr.value = ct.getCompilerContext().builder->CreateLoad(ptr);
          vr.gepResult = ptr;
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

    void ExpMemberAccess::postVisit(CompileTime &ct) {
      auto &builder = *ct.getCompilerContext().builder;
      auto nvmPtr = obj->getVR().gepResult;
      if (nvmPtr == nullptr) {
        cerr << "lhs is not a valid struct obj" << endl;
        abort();
      } else {
        auto structType = nvmPtr->getType()->getPointerElementType();
        // TODO
        ObjType structObjType = ct.getType(structType->getStructName());
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
          abort();
        }
      }
    }

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

    void ExpAssign::postVisit(CompileTime &ct) {
      auto exps = getChildren();
      if (exps.size() != 2) { cerr << "'=' only accepts 2 args" << endl; abort(); }

      auto rhs = dynamic_pointer_cast<Exp>(exps[1]);
      auto rhsVal = rhs->getVR().value;
      auto rhsPtr = rhs->getVR().gepResult;

      auto lhsPtr = exps[0]->getVR().gepResult;
      auto elementType = lhsPtr->getType()->getPointerElementType();

      if (elementType->isIntegerTy(32)) {
        ct.getCompilerContext().builder->CreateStore(
            rhsVal,
            lhsPtr
        );
        vr = rhs->getVR();
      }
      else if (elementType->isPointerTy()) {
        ct.getCompilerContext().builder->CreateStore(
            rhsVal,
            lhsPtr
        );
        vr = rhs->getVR();
      }
      else if (elementType->isStructTy()) {
        ct.getCompilerContext().builder->CreateMemCpy(
            lhsPtr,
            rhsPtr,
            CompileTime::getTypeSize(
                *ct.getCompilerContext().builder,
                rhsPtr->getType()->getPointerElementType()
            ),
            8
        );
        vr.gepResult = rhsPtr;
        vr.value = nullptr;
      }
      else {
        cerr << "type not supported" << endl;
        elementType->dump();
        abort();
      }

      // TODO support more types and do this only on nvm
//      if (lhsPtr->getType()->getPointerAddressSpace() == PtrAddressSpace::NVM) {
        ct.createCommitPersistentVar(
            lhsPtr,
            ct.getTypeSize(*ct.getCompilerContext().builder, elementType)
        );
//      }
    }

    void ExpGetAddr::postVisit(CompileTime &ct) {
      vr.value = getChildren()[0]->getVR().gepResult;
      vr.gepResult = nullptr;
    }

    void ExpDeref::postVisit(CompileTime &ct) {
      auto ptr = getChildren()[0]->getVR().value;
      if (ptr->getType()->isPointerTy()) {
        vr.gepResult = ptr;
        vr.value = ct.getCompilerContext().builder->CreateLoad(ptr);
      }
      else {
        cerr << "not a pointer " << ptr->getName().str() << endl;
        ptr->dump();
        abort();
      }
    }
  }
}

