%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.0"

%defines
%define parser_class_name { Parser }
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define api.namespace { al }

%code requires
{
    #include <iostream>
    #include <string>
    #include <vector>
    #include <stdint.h>
    #include "ast.h"
    #include "compile_time.h"

    using namespace std;

    namespace al {
        class Lexer;
        class Token;
    }
}

%code top
{
    #include <iostream>
    #include "lex.h"
    #include "parser.tab.hpp"
    #include "location.hh"

    static al::Parser::symbol_type yylex(al::Lexer &lexer) {
        return lexer.lex();
    }

    using namespace al;
}

%code {
}

%lex-param { al::Lexer &lexer }
%parse-param { al::Lexer &lexer } { al::CompileTime &rt }
%locations
%define parse.trace
%define parse.error verbose

%token <std::shared_ptr<al::ast::StringLiteral>> STRING_LIT
%token <std::shared_ptr<al::ast::Symbol>> SYMBOL_LIT
%token <std::string> INT_LIT
%token QUOTE "'";
%token LEFTPAR "(";
%token RIGHTPAR ")";
%token PLUS
%token EQ
%token RIGHT_ARROW "->";
%token LEFTBRACE RIGHTBRACE
%token COLON COMMA BANG DOT
%token FN REF NV STRUCT LET
%token SEMICOLON ";";

%token END 0 "end of file"

%type< std::shared_ptr<al::ast::Exp> > exp;
%type< std::shared_ptr<al::ast::ExpList> > exps;
%type< std::shared_ptr<al::ast::Blocks> > blocks;

%type< std::shared_ptr<al::ast::Block> > block;
%type< std::shared_ptr<al::ast::FnBlock> > fn_block;

%type< std::shared_ptr<al::ast::StructBlock> > struct_block;
%type< std::vector<std::tuple<std::string, std::shared_ptr<al::ast::Type>>> > struct_elements;
%type< std::tuple<std::shared_ptr<al::ast::Symbol>, std::shared_ptr<al::ast::Type>> > struct_element;
%type< std::tuple<std::shared_ptr<al::ast::Symbol>, std::shared_ptr<al::ast::Type>> > var_decl;

%start program

%%

program : blocks { rt.setASTRoot($1); }

blocks: { $$ = std::make_shared<al::ast::Blocks>(); }
    | block blocks { $$ = $2->append($1); }

block: struct_block { $$ = $1; }
    | fn_block { $$ = $1; }

/* struct_block:
 *  struct User {
 *    name: string,
 *    name1: string,
 *  }
 */
struct_block: STRUCT SYMBOL_LIT LEFTBRACE struct_elements RIGHTBRACE {
    //$$ = std::make_shared<al::ast::StructBlock>($2, $4);
  }
struct_elements:
    | struct_element struct_elements
struct_element: var_decl COMMA

/* fn_block
 * fn add(a: int, b: int) int {
 *    a + b
 * }
 */
fn_block: FN SYMBOL_LIT LEFTPAR fn_args RIGHTPAR type stmt_block
fn_args:
    | fn_arg fn_args
fn_arg: var_decl COMMA

stmt_block: LEFTBRACE stmts RIGHTBRACE
stmts:
    | stmt stmts
stmt: exp SEMICOLON
    | LET SYMBOL_LIT EQ exp SEMICOLON

exp: exp_call
    | exp_ctor
    | exp_nvctor
    | exp_op

exp_call: SYMBOL_LIT LEFTPAR exps RIGHTPAR
exp_ctor: SYMBOL_LIT LEFTBRACE exps RIGHTBRACE
exp_nvctor: SYMBOL_LIT BANG LEFTBRACE exps RIGHTBRACE
exp_op: exp PLUS exp

exps: exp
    | exp exps

var_decl: SYMBOL_LIT COLON type
type: SYMBOL_LIT
    | REF SYMBOL_LIT
    | NV REF SYMBOL_LIT

%%
void al::Parser::error(const location &loc , const std::string &message) {
    cout << "Error: " << message << endl;
}
