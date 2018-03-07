#pragma once

#include <llvm/IR/Module.h>
#include "compile_time.h"

al::CompileTime *compile(int argc, char **argv);
