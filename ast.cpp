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
//      std::cout << std::string((uint32_t)this->indent, '\t')
//                << "sym<'" << this->s << "'>"
//                << std::endl;
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
  }
}

