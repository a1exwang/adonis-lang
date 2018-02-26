//
// Created by alexwang on 11/24/17.
//

#include "lex.h"
#include <tuple>

bool al::Lexer::parseQuoteString(std::string &str, std::string eos) {
  string content;
  bool escaping = false;
  while (!input.empty()) {
    string cp = nextUtf8CodePoint();
    if (escaping) {
      escaping = false;
      if (cp == "n") {
        content += "\n";
      }
      else {
        content += cp;
      }
      continue;
    }
    if (cp == "\\") {
      escaping = true;
      continue;
    }
    if (cp == eos) {
      str = content;
      return true;
    }
    content += cp;
  }
  str = content;
  return false;
}

al::Parser::symbol_type al::Lexer::lex() {
  typedef std::function<Parser::symbol_type (const std::string &s)> LexFn;
  std::tuple<string, LexFn> tuples[] = {
      {"\\s+", nullptr},
      {"#.*\n", nullptr},
      {
          "\\(",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_LEFTPAR(Parser::location_type());
          }
      },
      {
          "\\:",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_COLON(Parser::location_type());
          }
      },
      {
          "\\;",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_SEMICOLON(Parser::location_type());
          }
      },
      {
          "\\,",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_COMMA(Parser::location_type());
          }
      },
      {
          "\\)",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_RIGHTPAR(Parser::location_type());
          }
      },
      {
          "\\{",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_LEFTBRACE(Parser::location_type());
          }
      },
      {
          "\\}",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_RIGHTBRACE(Parser::location_type());
          }
      },
      {
          "\"",
          [this](const std::string &s) -> Parser::symbol_type {
            std::string result;
            if (!this->parseQuoteString(result, "'"))
              throw "failed to parse quote string";

            auto p = std::make_shared<ast::StringLiteral>(result);
            return Parser::make_STRING_LIT(p, Parser::location_type());
          }
      },
      {
          "\\!",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_BANG(Parser::location_type());
          }
      },
      {
          "\\+",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_PLUS(Parser::location_type());
          }
      },
      {
          "\\*",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_STAR(Parser::location_type());
          }
      },
      {
          "\\=",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_EQ(Parser::location_type());
          }
      },
      {
          "\\&",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_AND(Parser::location_type());
          }
      },
      {
          "\\.",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_DOT(Parser::location_type());
          }
      },
      {
          "\\.",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_DOT(Parser::location_type());
          }
      },

      // Keywords
      {
          "fn",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_FN(Parser::location_type());
          }
      },
      {
          "struct",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_STRUCT(Parser::location_type());
          }
      },
      {
          "persistent",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_PERSISTENT(Parser::location_type());
          }
      },
      {
          "extern",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_EXTERN(Parser::location_type());
          }
      },
      // General Tokens
      {
          "\\d+",
          [](const std::string &s) -> Parser::symbol_type {
            return Parser::make_INT_LIT(std::make_shared<al::ast::IntLiteral>(s), Parser::location_type());
          }
      },
      {
          "[A-Za-z]\\w*",
          [](const std::string &s) -> Parser::symbol_type {
            auto p = std::make_shared<ast::Symbol>(s);
            return Parser::make_SYMBOL_LIT(p, Parser::location_type());
          },

      }
  };
  if (input.empty()) {
    return al::Parser::make_END(Parser::location_type());
  }

  int i = 0;
  string reg;
  LexFn fn;
  for (auto &tuple: tuples) {
    std::tie(reg, fn) = tuple;
    string var;

    RE2 re("((?m:" + reg + "))");
    if (RE2::Consume(&input, re, &var)) {
      // FIXME: i == 0 for blank characters, i == 1 for comments
      if (i >= 2)
        return fn(var);
    }
    i++;
  }

  if (input.empty()) {
    return al::Parser::make_END(Parser::location_type());
  }

  cerr << "input not parseable" << endl;
  cerr << input.ToString() << endl;
  abort();
}
