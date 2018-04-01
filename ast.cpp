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
        :originalType(retType), attrs(attrs), fnTypeArgs(args) {
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
          this->llvmType = llvm::PointerType::get(
              this->originalType->getLlvmType(),
              (this->originalType->attrs & Persistent) ? PtrAddressSpace::NVM : PtrAddressSpace::Volatile
          );
        } else if (this->attrs & Array) {
          this->originalType->parseLlvmType(ct);
          this->llvmType = this->getArrayPtrType(this->originalType->getLlvmType());
        } else if (this->attrs & Persistent) {
          this->originalType->parseLlvmType(ct);
          this->bPersistent = true;
          this->llvmType = this->originalType->getLlvmType();
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

    llvm::Value *Type::sizeOfArray(const llvm::Module &mainModule, llvm::IRBuilder<> &builder, llvm::PointerType *arrayPtrType, llvm::Value *len) {
      auto lenSize = llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(arrayPtrType->getContext()),
          mainModule.getDataLayout().getTypeAllocSize(llvm::IntegerType::getInt64Ty(arrayPtrType->getContext()))
      );
      auto arrayStructType = reinterpret_cast<llvm::StructType*>(arrayPtrType->getElementType());
      auto arrayElementType = reinterpret_cast<llvm::ArrayType*>(arrayStructType->getElementType(1))->getElementType();
      auto arrayElementSize = mainModule.getDataLayout().getTypeAllocSize(arrayElementType);

      return builder.CreateAdd(
          builder.CreateMul(
              builder.CreateZExt(
                  len,
                  llvm::IntegerType::getInt64Ty(builder.getContext())
              ),
              llvm::ConstantInt::get(llvm::Type::getInt64Ty(arrayPtrType->getContext()), arrayElementSize)),
          lenSize
      );
    }

    llvm::PointerType *Type::getArrayPtrType(llvm::Type *elementType) {
      /**
       * struct {
       *   uint64_t len;
       *   T arr[0];
       * }
       */
      return llvm::PointerType::get(
          llvm::StructType::get(
              elementType->getContext(),
              {
                  llvm::IntegerType::getInt64Ty(elementType->getContext()),
                  llvm::ArrayType::get(elementType, 0)
              },
              false
          ),
          PtrAddressSpace::Volatile
      );
    }

    llvm::Value *Type::getArrayElementPtr(llvm::IRBuilder<> &builder, llvm::Value *arrStruct, llvm::Value *index) {
      auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(arrStruct->getContext()), 0);
      auto one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(arrStruct->getContext()), 1);
      auto arrPtr = builder.CreateGEP(arrStruct, {zero, one}, "");
      return builder.CreateGEP(arrPtr, {zero, index}, "");
    }

    llvm::Value *Type::createArrayByAlloca(const llvm::Module &mainModule, llvm::IRBuilder<> &builder, llvm::PointerType *arrPtrType, llvm::Value *len) {
      auto totalLen = Type::sizeOfArray(mainModule, builder, arrPtrType, len);

      return builder.CreatePointerCast(
          builder.CreateAlloca(llvm::IntegerType::getInt8Ty(builder.getContext()), totalLen),
          arrPtrType
      );
    }

    Type::Type(shared_ptr<Symbol> symbol, int attrs, llvm::Type *llvmType, shared_ptr<Exp> arraySizeVal) :symbol(std::move(symbol)), attrs(attrs), llvmType(llvmType), arraySizeVal(arraySizeVal) {
      if (arraySizeVal != nullptr) {
        appendChild(arraySizeVal);
      }
    }

    llvm::Value *Type::getArraySizeVal() const { return this->arraySizeVal->getVR().value; }

    Type::Type(const sp<Type> &originalType, int attrs, shared_ptr<Exp> arraySizeVal) :originalType(originalType), attrs(attrs), arraySizeVal(arraySizeVal) {
      if (originalType == nullptr) {
        std::cerr << "originalType == nullptr" << std::endl;
        abort();
      }
      appendChild(this->originalType);
      if (arraySizeVal) {
        appendChild(arraySizeVal);
      }
    }

    void Type::arrayCopy(const llvm::Module &mainModule, IRBuilder<> &builder, llvm::Value *dst, llvm::Value *src) {
      llvm::Value *n = Type::getDataSizeOfArray(mainModule, builder, dst);
      llvm::Value *dstData = Type::dataPtrOfArray(builder, dst);
      llvm::Value *srcData = Type::dataPtrOfArray(builder, src);

      builder.CreateMemCpy(
          dstData,
          srcData,
          n,
          1
      );
    }

    sp<Type> Type::getInt32Type(llvm::LLVMContext &context) {
      return std::make_shared<Type>(std::make_shared<Symbol>("int32"), None, llvm::IntegerType::getInt32Ty(context));
    }

    llvm::Value *Type::ptrToElementCountOfArray(IRBuilder<> &builder, llvm::Value *arr) {
      return builder.CreateBitCast(
          arr,
          llvm::PointerType::get(llvm::IntegerType::getInt64Ty(builder.getContext()), PtrAddressSpace::Volatile)
      );
    }

    llvm::Value *Type::dataPtrOfArray(llvm::IRBuilder<> &builder, llvm::Value *arr) {
      return builder.CreateGEP(
          arr,
          {
              llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 0),
              llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 1)
          }
      );
    }

    llvm::Value *Type::getDataSizeOfArray(const llvm::Module &mainModule, llvm::IRBuilder<> &builder, llvm::Value *arr) {
      auto elementType = reinterpret_cast<ArrayType*>(builder.CreateGEP(arr, {
          llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 0),
          llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 1)
      })->getType()->getPointerElementType())->getElementType();
      auto n = builder.CreateLoad(
          builder.CreateGEP(arr, {
              llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 0),
              llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(builder.getContext()), 0)
          })
      );

      return builder.CreateMul(
          builder.CreateZExt(n, llvm::IntegerType::getInt64Ty(builder.getContext())),
          llvm::ConstantInt::get(
              llvm::IntegerType::getInt64Ty(builder.getContext()),
              mainModule.getDataLayout().getTypeAllocSize(elementType))
      );
    }

    ExpCall::ExpCall(const std::shared_ptr<Symbol> &name, std::vector<std::shared_ptr<Exp>> exps)
        :ExpCall(name->getName(), exps) { }

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
      else if (this->name == "!=") {
        if (args.size() != 2) { cerr << "'<' only accepts 2 args" << endl; abort(); }

        llvm::Value *lhsInt = args[0], *rhsInt = args[1];
        if (args[0]->getType()->isPointerTy()) {
           lhsInt = ct.getCompilerContext().builder->CreatePtrToInt(
              args[0],
              llvm::Type::getInt64Ty(ct.getContext())
          );
        }
        if (args[1]->getType()->isPointerTy()) {
          rhsInt = ct.getCompilerContext().builder->CreatePtrToInt(
              args[1],
              llvm::Type::getInt64Ty(ct.getContext())
          );
        }
        vr.value = ct.getCompilerContext().builder->CreateZExt(
            ct.getCompilerContext().builder->CreateNot(
                ct.getCompilerContext().builder->CreateICmp(
                    llvm::CmpInst::Predicate::ICMP_EQ,
                    lhsInt,
                    rhsInt
                )
            ),
            llvm::IntegerType::getInt32Ty(ct.getContext())
        );
      }
      else if (this->name == "<<") {
        if (args.size() != 2) { cerr << "'<' only accepts 2 args" << endl; abort(); }
        vr.value = ct.getCompilerContext().builder->CreateZExt(
            ct.getCompilerContext().builder->CreateShl(args[0], args[1]),
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

      CompilerContext cc(ct.getContext(), fn, BasicBlock::Create(ct.getContext(), "entry", fn), nullptr);
      ct.pushContext(cc);

      int i = 0;
      for (auto &arg : fn->args()) {
        auto varNewLocation = ct.getCompilerContext().builder->CreateAlloca(argTypes[i]);
        ct.getCompilerContext().builder->CreateStore(&arg, varNewLocation);
        ct.setFunctionStackVariable(fn->getName(), argNames[i], varNewLocation);
        i++;
      }

      for (auto &child : this->getChildren()) {
        child->visit(ct);
      }

      if (fn->getReturnType()->isVoidTy()) {
        ct.getCompilerContext().builder->CreateRetVoid();
      }
//      if (!verifyFunction(*ct.getCompilerContext().function)) {
//        cout << "failed to verifyFunction " << ct.getCompilerContext().function->getName().str() << endl;
//      }
      ct.popContext();

      return this->vr;
    }

    void FnDef::preTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) {
//      cout << "preTraverse(): push scope, name=" << this->getName() << endl;
      pass.scopeStack.push_back(this->getName());
    }
    void FnDef::postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) {
//      cout << "postTraverse(): scope, pop scope name=" << this->getName() << endl;
      pass.scopeStack.pop_back();
    }

    void ExpVarRef::postVisit(CompileTime &ct) {
      auto &cont = ct.getCompilerContext();
      auto funcVarName = string(ct.getCompilerContext().function->getName()) + "::" + this->name->getName();
      bool hasGlobalVar = ct.hasSymbol(this->name->getName());
      bool hasFunctionVar = ct.hasSymbol(funcVarName);

      if (ct.hasFunctionStackVariable(ct.getCompilerContext().function->getName(), this->name->getName())) {
        // stack variables
        auto var = ct.getFunctionStackVariable(ct.getCompilerContext().function->getName(), this->name->getName());
        vr.value = cont.builder->CreateLoad(var);
        vr.gepResult = var;
        this->varRefType = StackVolatile;
      }
      else if (hasGlobalVar || hasFunctionVar) {
        // function/global persistent/volatile variables
        string varName;
        if (hasGlobalVar) {
          varName = this->name->getName();
          this->varRefType = VarRefType::GlobalPersistent;
        } else {
          varName = funcVarName;
          this->varRefType = VarRefType::FunctionPersistent;
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
        if (vr.value != nullptr) {
          this->varRefType = VarRefType::Function;
        } else {
          // external global symbol(functions/variables)
          vr.value = ct.getMainModule()->getGlobalVariable(this->name->getName());
          if (vr.value == nullptr) {
            cerr << "no stack var, function persistent var, global var, function or global persistent var found named '" << this->name->getName() << "'" << endl;
            abort();
          }
          this->varRefType = VarRefType::External;
        }
      }
    }

    std::string ExpVarRef::getName() const { return name->getName(); }

    void ExpVarRef::postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) {
//      printf("ExpVarRef::postTraverse() %s, %s, %d\n", pass.getFnName().c_str(), this->getName().c_str(), pass.isAssignLhs);
      if (!pass.isAssignLhs) {
        pass.setPvarTag(pass.getFnName(), this->getName(), PersistentVarTaggingPass::PvarTag::Readable);
      }
    }

    void IntLiteral::postVisit(CompileTime &ct) {
      stringstream ss;
      int i;
      ss << this->s;
      ss >> i;
      vr.value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ct.getContext()), i);
    }

    sp<Type> IntLiteral::getType(CompileTime &ct) {
      return Type::getInt32Type(ct.getContext());
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
        auto structPtr = ct.getCompilerContext().builder->CreatePointerCast(
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
        auto elementType = structAstType->getLlvmType()->getStructElementType(idx);
        if (elementType->isIntegerTy(32) || elementType->isPointerTy()) {
          vr.gepResult = ct.getCompilerContext().builder->CreatePointerBitCastOrAddrSpaceCast(
              elementPtr,
              llvm::PointerType::get(elementType, structAstType->isPersistent() ? PtrAddressSpace::NVM : PtrAddressSpace::Volatile)
          );
          vr.value = ct.getCompilerContext().builder->CreateLoad(vr.gepResult);
        }
        else {
          cerr << "memberAccess element type not supported " << structAstType->getMembers()[idx] << endl;
          abort();
        }
      }
    }

    void StructBlock::preVisit(CompileTime &ct) {
      auto name = this->name->getName();
      auto llvmType = llvm::StructType::create(ct.getContext(), name);
      vector<string> a;
      this->type = std::make_shared<ast::Type>(
          std::make_shared<Symbol>(name.c_str()),
          llvmType,
          a
      );
      ct.registerType(name, this->type);
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
      reinterpret_cast<StructType*>(this->type->getLlvmType())->setBody(elements);
      this->type->setMemberNames(elementNames);
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

      // TODO this is a hack for simple variable assignment
      auto lhsVarRef = dynamic_pointer_cast<ExpVarRef>(exps[0]);
      if (lhsVarRef != nullptr) {
        auto fnName = ct.getCompilerContext().function->getName();

//        cout << "ExpAssign::postVisit() " << string(fnName) << " " << string(lhsVarRef->getName())
//             << " " << ct.getPvarTagPass().hasPvarTag(fnName, lhsVarRef->getName())
//             << " " << ct.getPvarTagPass().getPvarTag(fnName, lhsVarRef->getName()) << endl;
        if (ct.getPvarTagPass().hasPvarTag(fnName, lhsVarRef->getName()) &&
            ct.getPvarTagPass().getPvarTag(fnName, lhsVarRef->getName()) != PersistentVarTaggingPass::Readable) {
          // opt it out
          cout << "opt-out " << lhsVarRef->getName() << endl;
          vr = rhs->getVR();
          return;
        }
      }

      auto rhsVal = rhs->getVR().value;
      auto rhsPtr = rhs->getVR().gepResult;

      auto lhs = dynamic_pointer_cast<Exp>(exps[0]);
      auto lhsPtr = lhs->getVR().gepResult;

      // If lhs is a symbol, we support a batch operation
      llvm::Value *onBatchSizeVal = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 1);
      if (ct.getCompilerContext().annotation) {
        onBatchSizeVal = ct.getCompilerContext().annotation->getOnBatchSizeVal();
      }

      ct.createAssignment(
          lhsPtr->getType()->getPointerElementType(), // TODO
          lhsPtr,
          rhsVal,
          rhsPtr,
          onBatchSizeVal
      );

      vr = rhs->getVR();
    }

    void ExpAssign::traverse(CompileTime &ct, PersistentVarTaggingPass &pass) {
//      printf("ExpAssign::traverse() \n");

      auto &children = this->getChildren();
      for (int i = 0; i < children.size(); ++i) {
        if (i == 0) {
          pass.isAssignLhs = true;
//          cout << "ExpAssign::traverse() isAssignLhs = true" << endl;
          children[i]->traverse(ct, pass);
//          cout << "ExpAssign::traverse() isAssignLhs = false" << endl;
          pass.isAssignLhs = false;
        } else {
          children[i]->traverse(ct, pass);
        }
      }
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
        ptr->print(llvm::errs());
        abort();
      }
    }

    void ExpStackVarDef::postVisit(CompileTime &ct) {
      auto expVr = this->exp->getVR();
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
        // TODO: generalize array definition
        if (type->getAttrs() & Type::Array) {
          var = type->createArrayByAlloca(
              *ct.getMainModule(),
              *ct.getCompilerContext().builder,
              reinterpret_cast<llvm::PointerType*>(type->getLlvmType()),
              type->getArraySizeVal()
          );
          ct.getCompilerContext().builder->CreateStore(
              ct.getCompilerContext().builder->CreateZExt(
                  type->getArraySizeVal(),
                  llvm::IntegerType::getInt64Ty(ct.getContext())
              ),
              Type::ptrToElementCountOfArray(*ct.getCompilerContext().builder, var)
          );
        } else {
          var = cc.builder->CreateAlloca(llvmType);
        }
        ct.setFunctionStackVariable(
            ct.getCompilerContext().function->getName(),
            this->decl->getName(),
            var
        );
      }
      ct.createAssignment(
          llvmType,
          var,
          expVr.value,
          expVr.gepResult,
          nullptr,
          (type->getAttrs() & Type::Array) != 0
      );
    }

    void ExpStackVarDef::postTraverse(CompileTime &ct, PersistentVarTaggingPass &pass) {
      pass.setPvarTag(pass.getFnName(), this->decl->getName(), PersistentVarTaggingPass::PvarTag::None);
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

        auto judgementBlock = BasicBlock::Create(ct.getContext(), "for_judgement", function);
        auto bodyBlock = BasicBlock::Create(ct.getContext(), "for_body", function);
        auto doneBlock = BasicBlock::Create(ct.getContext(), "for_done", function);

        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext judgementCt(ct.getContext(), function, judgementBlock, doneBlock, this->annotation);
        ct.popContext();
        ct.pushContext(judgementCt);

        auto judgeResult = this->judgementExp->visit(ct);
        auto isFalse = ct.getCompilerContext().builder->CreateICmp(
            llvm::CmpInst::Predicate::ICMP_EQ,
            judgeResult.value,
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0, true)
        );
        ct.getCompilerContext().builder->CreateCondBr(
            isFalse,
            doneBlock,
            bodyBlock
        );

        CompilerContext bodyCt(ct.getContext(), function, bodyBlock, doneBlock, this->annotation);
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

        auto nextBlock = BasicBlock::Create(ct.getContext(), "for_next", function);
        llvm::IRBuilder<> doneBlockBuilder(doneBlock);
        doneBlockBuilder.CreateBr(nextBlock);

        CompilerContext nextCt(ct.getContext(), function, nextBlock, nullptr, this->annotation);
        ct.popContext();
        ct.pushContext(nextCt);

        return this->vr;

      } else {
        // init expression
        this->initExp->visit(ct);

        auto function = ct.getCompilerContext().function;

        auto judgementBlock = BasicBlock::Create(ct.getContext(), "for_judgement", function);
        auto bodyBlock = BasicBlock::Create(ct.getContext(), "for_body", function);
        auto doneBlock = BasicBlock::Create(ct.getContext(), "for_done", function);

        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        CompilerContext judgementCt(ct.getContext(), function, judgementBlock, doneBlock, outerAnnotation);
        ct.popContext();
        ct.pushContext(judgementCt);

        auto judgeResult = this->judgementExp->visit(ct);
        auto isFalse = ct.getCompilerContext().builder->CreateICmp(
            llvm::CmpInst::Predicate::ICMP_EQ,
            judgeResult.value,
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0, true)
        );
        ct.getCompilerContext().builder->CreateCondBr(
            isFalse,
            doneBlock,
            bodyBlock
        );

        CompilerContext bodyCt(ct.getContext(), function, bodyBlock, doneBlock, outerAnnotation);
        ct.popContext();
        ct.pushContext(bodyCt);
        this->body->visit(ct);
        this->tailExp->visit(ct);
        ct.getCompilerContext().builder->CreateBr(judgementBlock);

        auto nextBlock = BasicBlock::Create(ct.getContext(), "for_next", function);
        llvm::IRBuilder<> doneBlockBuilder(doneBlock);
        doneBlockBuilder.CreateBr(nextBlock);

        CompilerContext nextCt(ct.getContext(), function, nextBlock, nullptr, this->annotation);
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
      appendChildIfNotNull(annotation);
      appendChildIfNotNull(initExp);
      appendChildIfNotNull(judgementExp);
      appendChildIfNotNull(tailExp);
      appendChildIfNotNull(body);
    }

    VisitResult ExpIf::visit(CompileTime &ct) {
      auto vr = this->cond->visit(ct);

      auto function = ct.getCompilerContext().function;
      auto outerAnnotation = ct.getCompilerContext().annotation;
      auto breakToBlock = ct.getCompilerContext().breakToBlock;

      auto trueBlock = BasicBlock::Create(ct.getContext(), "if_true", function);
      auto falseBlock = BasicBlock::Create(ct.getContext(), "if_false", function);
      auto nextBlock = BasicBlock::Create(ct.getContext(), "if_next", function);

      auto isFalse = ct.getCompilerContext().builder->CreateICmp(
          llvm::CmpInst::Predicate::ICMP_EQ,
          vr.value,
          llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), 0, true)
      );
      ct.getCompilerContext().builder->CreateCondBr(
          isFalse,
          falseBlock,
          trueBlock
      );

      CompilerContext trueCt(ct.getContext(), function, trueBlock, breakToBlock, outerAnnotation);
      ct.popContext();
      ct.pushContext(trueCt);

      this->trueBranch->visit(ct);
      ct.getCompilerContext().builder->CreateBr(nextBlock);

      CompilerContext falseCt(ct.getContext(), function, falseBlock, breakToBlock, outerAnnotation);
      ct.popContext();
      ct.pushContext(falseCt);

      this->falseBranch->visit(ct);
      ct.getCompilerContext().builder->CreateBr(nextBlock);

      CompilerContext nextCt(ct.getContext(), function, nextBlock, breakToBlock, outerAnnotation);
      ct.popContext();
      ct.pushContext(nextCt);
      return this->vr;
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

    void ExpMove::postVisit(CompileTime &ct) {
      if (this->lhs->getVarRefType() == this->rhs->getVarRefType() &&
          this->lhs->getVarRefType() == ExpVarRef::VarRefType::StackVolatile) {
        auto fnName = ct.getCompilerContext().function->getName();
        auto rhsVar = ct.getFunctionStackVariable(ct.getCompilerContext().function->getName(), this->rhs->getName());
        ct.setFunctionStackVariable(fnName, this->lhs->getName(), rhsVar);
        ct.unsetFunctionStackVariable(fnName, this->rhs->getName());
      }
    }

    void ArrayLiteral::postVisit(CompileTime &ct) {
      // TODO: add for empty array support
      assert(!this->exps->getChildren().empty());
      auto firstElement = dynamic_pointer_cast<Exp>(this->exps->getChildren()[0]);

      stringstream ss;
      string sInt;
      ss << this->exps->getChildren().size();
      ss >> sInt;
      this->type = std::make_shared<Type>(
          firstElement->getType(ct),
          Type::Array,
          std::make_shared<ast::IntLiteral>(sInt)
      );
      this->type->visit(ct);

      auto arrayElementPtrType = reinterpret_cast<llvm::PointerType*>(this->type->getLlvmType());
      auto arr = Type::createArrayByAlloca(
          *ct.getMainModule(),
          *ct.getCompilerContext().builder,
          arrayElementPtrType,
          this->type->getArraySizeVal()
      );
      for (int i = 0; i < this->exps->getChildren().size(); ++i) {
        auto exp = dynamic_pointer_cast<Exp>(this->exps->getChildren()[i]);
        auto vr = exp->getVR();
        ct.createAssignment(
            arrayElementPtrType->getElementType(),
            Type::getArrayElementPtr(
                *ct.getCompilerContext().builder,
                arr,
                llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), i)
            ),
            vr.value,
            vr.gepResult,
            nullptr,
            false /* FIXME: assume array element cannot be an array */
        );
      }

      this->vr.gepResult = arr;
    }

    void ExpArrayIndex::postVisit(CompileTime &ct) {
      auto elementPtr = Type::getArrayElementPtr(*ct.getCompilerContext().builder, arr->getVR().gepResult, this->index->getVR().value);
      this->vr.gepResult = elementPtr;
      this->vr.value = ct.getCompilerContext().builder->CreateLoad(elementPtr);
    }

    void ExpSizeOf::postVisit(CompileTime &ct) {
      this->size = ct.getMainModule()->getDataLayout().getTypeAllocSize(type->getLlvmType());
      vr.value = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(ct.getContext()), this->size);
    }

    void ExpReturn::postVisit(CompileTime &ct) {
      // FIXME: llvm only support return at the end of a function
      if (exp) {
        ct.getCompilerContext().builder->CreateRet(exp->getVR().value);
      }
      else {
        ct.getCompilerContext().builder->CreateRetVoid();
      }
    }

    void ExpBreak::postVisit(CompileTime &ct) {
      if (!ct.getCompilerContext().breakToBlock) {
        cerr << "Cannot break without a loop" << endl;
        abort();
      }

      auto invalidBlock = BasicBlock::Create(ct.getContext(), "break_useless", ct.getCompilerContext().function);

      ct.getCompilerContext().builder->CreateCondBr(
          llvm::ConstantInt::get(llvm::IntegerType::getInt1Ty(ct.getContext()), 1),
          ct.getCompilerContext().breakToBlock,
          invalidBlock
      );

      CompilerContext invalidCt(
          ct.getContext(),
          ct.getCompilerContext().function,
          invalidBlock,
          nullptr,
          ct.getCompilerContext().annotation
      );
      ct.popContext();
      ct.pushContext(invalidCt);
    }
  }
}

