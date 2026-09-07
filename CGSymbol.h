#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// Lightweight symbol/scope bookkeeping used only for code generation
// (offsets and asm operand text). Semantic checking is assumed to have
// already been done in the syntax/semantic-analysis phase (Assignment 3);
// the ICG phase assumes the input program is error-free.

struct ParamInfo {
    std::string type;
    std::string name;
};

struct CGSymbol {
    std::string name;
    std::string dataType;   // "int", "float", "void"
    std::string operand;    // e.g. "[var_x]" for a global, "[EBP-4]" for a local/param
    bool isArray = false;
};

struct FuncInfo {
    std::string name;
    std::string label;      // asm label: "main" for main(), "func_<name>" otherwise
    std::string returnType;
    std::vector<ParamInfo> params;
};

class CGScope {
public:
    std::vector<std::unordered_map<std::string, CGSymbol>> scopes;
    std::unordered_map<std::string, FuncInfo> functions;

    CGScope() { scopes.emplace_back(); } // scopes[0] = global scope

    void enter() { scopes.emplace_back(); }
    void exit() { scopes.pop_back(); }

    void declareGlobal(const std::string& name, const std::string& type) {
        CGSymbol s;
        s.name = name;
        s.dataType = type;
        s.operand = "[var_" + name + "]";
        scopes.front()[name] = s;
    }

    void declareLocal(const std::string& name, const std::string& type, int offset) {
        std::string op = "[EBP" + (offset < 0
                             ? (std::string("-") + std::to_string(-offset))
                             : (std::string("+") + std::to_string(offset))) + "]";
        CGSymbol s;
        s.name = name;
        s.dataType = type;
        s.operand = op;
        scopes.back()[name] = s;
    }

    CGSymbol* lookup(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }
};
