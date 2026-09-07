#pragma once

#include "CSubsetBaseVisitor.h"
#include "CGSymbol.h"
#include <sstream>
#include <string>
#include <vector>

// Phase 1 Intermediate Code Generator.
//
// Supported (Phase 1 grammar subset, see P1_Grammar.pdf):
//   - global & local (non-array) int/void declarations, optionally
//     initialized with a constant int literal (ICG_A1: "int a=10, b, c;")
//   - zero-argument function definitions (int/void return)
//   - arithmetic (+ - * / %), unary (+ - !), relational (< <= > >= == !=)
//   - assignment, postfix ++/--, println(ID), return expr;
//
// Deferred to Phase 2 (stubbed with a comment so the tool never crashes,
// but the emitted code for these constructs is not meant to be correct):
//   - if / if-else / while / for
//   - logical && / ||
//   - arrays, function parameters, function calls
//
// Every expression-generating visit leaves its result in EAX on exit,
// following the pattern demonstrated in fasm/tutorial.pdf and
// fasm/codes/*.asm (evaluate right side, PUSH, evaluate left side into
// EAX, POP right into EBX, operate EAX/EBX).

class CodeGenVisitor : public CSubsetBaseVisitor {
public:
    CodeGenVisitor(const std::vector<std::string>& sourceLines);

    std::ostringstream dataSeg;   // global variable storage
    std::ostringstream codeSeg;   // finished functions, one after another
    bool sawMain = false;

    std::any visitVar_declaration(CSubsetParser::Var_declarationContext* ctx) override;
    std::any visitDeclaration_list(CSubsetParser::Declaration_listContext* ctx) override;
    std::any visitFunc_definition(CSubsetParser::Func_definitionContext* ctx) override;
    std::any visitCompound_statement(CSubsetParser::Compound_statementContext* ctx) override;
    std::any visitStatement(CSubsetParser::StatementContext* ctx) override;
    std::any visitExpression(CSubsetParser::ExpressionContext* ctx) override;
    std::any visitLogic_expression(CSubsetParser::Logic_expressionContext* ctx) override;
    std::any visitRel_expression(CSubsetParser::Rel_expressionContext* ctx) override;
    std::any visitSimple_expression(CSubsetParser::Simple_expressionContext* ctx) override;
    std::any visitTerm(CSubsetParser::TermContext* ctx) override;
    std::any visitUnary_expression(CSubsetParser::Unary_expressionContext* ctx) override;
    std::any visitFactor(CSubsetParser::FactorContext* ctx) override;

private:
    const std::vector<std::string>& sourceLines;
    CGScope scope;

    bool insideFunction = false;
    std::string currentDeclType;
    std::string currentFuncLabel;
    bool currentIsMain = false;
    int currentLocalOffset = -4;
    int labelCounter = 0;
    std::ostringstream funcBodyBuffer; // one buffer per function; the "temporary" storage
                                        // for on-the-fly generation before the frame size
                                        // (SUB ESP, N) is known.

    static std::string typeOf(CSubsetParser::Type_specifierContext* ctx);
    std::string newLabelBase();
    void emit(const std::string& instr);
    void emitLabel(const std::string& label);
    void emitComment(const std::string& text);
    void annotateLine(int line);
    CGSymbol* resolveVariable(CSubsetParser::VariableContext* ctx);
    void genCompare(CSubsetParser::Simple_expressionContext* left,
                     CSubsetParser::Simple_expressionContext* right,
                     const std::string& op);
    void printArguments(CSubsetParser::ArgumentsContext* ctx);
};
