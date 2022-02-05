#include "tac.h"

char *getAsmOp(enum TACType t)
{
    switch (t)
    {
    case tt_assign:
        return "";

    case tt_add:
        return "add";

    case tt_cmp:
        return "cmp";

    case tt_subtract:
        return "sub";

    case tt_push:
        return "push";

    case tt_call:
        return "call";

    case tt_label:
        return ".";

    case tt_return:
        return "ret";

    case tt_jg:
        return "jg";

    case tt_jge:
        return "jge";

    case tt_jl:
        return "jl";

    case tt_jle:
        return "jle";

    case tt_je:
        return "je";

    case tt_jne:
        return "jne";

    case tt_jmp:
        return "jmp";

    case tt_pushstate:
        return "PUSHSTATE";

    case tt_restorestate:
        return "RESTORESTATE";

    case tt_resetstate:
        return "RESETSTATE";

    case tt_popstate:
        return "POPSTATE";
    }
    return "";
}

struct TACLine *newTACLine()
{
    struct TACLine *wip = malloc(sizeof(struct TACLine));
    wip->nextLine = NULL;
    wip->prevLine = NULL;
    wip->operands[0] = NULL;
    wip->operands[1] = NULL;
    wip->operands[2] = NULL;
    wip->operandTypes[0] = vt_null;
    wip->operandTypes[1] = vt_null;
    wip->operandTypes[2] = vt_null;
    // default type of a line of TAC is assignment
    wip->operation = tt_assign;
    // by default operands are NOT reorderable
    wip->reorderable = 0;
    return wip;
}

void printTACLine(struct TACLine *it)
{
    char *operationStr;
    char fallingThrough = 0;
    switch (it->operation)
    {
    case tt_add:
        if (!fallingThrough)
            operationStr = "+";
        fallingThrough = 1;
    case tt_subtract:
        if (!fallingThrough)
            operationStr = "-";

        printf("%s = %s %s %s", it->operands[0], it->operands[1], operationStr, it->operands[2]);
        break;

    case tt_jg:
    case tt_jge:
    case tt_jl:
    case tt_jle:
    case tt_je:
    case tt_jne:
    case tt_jmp:
        printf("%s label %ld", getAsmOp(it->operation), (long int)it->operands[0]);
        break;

    case tt_cmp:
        printf("cmp %s %s", it->operands[1], it->operands[2]);
        break;

    case tt_assign:
        printf("%s = %s", it->operands[0], it->operands[1]);
        break;

    case tt_push:
        printf("push %s", it->operands[0]);
        break;

    case tt_call:
        printf("%s = call %s", it->operands[0], it->operands[1]);
        break;

    case tt_label:
        printf("~label %ld:", (long int)it->operands[0]);
        break;

    case tt_return:
        printf("ret %s", it->operands[0]);
        break;

    case tt_pushstate:
        printf("PUSHSTATE");
        break;

    case tt_restorestate:
        printf("RESTORESTATE");
        break;

    case tt_resetstate:
        printf("RESETSTATE");
        break;

    case tt_popstate:
        printf("POPSTATE");
        break;
    }
    printf("%s\n", it->reorderable ? " - Reorderable" : "");
    // printf("\t%d %d %d\n", it->operandTypes[0], it->operandTypes[1], it->operandTypes[2]);
}

void printTACBlock(struct TACLine *it)
{
    int lineIndex = 0;
    while (it != NULL)
    {
        if (it->operation != tt_label)
        {
            printf("\t%2x:", lineIndex);
        }

        printTACLine(it);
        lineIndex++;
        it = it->nextLine;
    }
}

// stick the "after" block at the end of the "before" block, returning the head of the full block
struct TACLine *appendTAC(struct TACLine *before, struct TACLine *after)
{
    if (before == NULL)
        return after;

    struct TACLine *runner = before;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = after;
    after->prevLine = runner;

    return before;
}

// stick the "before" block in front of the "after" block, returning the head of the full block
struct TACLine *prependTAC(struct TACLine *after, struct TACLine *before)
{
    struct TACLine *runner = before;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = after;
    return before;
}

struct TACLine *findLastTAC(struct TACLine *head)
{
    while (head->nextLine != NULL)
        head = head->nextLine;

    return head;
}

void freeTAC(struct TACLine *it)
{
    while (it != NULL)
    {
        struct TACLine *old = it;

        it = it->nextLine;
        free(old);
    }
}
