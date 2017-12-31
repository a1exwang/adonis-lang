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

%token LEFTBRACE RIGHTBRACE
%token COLON COMMA BANG DOT
%token FN REF NV STRUCT LET
%token PERSISTENT
%token RIGHT_ARROW "->";
%token SEMICOLON ";";
%token QUOTE "'";
%right EQ
%left PLUS
%token LEFTPAR "(";
%token RIGHTPAR ")";
%token <std::shared_ptr<al::ast::StringLiteral>> STRING_LIT
%token <std::shared_ptr<al::ast::Symbol>> SYMBOL_LIT
%token <std::shared_ptr<al::ast::IntLiteral>> INT_LIT

%token END 0 "end of file"

%type< std::shared_ptr<al::ast::ExpList> > exps;
%type< std::shared_ptr<al::ast::Blocks> > blocks;

%type< std::shared_ptr<al::ast::Block> > block;

%type< std::shared_ptr<al::ast::PersistentBlock> > persistent_block;
%type< std::shared_ptr<al::ast::VarDecls> > var_decls;
%type< std::shared_ptr<al::ast::VarDecl> > var_decl;

%type< std::shared_ptr<al::ast::Stmt> > stmt;
%type< std::shared_ptr<al::ast::StmtBlock> > stmt_block;
%type< std::shared_ptr<al::ast::Stmts> > stmts;

%type< std::shared_ptr<al::ast::FnDef> > fn_block;

%type< std::shared_ptr<al::ast::Exp> > exp;
%type< std::shared_ptr<al::ast::ExpCall> > exp_op;
%type< std::shared_ptr<al::ast::ExpCall> > exp_call;
%type< std::shared_ptr<al::ast::ExpVarRef> > exp_var_ref;

%type< std::shared_ptr<al::ast::Type> > type;

//%type< std::shared_ptr<al::ast::StructBlock> > struct_block;
//%type< std::vector<std::tuple<std::string, std::shared_ptr<al::ast::Type>>> > struct_elements;
//%type< std::tuple<std::shared_ptr<al::ast::Symbol>, std::shared_ptr<al::ast::Type>> > struct_element;

%start program

%%

program : blocks { rt.setASTRoot($1); }

blocks: { $$ = std::make_shared<al::ast::Blocks>();  }
    | block blocks { $$ = $2; $$->prependChild($1); }

block: persistent_block { $$ = $1; }
    | fn_block { $$ = $1; }

/* struct_block:
 *  struct User {
 *    name: string,
 *    name1: string,
 *  }
struct_block: STRUCT SYMBOL_LIT LEFTBRACE struct_elements RIGHTBRACE {
    //$$ = std::make_shared<al::ast::StructBlock>($2, $4);
  }
struct_elements:
    | struct_element struct_elements
struct_element: var_decl COMMA
 */

persistent_block: PERSISTENT LEFTBRACE var_decls RIGHTBRACE {
      $$ = std::make_shared<al::ast::PersistentBlock>($3);
    }

var_decls: { $$ = std::make_shared<al::ast::VarDecls>(); }
    | var_decl SEMICOLON var_decls { $$ = $3; $$->prependChild($1); }

/* fn_block
 * fn add(a: int, b: int) int {
 *    a + b
 * }
 */
fn_block: FN SYMBOL_LIT LEFTPAR RIGHTPAR stmt_block {
        $$ = std::make_shared<al::ast::FnDef>($2, $5);
    }
/*fn_args:
    | fn_arg fn_args
fn_arg: var_decl COMMA
*/

stmt_block: LEFTBRACE stmts RIGHTBRACE {
        $$ = std::make_shared<al::ast::StmtBlock>($2);
    }
stmts: { $$ = std::make_shared<al::ast::Stmts>(); }
    | stmt stmts { $$ = $2; $$->prependChild($1); }
stmt: exp SEMICOLON { $$ = $1; }
//    | LET SYMBOL_LIT EQ exp SEMICOLON

exp: exp_call { $$ = $1; }
    | exp_op { $$ = $1; }
    | exp_var_ref { $$ = $1; }
    | INT_LIT { $$ = $1; }
/*
    | exp_ctor
    | exp_nvctor
*/

exp_call: SYMBOL_LIT LEFTPAR exps RIGHTPAR {
    $$ = std::make_shared<al::ast::ExpCall>($1, $3->toVector());
}
//exp_ctor: SYMBOL_LIT LEFTBRACE exps RIGHTBRACE
//exp_nvctor: SYMBOL_LIT BANG LEFTBRACE exps RIGHTBRACE
exp_op: exp PLUS exp {
      $$ = std::make_shared<al::ast::ExpCall>("+", std::vector<std::shared_ptr<al::ast::Exp>>({$1, $3})); }
    | exp EQ exp { $$ = std::make_shared<al::ast::ExpCall>("=", std::vector<std::shared_ptr<al::ast::Exp>>{$1, $3}); }

exp_var_ref: SYMBOL_LIT { $$ = std::make_shared<al::ast::ExpVarRef>($1); }

exps: { $$ = std::make_shared<al::ast::ExpList>(); }
    | exp exps { $$ = $2; $$->prependChild($1); }


var_decl: SYMBOL_LIT COLON type {
    $$ = std::make_shared<al::ast::VarDecl>($1, $3);
}
type: SYMBOL_LIT { $$ = std::make_shared<al::ast::Type>($1); }
//    | REF SYMBOL_LIT
//    | NV REF SYMBOL_LIT

%%
void al::Parser::error(const location &loc , const std::string &message) {
    cout << "Error: " << message << endl;
}
