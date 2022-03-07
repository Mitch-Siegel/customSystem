#include "linearizer.h"

/*
 * These functions walk the AST and convert it to three-address code
 */

struct TACLine *linearizeASMBlock(struct astNode *it)
{
    struct TACLine *asmBlock = NULL;
    struct astNode *asmRunner = it->child;
    while (asmRunner != NULL)
    {
        struct TACLine *asmLine = newTACLine();
        asmLine->operation = tt_asm;
        asmLine->operands[0] = asmRunner->value;
        asmBlock = appendTAC(asmBlock, asmLine);

        asmRunner = asmRunner->sibling;
    }
    return asmBlock;
}

struct TACLine *linearizeDereference(struct astNode *it, int *tempNum, struct tempList *tl)
{
    printf("Call to linearize dereference:\n");
    printAST(it, 0);
    printf("\n");
    struct TACLine *thisDereference = newTACLine();
    struct TACLine *returnLine = thisDereference;

    thisDereference->operands[0] = getTempString(tl, *tempNum);
    thisDereference->operandTypes[0] = vt_temp;
    thisDereference->operation = tt_memr_1;
    (*tempNum)++;

    switch (it->type)
    {
    case t_name:
        thisDereference->operands[1] = it->value;
        thisDereference->operandTypes[1] = vt_var;
        break;

    case t_dereference:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        returnLine = prependTAC(returnLine, linearizeDereference(it->child, tempNum, tl));
        break;

    case t_unOp:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        returnLine = prependTAC(returnLine, linearizePointerArithmetic(it, tempNum, tl, 0));
        break;

    default:
        printf("Malformed parse tree when linearizing dereference!\n");
        exit(1);
    }

    printf("here's what we got:\n");
    printTACBlock(returnLine, 0);

    return returnLine;
}

/*
 * TODO - proper type comparisons and size multiplications
 */

struct TACLine *linearizePointerArithmetic(struct astNode *it, int *tempNum, struct tempList *tl, int depth)
{
    printf("Linearize pointer arithmetic for tree:\n");
    printAST(it, 0);
    printf("\n");

    struct TACLine *thisOperation = newTACLine();
    struct TACLine *returnOperation = thisOperation;

    thisOperation->operands[0] = getTempString(tl, *tempNum);
    (*tempNum)++;

    switch (it->type)
    {
        // assign the correct operation based on the operator node
    case t_unOp:
        switch (it->value[0])
        {
        case '+':
            thisOperation->reorderable = 1;
            thisOperation->operation = tt_add;
            break;

        case '-':
            thisOperation->operation = tt_subtract;
            break;
        }

        // see what the LHS of the tree is
        switch (it->child->type)
        {
        // recursively handle more operations
        case t_unOp:
            thisOperation->operands[1] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[1] = vt_temp;
            // TODO / BUG - recurse with depth of 0 to handle scaling within parens?
            // doing this breaks when more dereferences are involved
            returnOperation = prependTAC(thisOperation, linearizePointerArithmetic(it->child, tempNum, tl, depth));
            break;

        case t_dereference:
            thisOperation->operands[1] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[1] = vt_temp;
            returnOperation = prependTAC(thisOperation, linearizeDereference(it->child->child, tempNum, tl));
            break;

        case t_name:
            thisOperation->operands[1] = it->child->value;
            thisOperation->operandTypes[1] = vt_var;
            break;

        case t_literal:
            printf("Error - literal in LHS of pointer arithmetic expression!\n");
            exit(1);

        default:
            printf("Error - unexpected LHS of pointer arithmetic!\n");
            printAST(it, 0);
            exit(1);

            break;
        }

        switch (it->child->sibling->type)
        {
        case t_unOp:
            if (depth == 0)
            {
                struct TACLine *scaleMultiplication = newTACLine();
                scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
                thisOperation->operands[2] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[0] = vt_temp;
                (*tempNum)++;

                scaleMultiplication->operands[1] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[1] = vt_var;

                // TODO - scale multiplication based on object size (when implementing types)
                scaleMultiplication->operands[2] = "2";
                scaleMultiplication->operation = tt_mul;

                returnOperation = prependTAC(returnOperation, scaleMultiplication);

                returnOperation = prependTAC(returnOperation, linearizePointerArithmetic(it->child->sibling, tempNum, tl, depth + 1));
            }
            else
            {
                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;
                returnOperation = prependTAC(returnOperation, linearizePointerArithmetic(it->child->sibling, tempNum, tl, depth + 1));
            }
            break;

        case t_dereference:
            thisOperation->operands[2] = getTempString(tl, *tempNum);
            (*tempNum)++;
            thisOperation->operandTypes[2] = vt_temp;
            returnOperation = prependTAC(thisOperation, linearizeDereference(it->child->sibling->child, tempNum, tl));
            break;

        case t_name:
            // only perform a scaling multiplication if at the root of the tree
            if (depth == 0)
            {
                struct TACLine *scaleMultiplication = newTACLine();
                scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
                thisOperation->operands[2] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[0] = vt_temp;

                scaleMultiplication->operands[1] = it->child->value;
                scaleMultiplication->operandTypes[1] = vt_var;

                // TODO - scale multiplication based on object size (when implementing types)
                scaleMultiplication->operands[2] = "2";
                scaleMultiplication->operation = tt_mul;
                (*tempNum)++;

                returnOperation = prependTAC(returnOperation, scaleMultiplication);
            }
            else
            {
                thisOperation->operands[2] = it->child->sibling->value;
                thisOperation->operandTypes[2] = vt_var;
            }
            break;

        case t_literal:
            // only scale the literal if at the root of the tree
            if (depth == 0)
            {
                // TODO: use the dict to store these multiplied literals
                // this quick and dirty method will leak memory!
                char *scaledLiteral = malloc(8 * sizeof(char));
                // TODO - scale multiplication based on object size (when implementing types)
                sprintf(scaledLiteral, "%d", 2 * atoi(it->child->sibling->value));
                thisOperation->operands[2] = scaledLiteral;
            }
            else
            {
                thisOperation->operands[2] = it->child->sibling->value;
            }

            break;

        default:
            printf("Bad RHS of pointer assignment expression:\n");
            printAST(it, 0);
            exit(1);
        }
        break;

    case t_dereference:
        freeTAC(thisOperation);
        returnOperation = linearizeDereference(it->child, tempNum, tl);
        break;

    default:
        printf("malformed parse tree or bad call to linearize pointer arithmetic!\n");
        exit(1);
        break;
    }
    printf("here's what we got:\n");
    printTACBlock(returnOperation, 0);
    return returnOperation;
}

struct TACLine *linearizePointerWrite(struct astNode *it, int *tempNum, struct tempList *tl)
{
    printf("call to linearize pointer write\n");
    printAST(it, 0);
    struct TACLine *writeOperation = newTACLine();
    struct TACLine *returnLine = writeOperation;

    switch (it->type)
    {
    case t_literal:
    case t_name:
        writeOperation->operands[1] = getTempString(tl, *tempNum);
        (*tempNum)++;
        writeOperation->operation = tt_memw_1;
        writeOperation->operands[0] = it->value;
        break;

    default:
        writeOperation->operands[0] = getTempString(tl, *tempNum);
        returnLine = prependTAC(writeOperation, linearizePointerArithmetic(it->child, tempNum, tl, 0));
        break;
    }

    printf("done linearizing pointer write: here's what we got\n");
    printTACBlock(returnLine, 0);

    return returnLine;
}

struct TACLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct astNode *runner = it->child->child;
    struct TACLine *calltac = newTACLine();
    calltac->operands[0] = getTempString(tl, *tempNum);
    calltac->operandTypes[0] = vt_temp;

    (*tempNum)++;
    calltac->operands[1] = it->child->value;
    calltac->operandTypes[1] = vt_returnval;
    calltac->operation = tt_call;

    while (runner != NULL)
    {
        struct TACLine *thisArgument = NULL;
        switch (runner->type)
        {
        case t_name:
            thisArgument = newTACLine();
            thisArgument->operandTypes[0] = vt_var;
        case t_literal:
            if (thisArgument == NULL)
            {
                thisArgument = newTACLine();
                thisArgument->operandTypes[0] = vt_literal;
            }
            thisArgument->operands[0] = runner->value;
            thisArgument->operation = tt_push;
            break;

        default:

            struct TACLine *pushOperation = newTACLine();
            pushOperation->operands[0] = getTempString(tl, *tempNum);
            pushOperation->operandTypes[0] = vt_temp;
            pushOperation->operation = tt_push;

            thisArgument = linearizeExpression(runner, tempNum, tl);
            appendTAC(thisArgument, pushOperation);
        }

        calltac = prependTAC(calltac, thisArgument);
        runner = runner->sibling;
    }

    return calltac;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
struct TACLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl)
{
    printf("Linearizing expression:\n");
    printAST(it, 0);
    struct TACLine *thisExpression = newTACLine();
    struct TACLine *returnExpression = NULL;

    // since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
    if (it->type != t_compOp)
    {
        thisExpression->operands[0] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[0] = vt_temp;
        // increment count of temp variables, the parse of this expression will be written to a temp
        (*tempNum)++;
    }
    // support dereference and reference operations separately
    // since these have only one operand
    if (it->type == t_dereference)
    {
        thisExpression->operation = tt_memr_1;
        if (it->child->type == t_name)
        {
            thisExpression->operands[1] = it->child->value;
        }
        else
        {
            thisExpression->operands[1] = getTempString(tl, *tempNum);
            returnExpression = prependTAC(thisExpression, linearizePointerArithmetic(it->child, tempNum, tl, 0));
        }
        return returnExpression;
    }
    if (it->type == t_reference)
    {
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operation = tt_reference;
        return prependTAC(thisExpression, linearizePointerArithmetic(it->child, tempNum, tl, 0));
    }

    switch (it->child->type)
    {
    case t_call:
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeFunctionCall(it->child, tempNum, tl));
        break;

    case t_compOp:
    case t_unOp:
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, tl));
        break;

    case t_name:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_var;
        break;

    case t_literal:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_literal;
        break;

    case t_dereference:
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[1] = vt_temp;
        returnExpression = prependTAC(thisExpression, linearizeDereference(it->child->child, tempNum, tl));
        break;

    default:
        printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
        exit(1);
    }

    switch (it->value[0])
    {
    case '+':
        thisExpression->reorderable = 1;
        thisExpression->operation = tt_add;
        break;

    case '-':
        thisExpression->operation = tt_subtract;
        break;

    case '>':
    case '<':
    case '!':
    case '=':
        thisExpression->operation = tt_cmp;
        break;
    }

    switch (it->child->sibling->type)
    {
    case t_call:
        thisExpression->operandTypes[2] = vt_temp;
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child->sibling, tempNum, tl));
        break;

    case t_compOp:
    case t_unOp:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;
        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACLine to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        break;

    case t_name:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_var;
        break;

    case t_literal:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_literal;
        break;

    case t_dereference:
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeDereference(it->child->sibling, tempNum, tl));
        else
            returnExpression = prependTAC(thisExpression, linearizeDereference(it->child->sibling, tempNum, tl));
        break;

    default:
        printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
        exit(1);
    }

    // if we have prepended, make sure to return the first line of TAC
    // otherwise there is only one line of TAC to return, so return that
    if (returnExpression == NULL)
        returnExpression = thisExpression;

    printf("Done linearizing expresion - here's what we got:\n");
    printTACBlock(returnExpression, 0);
    return returnExpression;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
struct TACLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct TACLine *assignment;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newTACLine();
        assignment->operands[1] = it->child->sibling->value;

        switch (it->child->sibling->type)
        {
        case t_literal:
            assignment->operandTypes[1] = vt_literal;
            break;

        case t_name:
            assignment->operandTypes[1] = vt_var;
            break;

        default:
            printf("Error parsing simple assignment - unexpected type on RHS\n");
            exit(1);
        }
    }

    // otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
    else
    {
        switch (it->child->sibling->type)
        {
        case t_dereference:
            assignment = linearizeDereference(it->child->sibling->child, tempNum, tl);
            break;

        case t_unOp:
            assignment = linearizeExpression(it->child->sibling, tempNum, tl);
            break;

        case t_call:
            assignment = linearizeFunctionCall(it->child->sibling, tempNum, tl);
            break;

        default:
            printf("Error linearizing expression - malformed parse tree (expected unOp or call)\n");
            exit(1);
        }
    }
    printf("DONE linearizing RHS of assignment:\n");
    printTACBlock(assignment, 0);
    printf("\n");
    struct TACLine *lastLine = findLastTAC(assignment);
    printf("\nlast line is:");
    printTACLine(lastLine);
    if (it->child->type == t_dereference)
    {
        /*(*tempNum)++;
        lastLine->operands[0] = getTempString(tl, *tempNum);
        lastLine->operandTypes[0] = vt_temp;*/

        printf("need to linearize LHS\n");
        printAST(it->child, 0);
        lastLine->operands[0] = getTempString(tl, *tempNum);
        struct TACLine *LHS = linearizePointerWrite(it->child->child, tempNum, tl);
        assignment = appendTAC(assignment, LHS);
        printf("linearized LHS - here's what we got\n");

        printTACBlock(LHS, 0);
    }
    else
    {
        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }

    return assignment;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct TACLine *linearizeStatementList(struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl)
{
    // scrape along the function
    struct astNode *runner = it;
    struct TACLine *sltac = NULL;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_asm:
            sltac = appendTAC(sltac, linearizeASMBlock(runner));
            break;

        // if we see a variable being declared and then assigned
        // generate the code and stick it on to the end of the block
        case t_var:
            if (runner->child->type == t_assign)
                sltac = appendTAC(sltac, linearizeAssignment(runner->child, tempNum, tl));

            break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            sltac = appendTAC(sltac, linearizeAssignment(runner, tempNum, tl));
            break;

        case t_call:
            // for a raw call, return value is not used, null out that operand
            struct TACLine *callTAC = linearizeFunctionCall(runner, tempNum, tl);
            findLastTAC(callTAC)->operands[0] = NULL;
            sltac = appendTAC(sltac, callTAC);
            break;

        case t_return:
            struct TACLine *returnAssignment = linearizeAssignment(runner->child, tempNum, tl);
            // force the ".RETVAL" variable to a temp type since we don't care about its lifespan
            findLastTAC(returnAssignment)->operandTypes[0] = vt_temp;
            sltac = appendTAC(sltac, returnAssignment);

            struct TACLine *returnTac = newTACLine();
            returnTac->operands[0] = findLastTAC(sltac)->operands[0];
            returnTac->operation = tt_return;
            sltac = appendTAC(sltac, returnTac);
            break;

        case t_if:
        {
            // linearize the expression we will test
            sltac = appendTAC(sltac, linearizeExpression(runner->child, tempNum, tl));

            struct TACLine *pushState = newTACLine();
            pushState->operation = tt_pushstate;
            appendTAC(sltac, pushState);

            struct TACLine *noifJump = newTACLine();

            // generate a label and figure out condition to jump when the if statement isn't executed
            char *cmpOp = runner->child->value;
            switch (cmpOp[0])
            {
            case '<':
                switch (cmpOp[1])
                {
                case '=':
                    noifJump->operation = tt_jg;
                    break;

                default:
                    noifJump->operation = tt_jge;
                    break;
                }
                break;

            case '>':
                switch (cmpOp[1])
                {
                case '=':
                    noifJump->operation = tt_jl;
                    break;

                default:
                    noifJump->operation = tt_jle;
                    break;
                }
                break;

            case '!':
                noifJump->operation = tt_je;
                break;

            case '=':
                noifJump->operation = tt_jne;
                break;
            }
            struct TACLine *noifLabel = newTACLine();
            noifLabel->operation = tt_label;
            appendTAC(sltac, noifJump);

            // bit hax (⌐▨_▨)
            // (store the label index using the char* for this TAC line)
            // (avoids having to use more fields in the struct)
            noifLabel->operands[0] = (char *)((long int)++(*labelCount));
            noifJump->operands[0] = (char *)(long int)(*labelCount);
            // printf("\n\n~~~\n");

            struct TACLine *ifBody = linearizeStatementList(runner->child->sibling->child, tempNum, labelCount, tl);

            appendTAC(sltac, ifBody);

            // if the if block ends in a return, no need to restore the state
            // since execution will jump directly to the end of the funciton from here
            if (findLastTAC(ifBody)->operation != tt_return)
            {
                struct TACLine *endIfRestore = newTACLine();
                endIfRestore->operation = tt_restorestate;
                appendTAC(sltac, endIfRestore);
            }

            // if there's an else to fall through to
            if (runner->child->sibling->sibling != NULL)
            {
                struct TACLine *ifBlockFinalTAC = findLastTAC(ifBody);

                // don't need this jump and label when the if statement ends in a return
                struct TACLine *endIfLabel;
                if (ifBlockFinalTAC->operation != tt_return)
                {
                    // generate a label for the absolute end of the if/else block
                    endIfLabel = newTACLine();
                    endIfLabel->operation = tt_label;
                    endIfLabel->operands[0] = (char *)((long int)++(*labelCount));

                    // jump to the end after the if part is done
                    struct TACLine *ifDoneJump = newTACLine();
                    ifDoneJump->operation = tt_jmp;
                    ifDoneJump->operands[0] = endIfLabel->operands[0];
                    appendTAC(sltac, ifDoneJump);
                }
                // make sure we are in a known state before the else statement
                struct TACLine *beforeElseRestore = newTACLine();
                beforeElseRestore->operation = tt_resetstate;
                appendTAC(noifLabel, beforeElseRestore);

                struct TACLine *elseBody = linearizeStatementList(runner->child->sibling->sibling->child->child, tempNum, labelCount, tl);

                // if the if won't be executed, so execution from the noif label goes to the else block
                appendTAC(noifLabel, elseBody);
                appendTAC(sltac, noifLabel);

                // only need to restore the state if execution will continue after the if/else
                if (ifBlockFinalTAC->operation != tt_return)
                {
                    struct TACLine *endElseRestore = newTACLine();
                    endElseRestore->operation = tt_restorestate;
                    appendTAC(sltac, endElseRestore);
                    appendTAC(sltac, endIfLabel);
                }
            }
            else
            {
                appendTAC(sltac, noifLabel);
            }

            // throw away the saved register states
            struct TACLine *popStateLine = newTACLine();
            popStateLine->operation = tt_popstate;
            appendTAC(sltac, popStateLine);
        }
        break;

        case t_while:
        {
            // push state before entering the loop
            struct TACLine *pushState = newTACLine();
            pushState->operation = tt_pushstate;
            sltac = appendTAC(sltac, pushState);

            // establish a block during which any live variable's lifetime will be extended
            // since a variable could "die" during the while loop, then the loop could repeat
            // we need to extend all the lifetimes within the loop or risk overwriting and losing some variables
            struct TACLine *doLine = newTACLine();
            doLine->operation = tt_do;
            appendTAC(sltac, doLine);

            // generate a label for the top of the while loop
            struct TACLine *beginWhile = newTACLine();
            beginWhile->operation = tt_label;
            beginWhile->operands[0] = (char *)((long int)++(*labelCount));
            appendTAC(sltac, beginWhile);

            // linearize the expression we will test
            appendTAC(sltac, linearizeExpression(runner->child, tempNum, tl));

            // generate a label and figure out condition to jump when the while loop isn't executed
            struct TACLine *noWhileJump = newTACLine();
            char *cmpOp = runner->child->value;
            switch (cmpOp[0])
            {
            case '<':
                switch (cmpOp[1])
                {
                case '=':
                    noWhileJump->operation = tt_jg;
                    break;

                default:
                    noWhileJump->operation = tt_jge;
                    break;
                }
                break;

            case '>':
                switch (cmpOp[1])
                {
                case '=':
                    noWhileJump->operation = tt_jl;
                    break;

                default:
                    noWhileJump->operation = tt_jle;
                    break;
                }
                break;

            case '!':
                noWhileJump->operation = tt_je;
                break;

            case '=':
                noWhileJump->operation = tt_jne;
                break;
            }
            appendTAC(sltac, noWhileJump);

            struct TACLine *noWhileLabel = newTACLine();
            noWhileLabel->operation = tt_label;
            // bit hax (⌐▨_▨)
            // (store the label index using the char* for this TAC line)
            // (avoids having to use more fields in the struct)
            noWhileLabel->operands[0] = (char *)((long int)++(*labelCount));
            noWhileJump->operands[0] = (char *)(long int)(*labelCount);

            // linearize the body of the loop
            struct TACLine *whileBody = linearizeStatementList(runner->child->sibling->child, tempNum, labelCount, tl);
            appendTAC(sltac, whileBody);

            // restore the registers to a known state at the end of every loop
            struct TACLine *restoreState = newTACLine();
            restoreState->operation = tt_restorestate;
            appendTAC(sltac, restoreState);

            // jump back to the condition test
            struct TACLine *loopJump = newTACLine();
            loopJump->operation = tt_jmp;
            loopJump->operands[0] = beginWhile->operands[0];
            appendTAC(sltac, loopJump);

            // end the period over which lifetimes will be extended
            struct TACLine *endDoLine = newTACLine();
            endDoLine->operation = tt_enddo;
            appendTAC(sltac, endDoLine);

            // at the very end, add the label we jump to if the condition test fails
            appendTAC(sltac, noWhileLabel);
            // throw away the saved state when done with the loop
            struct TACLine *endWhilePop = newTACLine();
            endWhilePop->operation = tt_popstate;
            appendTAC(sltac, endWhilePop);
        }
        break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }
    return sltac;
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct astNode *it, struct symbolTable *table)
{
    // scrape along the top level of the AST
    struct astNode *runner = it;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {
        // if we encounter a function, lookup its symbol table entry
        // then generate the TAC for it and add a reference to the start of the generated code to the function entry
        case t_fun:
        {
            int tempNum = 0; // track the number of temporary variables used
            int labelCount = 0;
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct TACLine *generatedTAC = newTACLine();

            generatedTAC->operation = tt_label;
            generatedTAC->operands[0] = 0;
            appendTAC(generatedTAC, linearizeStatementList(runner->child->sibling, &tempNum, &labelCount, table->tl));
            theFunction->table->codeBlock = generatedTAC;
            break;
        }
        break;

        case t_asm:
            table->codeBlock = appendTAC(table->codeBlock, linearizeASMBlock(runner));
            break;

        // ignore everything else (for now) - no global vars, etc...
        default:
            break;
        }
        runner = runner->sibling;
    }
}
