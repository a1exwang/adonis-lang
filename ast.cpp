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

    bool Type::isVoid() { return symbol->getName() == "void"; }

    std::string Type::getOriginalTypeName() const { return symbol->getName(); }

    void Type::markPersistent() {
      bPersistent = true;
    }

    llvm::Type *Type::getLlvmType() const {
      return this->llvmType;
    }

    void Type::postVisit(CompileTime &ct) {
      parseLlvmType(ct);
    }

    bool Type::same(const Type &rhs) const {
      if (this->attrs == Type::Fn) {
        if (!this->originalType->same(*rhs.originalType)) {
          return false;
        }
        for (int i = 0; i < this->fnTypeArgs->getChildren().size(); ++i) {
          auto larg = dynamic_cast<VarDecl*>(this->fnTypeArgs->getChildren()[i].get());
          auto rarg = dynamic_cast<VarDecl*>(rhs.fnTypeArgs->getChildren()[i].get());
          if (!larg->getType()->same(*rarg->getType())) {
            return false;
          }
        }
        return true;
      } else if (this->attrs == Type::None) {
        return this->symbol->getName() == rhs.symbol->getName();
      } else if (this->attrs & Type::Ptr || this->attrs & Type::Persistent){
        return (this->attrs == rhs.attrs) && (this->originalType->same(*rhs.originalType));
      } else {
        cerr << "unreachable code" << endl;
        abort();
      }
    }

    std::string Type::toString() const {
      if ((this->attrs & Type::Ptr) && (this->attrs & Type::Persistent)) {
        cerr << "Type::attrs has Ptr and Persistent set" << endl;
        abort();
      }

      if (this->attrs & Type::None) {
        return this->symbol->getName();
      } else if (this->attrs & Type::Ptr) {
        return "*" + this->originalType->toString();
      } else if (this->attrs & Type::Persistent) {
        return "persistent " + this->originalType->toString();
      } else if (this->attrs & Type::Fn) {
        auto c = this->fnTypeArgs->getChildren();
        string ret = "fn (";
        for (int i = 0; i < c.size(); ++i) {
          auto arg = dynamic_cast<VarDecl*>(c[i].get());
          ret += arg->getType()->toString();
          if (i != c.size() - 1) {
            ret += ", ";
          }
        }
        return ret + ") " + this->originalType->toString();
      } else {
        cerr << "Type::attrs has unknown values" << endl;
        abort();
      }
    }

    Type::Type(sp<al::ast::VarDecls> args,
               sp<al::ast::Type> retType,
               int attrs)
        :fnTypeArgs(args), attrs(attrs), originalType(retType) {
      if (fnTypeArgs== nullptr) {
        std::cerr << "fnTypeArgs== nullptr" << std::endl;
        abort();
      }
      appendChild(fnTypeArgs);
      appendChild(retType);
    }

    Type::Type(sp<Symbol> symbol, llvm::Type *llvmType, std::vector<std::string> memberNames)
        :symbol(std::move(symbol)), llvmType(llvmType), memberNames(std::move(memberNames)) {
    }

    void Type::parseLlvmType(CompileTime &ct) {
      if (this->llvmType == nullptr) {
        if (this->symbol) {
          this->llvmType = ct.getType(this->symbol->getName())->getLlvmType();
        } else if (this->attrs & Ptr) {
          this->originalType->parseLlvmType(ct);
          this->llvmType = llvm::PointerType::get(this->originalType->getLlvmType(), PtrAddressSpace::Volatile);
        } else if (this->attrs & Persistent) {
          this->originalType->parseLlvmType(ct);
          if (this->originalType->attrs & Ptr) {
            this->llvmType = llvm::PointerType::get(
                ((llvm::PointerType*)this->originalType->getLlvmType())->getPointerElementType(),
                PtrAddressSpace::NVM
            );
          } else {
            this->bPersistent = true;
            this->llvmType = this->originalType->getLlvmType();
          }
        } else if (this->attrs & Fn) {
          std::vector<llvm::Type*> myArgs;
          for (const auto &_arg : getArgs()->getChildren()) {
            auto arg = dynamic_cast<VarDecl*>(_arg.get());
            auto t = arg->getType();
            t->parseLlvmType(ct);
            myArgs.push_back(t->getLlvmType());
          }

          auto fnType = llvm::FunctionType::get(
              // TODO: support for non void return types
              llvm::Type::getVoidTy(ct.getContext()),
              myArgs,
              false
          );
          this->llvmType = llvm::PointerType::get(fnType, Volatile);
        }
      }
    }

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
      if (this->name == "+") {
        if (args.size() != 2) { cerr << "'+' only accepts 2 args" << endl; abort(); }
        vr.value = ct.getCompilerContext().builder->CreateBinOp(
            Instruction::BinaryOps::Add,
            args[0],
            args[1]
        );
      }
      else if (this->name == "<") {
        if (args.size() != 2) { cerr << "'<' only accepts 2 args" << endl; abort(); }
        vr.value = ct.getCompilerContext().builder->CreateZExt(
            ct.getCompilerContext().builder->CreateICmp(llvm::CmpInst::Predicate::ICMP_SLT,
                                                        args[0],
                                                        args[1]),
            llvm::IntegerType::getInt32Ty(ct.getContext())
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

    std::string FnDef::getName() const {
      return decl->getName();
    }

    std::string FnDef::getLinkageName() const {
      return decl->getName();
    }

    VisitResult FnDef::visit(CompileTime &ct) {
      this->decl->visit(ct);
      auto retType = this->decl->getRetType();
      auto argTypes = this->decl->getArgTypes(ct);
      auto argNames = this->decl->getArgNames(ct);

      auto fn = ct.getMainModule()->getFunction(this->getLinkageName());
      if (fn == nullptr) {
        fn = Function::Create(
            FunctionType::get(retType.getLlvmType(), argTypes, false),
            Function::ExternalLinkage,
            this->getLinkageName(),
            ct.getMainModule()
        );
      }

      CompilerContext cc(ct.getContext(), fn, BasicBlock::Create(ct.getContext(), "entry", fn));
      ct.pushContext(cc);


      int i = 0;
      for (auto &arg : fn->args()) {
        auto varNewLocation = ct.getCompilerContext().builder->CreateAlloca(argTypes[i]);
        ct.getCompilerContext().builder->CreateStore(&arg, varNewLocation);
        ct.setFunctionStackVariable(fn->getName(), argNames[i], varNewLocation);
        i++;
      }

      for (auto child : this->getChildren()) {
        child->visit(ct);
      }

      ct.getCompilerContext().builder->CreateRet(nullptr);
//      if (!verifyFunction(*ct.getCompilerContext().function)) {
//        cout << "failed to verifyFunction " << ct.getCompilerContext().function->getName().str() << endl;
//      }
      ct.popContext();

      return this->vr;
    }

    VisitResult ExpVarRef::visit(CompileTime &ct) {
      auto &cont = ct.getCompilerContext();
      auto funcVarName = string(ct.getCompilerContext().function->getName()) + "::" + this->name->getName();


      bool hasGlobalVar = ct.hasSymbol(this->name->getName());
      bool hasFunctionVar = ct.hasSymbol(funcVarName);

      if (ct.hasFunctionStackVariable(ct.getCompilerContext().function->getName(), this->name->getName())) {
        // stack variables
        auto var = ct.getFunctionStackVariable(ct.getCompilerContext().function->getName(), this->name->getName());
        vr.value = cont.builder->CreateLoad(var);
        vr.gepResult = var;
      }
      else if (hasGlobalVar || hasFunctionVar) {
        // function/global persistent/volatile variables
        string varName;
        if (hasGlobalVar) {
          varName = this->name->getName();
        } else {
          varName = funcVarName;
        }

        auto type = ct.getSymbolType(varName);
        if (type->getLlvmType()->isStructTy() ||
            type->getLlvmType()->isIntegerTy(32) ||
            type->getLlvmType()->isPointerTy()) {
          // FIXME: support global volatile variables
          vr.gepResult = ct.createGetMemNvmVar(varName);
          vr.value = ct.getCompilerContext().builder->CreateLoad(vr.gepResult);
        }
        else {
          cerr << "persistent var type not supported '" << varName << "'" << endl;
        }
      }
      else {
        // global adonis-lang functions
        vr.value = ct.getMainModule()->getFunction(this->name->getName());
        if (vr.value == nullptr) {
          // external global symbol(functions/variables)
          vr.value = ct.getMainModule()->getGlobalVariable(this->name->getName());
          if (vr.value == nullptr) {
            cerr << "no stack var, function persistent var, global var, function or global persistent var found named '" << this->name->getName() << "'" << endl;
            abort();
          }
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

    void PersistentBlock::postVisit(CompileTime &ct) {
      for (const auto &_varDecl : getChildren()[0]->getChildren()) {
        auto varDecl = std::dynamic_pointer_cast<VarDecl>(_varDecl);
        varDecl->markPersistent();
        ct.registerSymbol(varDecl->getName(), varDecl->getType());
      }
    }

    VarDecl::VarDecl(const std::shared_ptr<Symbol> &symbol, const std::shared_ptr<Type> &type) {
      appendChild(type);
      appendChild(symbol);
    }

    std::string VarDecl::getName() { return  std::dynamic_pointer_cast<Symbol>(getChildren()[1])->getName();  }

    sp<Type> VarDecl::getType() { return std::dynamic_pointer_cast<Type>(getChildren()[0]); }

    void VarDecl::markPersistent() {
      this->getType()->markPersistent();
    }

    llvm::Type *VarDecl::getLlvmType() {
      return nullptr;
    }

    void ExpMemberAccess::postVisit(CompileTime &ct) {
      auto &builder = *ct.getCompilerContext().builder;
      auto lhsPtr = obj->getVR().gepResult;
      if (lhsPtr == nullptr) {
        cerr << "lhs is not a valid struct obj" << endl;
        abort();
      } else {
        auto structType = lhsPtr->getType()->getPointerElementType();
        // TODO
        auto structAstType = ct.getType(structType->getStructName());
        uint64_t idx = structAstType->getMembers().size();
        for (uint64_t i = 0; i < structAstType->getMembers().size(); ++i) {
          if (structAstType->getMembers()[i] == member->getName()) {
            idx = i;
            break;
          }
        }
        if (idx >= structAstType->getMembers().size()) {
          cerr << "object member does not exist '" << member->getName() << "'" << endl;
          abort();
        }
        auto structPtr = builder.CreatePointerCast(
            lhsPtr, llvm::PointerType::get(
                structType,
                static_cast<llvm::PointerType*>(lhsPtr->getType())->getAddressSpace())
        );
        auto elementPtr = ct.getCompilerContext().builder->CreateGEP(
            structType,
            structPtr,
            {ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), 0),
             ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), idx)}
        );
        // TODO Add more types
        if (structAstType->getLlvmType()->getStructElementType(idx)->isIntegerTy(32)) {
          vr.gepResult = builder.CreateBitCast(
              elementPtr,
              llvm::Type::getInt32PtrTy(
                  ct.getContext(),
                  static_cast<llvm::PointerType*>(elementPtr->getType())->getAddressSpace()
              )
          );
          vr.value = builder.CreateLoad(vr.gepResult);
        }
        else {
          cerr << "memberAccess element type not supported " << structAstType->getMembers()[idx] << endl;
          abort();
        }
      }
    }

    void StructBlock::postVisit(CompileTime &ct) {
      auto name = this->name->getName();

      vector<llvm::Type*> elements;

      vector<std::string> elementNames;
      for (const auto &_varDecl : this->varDecls->getChildren()) {
        auto varDecl = dynamic_pointer_cast<VarDecl>(_varDecl);
        auto elementObjType = varDecl->getType()->getLlvmType();
        elements.push_back(elementObjType);
        elementNames.push_back(varDecl->getName());
      }

      auto llvmType = llvm::StructType::create(ct.getContext(), elements, name);
      auto type = std::make_shared<ast::Type>(
          std::make_shared<Symbol>(name.c_str()),
          llvmType,
          elementNames
      );
      ct.registerType(name, type);
    }

    FnDecl::FnDecl(sp<Symbol> name, sp<Type> ret, sp<VarDecls> args) :name(move(name)), ret(ret), args(move(args)) {
      appendChild(this->ret);
      appendChild(this->args);
    }

    std::string FnDecl::getName() const { return name->getName(); }

    std::vector<llvm::Type *> FnDecl::getArgTypes(CompileTime &ct) const {
      std::vector<llvm::Type*> ts;
      for (auto &arg : args->getChildren()) {
        auto decl = dynamic_cast<VarDecl*>(arg.get());
        ts.push_back(decl->getType()->getLlvmType());
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

    Type FnDecl::getRetType() { return *ret; }

    void ExternBlock::postVisit(CompileTime &ct) {
      for (const auto &child : getChildren()[0]->getChildren()) {
        auto fnDecl = dynamic_pointer_cast<FnDecl>(child);
        if (fnDecl != nullptr) {
          auto fn = ct.getMainModule()->getFunction(fnDecl->getName());
          if (fn != nullptr) {
            cerr << "function already exists" << endl;
            abort();
          }
          auto llvmTypeRet = fnDecl->getRetType().getLlvmType();
          vector<llvm::Type*> args;
          for (auto arg : fnDecl->getArgTypes(ct)) {
            args.push_back(arg);
          }
          fn = Function::Create(
              FunctionType::get(llvmTypeRet, args, false),
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

      // If lhs is a symbol, we support a batch operation
      llvm::Value *onBatchSizeVal = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 1);
      if (ct.getCompilerContext().annotation) {
        onBatchSizeVal = ct.getCompilerContext().annotation->getOnBatchSizeVal();
      }

      ct.createAssignment(
          elementType,
          lhsPtr,
          rhsVal,
          rhsPtr,
          onBatchSizeVal
      );

      vr = rhs->getVR();
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

    void ExpStackVarDef::postVisit(CompileTime &ct) {
      auto vr = this->exp->getVR();
      auto &cc = ct.getCompilerContext();
      auto type = this->decl->getType();
      auto llvmType = type->getLlvmType();
      llvm::Value *var;
      if (type->getAttrs() & ast::Type::Persistent) {
        auto varName = string(ct.getCompilerContext().function->getName()) + "::" + this->decl->getName();
        ct.registerSymbol(
            varName,
            type
        );
        var = ct.createGetMemNvmVar(varName);
      } else {
        var = cc.builder->CreateAlloca(llvmType);
        ct.setFunctionStackVariable(
            ct.getCompilerContext().function->getName(),
            this->decl->getName(),
            var
        );
      }
      ct.createAssignment(
          llvmType,
          var,
          vr.value,
          vr.gepResult
      );
    }

    void ExpVolatileCast::postVisit(CompileTime &ct) {
      auto exp = getChildren()[0];
      auto val = exp->getVR().value;
      if (!val->getType()->isPointerTy()) {
        cerr << "Cannot use volatile cast operator on non-pointer type" << endl;
        abort();
      }

      auto valPtrType = static_cast<llvm::PointerType*>(val->getType());
      auto valVolatilePtrType = PointerType::get(valPtrType->getElementType(), PtrAddressSpace::Volatile);
      auto gepResult = exp->getVR().gepResult;
      auto builder = ct.getCompilerContext().builder;

      if (!gepResult) {
        vr.value = builder->CreatePointerCast(val, valVolatilePtrType);
      }
      else {
        vr.value = builder->CreatePointerCast(val, valVolatilePtrType);
        vr.gepResult = builder->CreatePointerCast(
            gepResult,
            PointerType::get(
              valVolatilePtrType,
              valPtrType->getAddressSpace()
            )
        );
      }
    }

    VisitResult ExpFor::visit(CompileTime &ct) {
      auto outerAnnotation = ct.getCompilerContext().annotation;
      if (this->annotation && this->annotation->getName() == "batch") {

        auto batchCount = this->annotation->getBatchCount();
        // init expression
        this->initExp->visit(ct);
        auto counterVal = ct.getCompilerContext().builder->CreateAlloca(
            llvm::IntegerType::getInt32Ty(ct.getContext())
        );
        ct.getCompilerContext().builder->CreateStore(
            llvm::ConstantInt::get(llvm::IntegerType::get(ct.getContext(), 32), 0),
            counterVal
        );

        auto function = ct.getCompilerContext().function;

        auto judgementBlock = BasicBlock::Create(ct.getContext(), "", function);

        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext judgementCt(ct.getContext(), function, judgementBlock, this->annotation);
        ct.popContext();
        ct.pushContext(judgementCt);

        auto bodyBlock = BasicBlock::Create(ct.getContext(), "", function);
        auto nextBlock = BasicBlock::Create(ct.getContext(), "", function);

        auto judgeResult = this->judgementExp->visit(ct);
        auto isFalse = ct.getCompilerContext().builder->CreateICmp(
            llvm::CmpInst::Predicate::ICMP_EQ,
            judgeResult.value,
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0, true)
        );
        ct.getCompilerContext().builder->CreateCondBr(
            isFalse,
            nextBlock,
            bodyBlock
        );

        CompilerContext bodyCt(ct.getContext(), function, bodyBlock, this->annotation);
        ct.popContext();
        ct.pushContext(bodyCt);

        // Only persist when i % batchSize == 0
        auto builder = ct.getCompilerContext().builder;

        // counter++
        builder->CreateStore(
            builder->CreateAdd(
                builder->CreateLoad(counterVal),
                llvm::ConstantInt::get(llvm::IntegerType::get(ct.getContext(), 32), 1)
            ),
            counterVal
        );
        // onBatchSize = counter % batchSize == 0
        auto onBatchSizeVal = builder->CreateICmp(
            llvm::CmpInst::Predicate::ICMP_EQ,
            builder->CreateSRem(
                builder->CreateLoad(counterVal),
                llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), batchCount)
            ),
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0)
        );
        // TODO: Persist the NVM variable after the loop
        // Before that, we need to maintain the NVM variable list in the loop
        annotation->setOnBatchSizeVal(builder->CreateZExt(onBatchSizeVal, llvm::IntegerType::getInt32Ty(ct.getContext())));
        // Persist ends

        this->body->visit(ct);
        this->tailExp->visit(ct);

        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext nextCt(ct.getContext(), function, nextBlock, this->annotation);
        ct.popContext();
        ct.pushContext(nextCt);

        return this->vr;

      } else {
        // init expression
        this->initExp->visit(ct);

        auto function = ct.getCompilerContext().function;

        auto judgementBlock = BasicBlock::Create(ct.getContext(), "", function);

        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext judgementCt(ct.getContext(), function, judgementBlock, outerAnnotation);
        ct.popContext();
        ct.pushContext(judgementCt);

        auto bodyBlock = BasicBlock::Create(ct.getContext(), "", function);
        auto nextBlock = BasicBlock::Create(ct.getContext(), "", function);

        auto judgeResult = this->judgementExp->visit(ct);
        auto isFalse = ct.getCompilerContext().builder->CreateICmp(
            llvm::CmpInst::Predicate::ICMP_EQ,
            judgeResult.value,
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0, true)
        );
        ct.getCompilerContext().builder->CreateCondBr(
            isFalse,
            nextBlock,
            bodyBlock
        );

        CompilerContext bodyCt(ct.getContext(), function, bodyBlock, outerAnnotation);
        ct.popContext();
        ct.pushContext(bodyCt);
        this->body->visit(ct);
        this->tailExp->visit(ct);
        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext nextCt(ct.getContext(), function, nextBlock, outerAnnotation);
        ct.popContext();
        ct.pushContext(nextCt);

        return this->vr;
      }
    }

    ExpFor::ExpFor(
        sp<Exp> initExp, sp<Exp> judgementExp, sp<Exp> tailExp, sp<StmtBlock> body,
        sp<al::ast::Annotation> annotation)
        :initExp(initExp), judgementExp(judgementExp), tailExp(tailExp), body(body), annotation(annotation)
    {
      if (annotation) {
        appendChild(initExp);
        appendChild(judgementExp);
        appendChild(tailExp);
        appendChild(body);
        appendChild(annotation);
      }
    }

    int Annotation::getBatchCount() {
//      if (!this->isBatchFor(nvmVarName))
//        return -1;

      auto args = this->getChildren()[0]->getChildren();
      if (this->getName() != "batch")
        return false;

      auto batchCountParam = dynamic_cast<IntLiteral*>(args[1].get());
      if (batchCountParam == nullptr) {
        cerr << "The parameters of @batch annotation must be symbol and int literal" << endl;
        abort();
      }

      int ret = -1;
      stringstream ss;
      ss << batchCountParam->getValue();
      ss >> ret;
      return ret;
    }
  }
}

