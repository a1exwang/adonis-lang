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

al::CompileTime *compile(int argc, char** argv) {
  al::CompileTime *rt = new al::CompileTime;

  if (argc != 2) {
    cerr << "wrong arguments" << endl;
    abort();
  }

  ifstream ifs(argv[1]);
  std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  al::Lexer lexer(str);
  al::Parser parser(lexer, *rt);
  parser.parse();

  /**
   * AST passes
   */

  rt->init1();
  rt->traverseAll();
  rt->finish1();

  return rt;
}
