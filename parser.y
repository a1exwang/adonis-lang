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

%lex-param { al::Lexer &lexer }
%parse-param { al::Lexer &lexer } { al::CompileTime &rt }
%locations
%define parse.trace
%define parse.error verbose

%token FN FOR STRUCT PERSISTENT EXTERN VOLATILE
%token SEMICOLON ";";
%token COLON COMMA BANG AT
%token QUOTE "'";
%right RIGHT_ARROW
%right EQ
%left LT
%left AND
%left PLUS
%left STAR
%left DOT
%token LEFTBRACE RIGHTBRACE
%token LEFTPAR RIGHTPAR
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
%type< std::shared_ptr<al::ast::ExpAssign> > exp_assign;
%type< std::shared_ptr<al::ast::ExpCall> > exp_call;
%type< std::shared_ptr<al::ast::ExpVarRef> > exp_var_ref;
%type< std::shared_ptr<al::ast::ExpStackVarDef> > exp_var_def;
%type< std::shared_ptr<al::ast::ExpMemberAccess> > exp_member;
%type< std::shared_ptr<al::ast::Literal> > exp_lit;
%type< std::shared_ptr<al::ast::ExpDeref> > exp_deref;
%type< std::shared_ptr<al::ast::ExpGetAddr> > exp_get_addr;
%type< std::shared_ptr<al::ast::ExpVolatileCast> > exp_volatile_cast;
%type< std::shared_ptr<al::ast::ExpFor> > exp_for;

%type< std::shared_ptr<al::ast::Type> > type;
%type< std::shared_ptr<al::ast::Annotation> > annotation;

%type< std::shared_ptr<al::ast::StructBlock> > struct_block;
%type< std::shared_ptr<al::ast::ExternBlock> > extern_block;

%type< std::shared_ptr<al::ast::VarDecls> > fn_args;
%type< std::shared_ptr<al::ast::FnDecl> > fn_decl;
%type< std::shared_ptr<al::ast::Decls> > decls;

%start program

%%

program : blocks { rt.setASTRoot($1); }

blocks: { $$ = std::make_shared<al::ast::Blocks>();  }
    | block blocks { $$ = $2; $$->prependChild($1); }

block: persistent_block { $$ = $1; }
    | fn_block { $$ = $1; }
    | struct_block { $$ = $1; }
    | extern_block { $$ = $1; }

/* struct_block:
 *  struct User {
 *    name: int32;
 *    name1: int32;
 *  }
 */
struct_block: STRUCT SYMBOL_LIT LEFTBRACE var_decls RIGHTBRACE {
      $$ = std::make_shared<al::ast::StructBlock>($2, $4);
    }

persistent_block: PERSISTENT LEFTBRACE var_decls RIGHTBRACE {
      $$ = std::make_shared<al::ast::PersistentBlock>($3);
    }

extern_block: EXTERN LEFTBRACE decls RIGHTBRACE {
      $$ = std::make_shared<al::ast::ExternBlock>($3);
    }
decls: { $$ = std::make_shared<al::ast::Decls>(); }
    | fn_decl SEMICOLON decls { $$ = $3; $$->prependChild($1); }
    | var_decl SEMICOLON decls { $$ = $3; $$->prependChild($1); }

/* fn_block
 * fn add(a: int, b: int) int {
 *    a + b
 * }
 */
var_decls: var_decl { $$ = std::make_shared<al::ast::VarDecls>(); $$->prependChild($1); }
    | var_decl var_decls { $$ = $2; $$->prependChild($1); }
    | var_decl COMMA var_decls { $$ = $3; $$->prependChild($1); }
fn_args: var_decls { $$ = $1; }
fn_decl: FN SYMBOL_LIT LEFTPAR fn_args RIGHTPAR {
      $$ = std::make_shared<al::ast::FnDecl>($2, std::make_shared<al::ast::Type>(), $4);
    }
    | FN SYMBOL_LIT LEFTPAR RIGHTPAR { $$ = std::make_shared<al::ast::FnDecl>($2); }
    | FN SYMBOL_LIT LEFTPAR fn_args RIGHTPAR type { $$ = std::make_shared<al::ast::FnDecl>($2, $6, $4); }
    | FN SYMBOL_LIT LEFTPAR RIGHTPAR type { $$ = std::make_shared<al::ast::FnDecl>($2, $5); }
fn_block: fn_decl stmt_block {
        $$ = std::make_shared<al::ast::FnDef>($1, $2);
    }

/* stmt_block
 * { a = 1; putsInt(123); }
 */
stmt_block: LEFTBRACE stmts RIGHTBRACE {
        $$ = std::make_shared<al::ast::StmtBlock>($2);
    }
stmts: { $$ = std::make_shared<al::ast::Stmts>(); }
    | stmt stmts { $$ = $2; $$->prependChild($1); }
stmt: exp SEMICOLON { $$ = $1; }

/**
 * exp
 * every exp returns a value
 */
exp: exp_call { $$ = $1; }
    | exp_op { $$ = $1; }
    | exp_assign { $$ = $1; }
    | exp_var_def { $$ = $1; }
    | exp_var_ref { $$ = $1; }
    | exp_member { $$ = $1; }
    | exp_lit { $$ = $1; }
    | exp_get_addr { $$ = $1; }
    | exp_deref { $$ = $1; }
    | LEFTPAR exp RIGHTPAR { $$ = $2; }
    | exp_volatile_cast { $$ = $1; }
    | exp_for { $$ = $1; }

exp_call: SYMBOL_LIT LEFTPAR exps RIGHTPAR {
      $$ = std::make_shared<al::ast::ExpCall>($1, $3->toVector());
    }
    | SYMBOL_LIT LEFTPAR RIGHTPAR {
      $$ = std::make_shared<al::ast::ExpCall>($1, std::vector<std::shared_ptr<al::ast::Exp>>());
    }
exp_op: exp PLUS exp {
      $$ = std::make_shared<al::ast::ExpCall>("+", std::vector<std::shared_ptr<al::ast::Exp>>({$1, $3}));
      }
    | exp LT exp {
      $$ = std::make_shared<al::ast::ExpCall>("<", std::vector<std::shared_ptr<al::ast::Exp>>({$1, $3}));
      }
exp_assign: exp EQ exp { $$ = std::make_shared<al::ast::ExpAssign>($1, $3); }

exp_var_ref: SYMBOL_LIT { $$ = std::make_shared<al::ast::ExpVarRef>($1); }
exp_var_def: var_decl EQ exp { $$ = std::make_shared<al::ast::ExpStackVarDef>($1, $3); }
exp_member: exp DOT SYMBOL_LIT { $$ = std::make_shared<al::ast::ExpMemberAccess>($1, $3); }
exp_lit: INT_LIT { $$ = $1; }
exp_get_addr: AND exp { $$ = std::make_shared<al::ast::ExpGetAddr>($2); }
exp_deref: STAR exp { $$ = std::make_shared<al::ast::ExpDeref>($2); }
exp_volatile_cast: VOLATILE LEFTPAR exp RIGHTPAR { $$ = std::make_shared<al::ast::ExpVolatileCast>($3); }

exp_for: FOR exp SEMICOLON exp SEMICOLON exp stmt_block {
      $$ = std::make_shared<al::ast::ExpFor>($2, $4, $6, $7);
    }
    | annotation FOR exp SEMICOLON exp SEMICOLON exp stmt_block {
      $$ = std::make_shared<al::ast::ExpFor>($3, $5, $7, $8, $1);
    }

exps: exp { $$ = std::make_shared<al::ast::ExpList>(); $$->prependChild($1); }
    | exp COMMA exps { $$ = $3; $$->prependChild($1); }

/**
 * Something like:
 * name: string
 */
var_decl: SYMBOL_LIT COLON type {
    $$ = std::make_shared<al::ast::VarDecl>($1, $3);
}

type: SYMBOL_LIT { $$ = std::make_shared<al::ast::Type>($1); }
    | STAR type { $$ = std::make_shared<al::ast::Type>($2, al::ast::Type::Ptr); }
    | PERSISTENT type { $$ = std::make_shared<al::ast::Type>($2, al::ast::Type::Persistent); }
    | FN LEFTPAR fn_args RIGHTPAR { $$ = std::make_shared<al::ast::Type>($3); }
    | FN LEFTPAR RIGHTPAR {
        $$ = std::make_shared<al::ast::Type>(std::make_shared<al::ast::VarDecls>());
      }

annotation: AT SYMBOL_LIT LEFTPAR exps RIGHTPAR {
        $$ = std::make_shared<al::ast::Annotation>($2, $4);
      }

%%
void al::Parser::error(const location &loc , const std::string &message) {
    cout << "Error: " << message << endl;
}
