#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include <iostream>
#include <fstream>
#include "parser.tab.hpp"
#include "lex.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdarg>
#include <csignal>

using namespace llvm;
using namespace std;

al::CompileTime *compile() {
  al::CompileTime *rt = new al::CompileTime;

  ifstream ifs("/home/alexwang/dev/proj/cpp/adonis-lang/nvm.al");
  std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  al::Lexer lexer(str);
  al::Parser parser(lexer, *rt);
  parser.parse();

  /**
   * AST passes
   */
  rt->init1();
  rt->traverse1();
  rt->finish1();

  return rt;
}
