#pragma once

#include <string>
#include <vector>
#include <sstream>

class ArgParser {
public:
  ArgParser (int &argc, char **argv) {
    for (int i = 1; i < argc; ++i)
      this->tokens.emplace_back(argv[i]);
  }
  /// @author iain
  const std::string& getCmdOption(const std::string &option) const{
    std::vector<std::string>::const_iterator itr;
    itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
    if (itr != this->tokens.end() && ++itr != this->tokens.end()){
      return *itr;
    }
    static std::string emptyString;
    return emptyString;
  }
  template<typename T>
  T getCmdOption(const std::string &option, T defaultValue) const{
    if (!cmdOptionExists(option)) {
      return defaultValue;
    }
    auto opt = getCmdOption(option);
    T v;
    std::stringstream ss;
    ss << opt;
    ss >>  v;
    return v;
  }
  bool getCmdOption(const std::string &option, bool defaultValue) const{
    if (!cmdOptionExists(option)) {
      return defaultValue;
    }
    auto opt = getCmdOption(option);
    return opt == "true" || opt == "y" || opt == "yes" || opt == "1";
  }
  /// @author iain
  bool cmdOptionExists(const std::string &option) const{
    return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
  }
private:
  std::vector <std::string> tokens;
};
