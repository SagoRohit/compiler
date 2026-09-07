#!/bin/bash
# Generates the ANTLR parser/visitor from the grammar and compiles the
# Phase 1 intermediate code generator into ./icg.out
set -e
cd "$(dirname "$0")"

GRAMMAR=CSubset.g4
ANTLR_JAR=$(command -v antlr4-jar-path 2>/dev/null || true)
for candidate in /usr/local/lib/antlr-4.13.2-complete.jar /usr/local/lib/antlr4-complete.jar \
                 /home/sagor/antlr-4.13.2-complete.jar "$HOME"/antlr-4.13.2-complete.jar; do
    if [ -f "$candidate" ]; then ANTLR_JAR="$candidate"; break; fi
done

echo "[1/2] Generating parser/visitor from grammar ..."
if command -v antlr4 >/dev/null 2>&1; then
    antlr4 -Dlanguage=Cpp -visitor -no-listener "$GRAMMAR"
else
    java -jar "$ANTLR_JAR" -Dlanguage=Cpp -visitor -no-listener "$GRAMMAR"
fi

echo "[2/2] Compiling ..."
g++ -std=c++17 -w -I/usr/local/include/antlr4-runtime \
    CodeGenVisitor.cpp PeepholeOptimizer.cpp main.cpp \
    CSubsetLexer.cpp CSubsetParser.cpp CSubsetBaseVisitor.cpp CSubsetVisitor.cpp \
    -L/usr/local/lib -lantlr4-runtime -pthread -o icg.out

echo "Build complete: ./icg.out"
