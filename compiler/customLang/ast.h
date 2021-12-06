#include <stdlib.h>

enum token
{
    t_var,
    t_fun,
    t_name,
    t_literal,
    t_unOp,
    t_binOp,
    t_assign,
    t_semicolon,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly,
    t_EOF
};

struct astNode
{
    char *value;
    enum token type;
    struct astNode *child;
    struct astNode *sibling;
};

struct astNode *newastNode(enum token t, char *newdata);
void astNode_insertSibling(struct astNode *it, struct astNode *newSibling);
void astNode_insertChild(struct astNode *it, struct astNode *newChild);
void printAST(struct astNode *it, int depth);