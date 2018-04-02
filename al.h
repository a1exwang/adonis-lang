#pragma once

#include <memory>

namespace al {
  class CompileTime;
}

std::unique_ptr<al::CompileTime> compile(int argc, char **argv);
