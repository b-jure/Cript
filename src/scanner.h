#ifndef __SKOOMA_SCANNER_H__
#define __SKOOMA_SCANNER_H__

#include "common.h"

typedef enum {
  // Single character tokens.
  TOK_LPAREN = 0,
  TOK_RPAREN,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_DOT,
  TOK_COMMA,
  TOK_MINUS,
  TOK_PLUS,
  TOK_COLON,
  TOK_SEMICOLON,
  TOK_SLASH,
  TOK_STAR,
  TOK_QMARK,
  // One or two character tokens.
  TOK_BANG,
  TOK_BANG_EQUAL,
  TOK_EQUAL,
  TOK_EQUAL_EQUAL,
  TOK_GREATER,
  TOK_GREATER_EQUAL,
  TOK_LESS,
  TOK_LESS_EQUAL,
  // Literals.
  TOK_IDENTIFIER,
  TOK_STRING,
  TOK_NUMBER,
  // Keywords.
  TOK_AND,
  TOK_BREAK,
  TOK_CASE,
  TOK_CONTINUE,
  TOK_CLASS,
  TOK_DEFAULT,
  TOK_ELSE,
  TOK_FALSE,
  TOK_FOR,
  TOK_FN,
  TOK_IF,
  TOK_IMPL,
  TOK_NIL,
  TOK_OR,
  TOK_PRINT,
  TOK_RETURN,
  TOK_SUPER,
  TOK_SELF,
  TOK_SWITCH,
  TOK_TRUE,
  TOK_VAR,
  TOK_WHILE,
  TOK_FIXED,

  TOK_ERROR,
  TOK_EOF
} TokenType;
// @TODO: Implement TOK_BREAK (track last loop start, otherwise error)

typedef struct {
  TokenType type;
  const char *start;
  UInt len;
  UInt line;
} Token;

typedef struct {
  const char *start;
  const char *current;
  UInt line;
} Scanner;

Scanner Scanner_new(const char *source);
Token Scanner_scan(Scanner *scanner);

#endif
