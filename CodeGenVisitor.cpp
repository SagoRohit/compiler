#include "CodeGenVisitor.h"
#include <cctype>

CodeGenVisitor::CodeGenVisitor(const std::vector<std::string>& sourceLines)
    : sourceLines(sourceLines) {}

std::string CodeGenVisitor::typeOf(CSubsetParser::Type_specifierContext* ctx) {
    if (ctx->INT()) return "int";
    if (ctx->FLOAT()) return "float";
    return "void";
}

std::string CodeGenVisitor::newLabelBase() {
    return "L" + std::to_string(labelCounter++);
}

void CodeGenVisitor::emit(const std::string& instr) {
    funcBodyBuffer << "    " << instr << "\n";
}

void CodeGenVisitor::emitLabel(const std::string& label) {
    funcBodyBuffer << label << ":\n";
}

void CodeGenVisitor::emitComment(const std::string& text) {
    funcBodyBuffer << "    ; " << text << "\n";
}

void CodeGenVisitor::annotateLine(int line) {
    std::string src;
    if (line >= 1 && (size_t)line <= sourceLines.size()) {
        src = sourceLines[line - 1];
        size_t a = src.find_first_not_of(" \t");
        size_t b = src.find_last_not_of(" \t\r");
        src = (a == std::string::npos) ? "" : src.substr(a, b - a + 1);
    }
    funcBodyBuffer << "    ; Line " << line << ": " << src << "\n";
}

CGSymbol* CodeGenVisitor::resolveVariable(CSubsetParser::VariableContext* ctx) {
    if (ctx->expression() != nullptr) {
        // ID LTHIRD expression RTHIRD -- arrays are a Phase 2 feature.
        emitComment("[Phase 2] array indexing not implemented in Phase 1");
        return nullptr;
    }
    std::string name = ctx->ID()->getText();
    CGSymbol* sym = scope.lookup(name);
    if (!sym) emitComment("[warn] undeclared identifier '" + name + "'");
    return sym;
}

// ==================== var_declaration ====================
std::any CodeGenVisitor::visitVar_declaration(CSubsetParser::Var_declarationContext* ctx) {
    currentDeclType = typeOf(ctx->type_specifier());
    if (currentDeclType == "float") {
        emitComment("[Phase 2] float variables not required/implemented in Phase 1");
    }
    visit(ctx->declaration_list());
    return {};
}

// ==================== declaration_list ====================
std::any CodeGenVisitor::visitDeclaration_list(CSubsetParser::Declaration_listContext* ctx) {
    // if (ctx->declaration_list()) visit(ctx->declaration_list());
    if(ctx->declaration_list()) visit(ctx->declaration_list());
    std::string name = ctx->ID()->getText();
    // LTHIRD / ASSIGNOP only ever appear together with CONST_INT in mutually
    // exclusive alternatives (array-size vs. initializer), so check the
    // token that is unique to each alternative rather than CONST_INT itself.
    bool isArrayDecl = (ctx->LTHIRD() != nullptr);
    bool hasInit = (ctx->ASSIGNOP() != nullptr);

    if (insideFunction) {
        if (isArrayDecl) emitComment("[Phase 2] array declaration '" + name + "' treated as a scalar in Phase 1");
        int offset = currentLocalOffset;
        currentLocalOffset -= 4;
        scope.declareLocal(name, currentDeclType, offset);
        if (hasInit) {
            emit("MOV EAX, " + ctx->CONST_INT()->getText());
            emit("MOV " + scope.lookup(name)->operand + ", EAX");
        }
    } else {
        if (isArrayDecl) dataSeg << "    ; [Phase 2] array declaration '" << name << "' treated as a scalar in Phase 1\n";
        scope.declareGlobal(name, currentDeclType);
        std::string initVal = hasInit ? ctx->CONST_INT()->getText() : "0";
        dataSeg << "    var_" << name << " dd " << initVal << "\n";
    }
    return {};
}

// ==================== func_definition ====================
std::any CodeGenVisitor::visitFunc_definition(CSubsetParser::Func_definitionContext* ctx) {
    std::string returnType = typeOf(ctx->type_specifier());
    std::string funcName = ctx->ID()->getText();
    bool isMain = (funcName == "main");
    std::string label = isMain ? "main" : ("func_" + funcName);

    FuncInfo fi;
    fi.name = funcName;
    fi.label = label;
    fi.returnType = returnType;
    scope.functions[funcName] = fi;

    if (ctx->parameter_list() != nullptr) {
        // Function definitions with parameters are a Phase 2 feature.
        // Emit a harmless stub so the assembled program stays valid;
        // this function is never callable in Phase 1 anyway (no call syntax).
        codeSeg << label << ":\n"
                << "    ; [Phase 2] parameterized function '" << funcName
                << "' not implemented in Phase 1\n"
                << "    PUSH EBP\n"
                << "    MOV  EBP, ESP\n"
                << "    POP  EBP\n"
                << "    RET\n\n";
        return {};
    }

    bool savedInside = insideFunction;
    std::string savedLabel = currentFuncLabel;
    bool savedIsMain = currentIsMain;
    int savedOffset = currentLocalOffset;
    std::ostringstream savedBuffer;
    savedBuffer.swap(funcBodyBuffer);

    insideFunction = true;
    currentFuncLabel = label;
    currentIsMain = isMain;
    currentLocalOffset = -4;

    visit(ctx->compound_statement());

    int frameSize = (-currentLocalOffset) - 4; // total bytes reserved for locals

    codeSeg << label << ":\n"
            << "    PUSH EBP\n"
            << "    MOV  EBP, ESP\n"
            << "    SUB  ESP, " << frameSize << "\n"
            << funcBodyBuffer.str()
            << label << "_exit:\n"
            << "    ADD  ESP, " << frameSize << "\n"
            << "    POP  EBP\n";
    if (isMain) {
        codeSeg << "    MOV EAX, 1\n"
                << "    XOR EBX, EBX\n"
                << "    INT 0x80\n";
        sawMain = true;
    } else {
        codeSeg << "    RET\n"; // Phase 1 functions take no parameters
    }
    codeSeg << "\n";

    funcBodyBuffer.swap(savedBuffer);
    insideFunction = savedInside;
    currentFuncLabel = savedLabel;
    currentIsMain = savedIsMain;
    currentLocalOffset = savedOffset;
    return {};
}

// ==================== compound_statement ====================
std::any CodeGenVisitor::visitCompound_statement(CSubsetParser::Compound_statementContext* ctx) {
    scope.enter();
    if (ctx->statements()) visit(ctx->statements());
    scope.exit();
    return {};
}

// ==================== statement ====================
std::any CodeGenVisitor::visitStatement(CSubsetParser::StatementContext* ctx) {
    int line = ctx->getStart()->getLine();

    if (ctx->var_declaration()) {
        annotateLine(line);
        visit(ctx->var_declaration());
        return {};
    }
    if (!ctx->expression_statement().empty() && !ctx->FOR()) {
        annotateLine(line);
        auto* es = ctx->expression_statement(0);
        if (es->expression()) visit(es->expression());
        return {};
    }
    if (ctx->compound_statement()) {
        visit(ctx->compound_statement());
        return {};
    }
    if (ctx->PRINTLN()) {
        annotateLine(line);
        std::string id = ctx->ID()->getText();
        CGSymbol* sym = scope.lookup(id);
        if (sym) {
            emit("MOV EAX, " + sym->operand);
            emit("CALL OUTDEC");
        } else {
            emitComment("[warn] undeclared identifier '" + id + "' in println");
        }
        return {};
    }
    if (ctx->RETURN()) {
        annotateLine(line);
        visit(ctx->expression());
        emit("JMP " + (currentIsMain ? std::string("main_exit") : (currentFuncLabel + "_exit")));
        return {};
    }
    if (ctx->FOR() || ctx->IF() || ctx->WHILE()) {
        annotateLine(line);
        emitComment("[Phase 2] control-flow statement not implemented in Phase 1");
        return {};
    }

    if(ctx->GOTO()){
        annotateLine(line);
        std::string label = ctx->ID()->getText();
        emit("JMP label_" + label);
        return {};
    }
    if(ctx->COLON()){
        emitLabel("label_"+ ctx->ID()->getText());
        return {};
    }
    return {};
}

// ==================== expression ====================
std::any CodeGenVisitor::visitExpression(CSubsetParser::ExpressionContext* ctx) {
    if (ctx->ASSIGNOP()) {
        CGSymbol* sym = resolveVariable(ctx->variable());
        visit(ctx->expression());
        if (sym) emit("MOV " + sym->operand + ", EAX");
        return {};
    }
    if(ctx->COMPOUND_ASSIGNOP()){
        CGSymbol* sym = resolveVariable(ctx->variable());

        visit(ctx->logic_expression()); // visit right side
        emit("PUSH EAX"); // save right side value
        emit("MOV EAX," + (sym ? sym->operand: "0")); // left side eax save
        emit("POP EBX"); // now right side->EBX

        std::string op = ctx-> COMPOUND_ASSIGNOP()->getText();
        if(op == "+="){
            emit("ADD EAX, EBX");
        }else if(op == "-=")
        {
               emit("SUB EAX, EBX");
        }else if(op == "*="){
            emit("MUL EBX");
        }else if(op == "/="){
            emit("CDQ");
            emit("IDIV EBX");
        }else {
            emit("CDQ");
            emit("IDIV EBX");
            emit("MOV EAX, EDX");
        }
        if (sym) emit("MOV " + sym->operand + ", EAX");
        return {};
    }
    visit(ctx->logic_expression());
    return {};
}

// ==================== logic_expression ====================
std::any CodeGenVisitor::visitLogic_expression(CSubsetParser::Logic_expressionContext* ctx) {
    if (ctx->LOGICOP()) {
        emitComment("[Phase 2] logical && / || not implemented in Phase 1");
        emit("MOV EAX, 0");
        return {};
    }
    visit(ctx->rel_expression(0));
    return {};
}

// ==================== rel_expression ====================
void CodeGenVisitor::genCompare(CSubsetParser::Simple_expressionContext* left,
                                 CSubsetParser::Simple_expressionContext* right,
                                 const std::string& op) {
    visit(right);
    emit("PUSH EAX");
    visit(left);
    emit("POP EBX");
    emit("CMP EAX, EBX");

    std::string base = newLabelBase();
    std::string trueLabel = base + "_true";
    std::string endLabel = base + "_end";
    std::string jmp = (op == "<") ? "JL" : (op == ">") ? "JG" : (op == "<=") ? "JLE"
                     : (op == ">=") ? "JGE" : (op == "==") ? "JE" : "JNE";
    emit(jmp + " " + trueLabel);
    emit("MOV EAX, 0");
    emit("JMP " + endLabel);
    emitLabel(trueLabel);
    emit("MOV EAX, 1");
    emitLabel(endLabel);
}

std::any CodeGenVisitor::visitRel_expression(CSubsetParser::Rel_expressionContext* ctx) {
    if (ctx->RELOP()) {
        genCompare(ctx->simple_expression(0), ctx->simple_expression(1), ctx->RELOP()->getText());
        return {};
    }
    visit(ctx->simple_expression(0));
    return {};
}

// ==================== simple_expression ====================
std::any CodeGenVisitor::visitSimple_expression(CSubsetParser::Simple_expressionContext* ctx) {
    if (ctx->ADDOP()) {
        visit(ctx->term());
        emit("PUSH EAX");
        visit(ctx->simple_expression());
        emit("POP EBX");
        emit(ctx->ADDOP()->getText() == "+" ? "ADD EAX, EBX" : "SUB EAX, EBX");
        return {};
    }
    visit(ctx->term());
    return {};
}

// ==================== term ====================
std::any CodeGenVisitor::visitTerm(CSubsetParser::TermContext* ctx) {
    if (ctx->MULOP()) {
        visit(ctx->unary_expression());
        emit("PUSH EAX");
        visit(ctx->term());
        emit("POP EBX");
        std::string op = ctx->MULOP()->getText();
        if (op == "*") {
            emit("MUL EBX");
        } else if (op == "/") {
            emit("CDQ");
            emit("IDIV EBX");
        } else { // "%"
            emit("CDQ");
            emit("IDIV EBX");
            emit("MOV EAX, EDX");
        }
        return {};
    }
    visit(ctx->unary_expression());
    return {};
}

// ==================== unary_expression ====================
std::any CodeGenVisitor::visitUnary_expression(CSubsetParser::Unary_expressionContext* ctx) {
    if (ctx->NOT()) {
        visit(ctx->unary_expression());
        std::string base = newLabelBase();
        emit("TEST EAX, EAX");
        emit("JNE " + base + "_true");
        emit("MOV EAX, 1");
        emit("JMP " + base + "_end");
        emitLabel(base + "_true");
        emit("MOV EAX, 0");
        emitLabel(base + "_end");
        return {};
    }
    if (ctx->ADDOP()) {
        visit(ctx->unary_expression());
        if (ctx->ADDOP()->getText() == "-") emit("NEG EAX");
        return {};
    }
    visit(ctx->factor());
    return {};
}

// ==================== factor ====================
std::any CodeGenVisitor::visitFactor(CSubsetParser::FactorContext* ctx) {
    if (ctx->CONST_INT()) {
        emit("MOV EAX, " + ctx->CONST_INT()->getText());
        return {};
    }
    if (ctx->CONST_FLOAT()) {
        emitComment("[Phase 1] floating point not required - truncated to int");
        int v = (int) std::stod(ctx->CONST_FLOAT()->getText());
        emit("MOV EAX, " + std::to_string(v));
        return {};
    }
    if (ctx->LPAREN() && ctx->expression()) {
        visit(ctx->expression());
        return {};
    }
    if (ctx->ID() && ctx->LPAREN()) {
        emitComment("[Phase 2] function call '" + ctx->ID()->getText() + "(...)' not implemented in Phase 1");
        emit("MOV EAX, 0");
        return {};
    }
    if (ctx->INCOP() || ctx->DECOP()) {
        CGSymbol* sym = resolveVariable(ctx->variable());
        if (sym) {
            emit(std::string("PUSH dword ") + sym->operand);
            emit((ctx->INCOP() ? std::string("INC dword ") : std::string("DEC dword ")) + sym->operand);
            emit("POP EAX");
        } else {
            emit("MOV EAX, 0");
        }
        return {};
    }
    // factor : variable
    CGSymbol* sym = resolveVariable(ctx->variable());
    emit("MOV EAX, " + (sym ? sym->operand : "0"));
    return {};
}
