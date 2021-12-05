#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"

char lookahead_dumb();
void trimWhitespace();
char lookahead();
enum token scan();
struct astNode *match(enum token t);
void consume(enum token t);

struct astNode *parseProgram(char* inFileName);
struct astNode *parseTLDList();
struct astNode *parseTLD();
struct astNode *parseVariableInit();
struct astNode *parseAssignment();
struct astNode *parseStatementList();
struct astNode *parseStatement();
struct astNode *parseExpression();