#pragma once
#include <string>
#include <vector>
#include <map>


struct PersistentVarTaggingPass {
  std::vector<std::string> scopeStack;

  enum PvarTag {
    None = 0,
    Readable = 1,
  };

  bool hasPvarTag(const std::string &fnName, const std::string &varName) {
    return this->pvarTags.find(fnName + "::" + varName) != this->pvarTags.end();
  }
  void setPvarTag(const std::string &fnName, const std::string &varName, int tag) {
    this->pvarTags[fnName + "::" + varName] = tag;
  }
  int getPvarTag(const std::string &fnName, const std::string &varName) {
    return this->pvarTags[fnName + "::" + varName];
  }
  std::string getFnName() {
    return *(scopeStack.end()-1);
  }

  std::map<std::string, int> pvarTags;
  bool isAssign = false;
};