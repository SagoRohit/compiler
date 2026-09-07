# CSE 310 Assignment 4 — Intermediate Code Generation (Phase 1)

Student ID: 2005071

## What this is

An ANTLR4 (C++ target) visitor-based compiler that reads a program written in
the C-subset grammar from Assignment 3 and emits 32-bit x86 Linux assembly
for FASM (https://flatassembler.net/), following the conventions demonstrated
in `fasm/tutorial.pdf` and `fasm/codes/*.asm` (stack-frame based locals/params,
`INT 0x80` syscalls, the `OUTDEC` printing routine, jumping/short-circuit code
for boolean expressions).

Two files are produced from an input `.c` file:

- `code.asm` — generated straight from the parse tree, annotated with the
  originating source line as a comment above each statement's instructions.
- `optimized_code.asm` — `code.asm` after a peephole optimization pass
  (`PeepholeOptimizer.cpp`), implementing the four redundancy patterns from
  the spec: redundant `MOV a,b`/`MOV b,a`, redundant `PUSH x`/`POP x`,
  `ADD/SUB reg,0` and `MOV reg,1`+`MUL reg`, and collapsing runs of
  consecutive labels (rewriting jump/call targets accordingly).

Both files assemble and run under FASM (a Linux build of `fasm` is bundled at
`tools/fasm` in case it isn't installed on the grading machine).

## Phase 1 scope

Per `P1_Grammar.pdf`, this submission implements the highlighted subset only:

- global & local **non-array** `int`/`void` declarations
- **zero-argument** function definitions (`int`/`void` return type)
- expressions: `+ - * /  %`, unary `+ - !`, relational `< <= > >= == !=`,
  assignment, postfix `++`/`--`
- statements: variable declarations, expression statements, nested blocks,
  `println(ID)`, `return expr;`

Constructs that are part of the *incremental* grammar but explicitly deferred
to Phase 2 (`if`/`if-else`/`while`/`for`, logical `&&`/`||`, arrays,
function parameters, and function calls) are still *parsed* (the same full
grammar from Assignment 3 is reused, unmodified, as required by the spec),
but the code generator emits a `; [Phase 2] ...` comment and a safe stub
instead of real code for them, so the tool never crashes even if it is fed
a program that goes beyond Phase 1 — it just won't produce a functionally
correct translation for those parts. `tests/` only contains programs that
stay within the Phase 1 subset.

Note: the sample files under `../input/` (the top-level `input/` folder next
to `P1_Grammar.pdf`) exercise the *entire* two-phase grammar (if/while/for,
function calls with arguments, arrays, `&&`/`||`), so they are not expected
to produce fully correct output yet — that is Phase 2's job. Custom,
Phase-1-only test programs are provided instead, under `tests/`.

One more deliberate deviation from the reused Assignment 3 grammar: the
lexer's `PRINTLN` token now matches the literal keyword `println` (the
Assignment 3 lexer mapped it to `printf`, to match `P1_Grammar.pdf` /
`println(ID);` used throughout `../input/*.c`).

## Files

- `CSubset.g4`, `Lexer.g4` — the grammar (same productions as Assignment 3;
  file renamed to match the grammar name, which newer ANTLR versions require).
- `CGSymbol.h` — minimal scope/symbol bookkeeping for code generation
  (global label vs. `[EBP±N]` stack operand, function table).
- `CodeGenVisitor.h/.cpp` — the visitor that walks the parse tree and emits
  assembly (no embedded actions in the grammar file, as required).
- `PeepholeOptimizer.h/.cpp` — the optimization pass, operating purely on the
  text of `code.asm`.
- `main.cpp` — driver: parses the input file, runs the visitor, writes
  `code.asm`, runs the optimizer, writes `optimized_code.asm`.
- `build.sh` — generates the parser from the grammar (via ANTLR) and compiles
  everything into `./icg.out`.
- `run.sh <input.c>` — builds if needed, runs `icg.out`, assembles both
  `.asm` files with FASM, and runs both resulting executables.
- `tests/*.c` — custom Phase-1-only sample programs.
- `tools/fasm` — a bundled Linux FASM 1.73.32 binary (used automatically by
  `run.sh` if `fasm` isn't already on `PATH`).

## How to run

```
./run.sh tests/t1_arith.c
```

This regenerates `code.asm` / `optimized_code.asm` in the current directory,
assembles them into `code.out` / `optimized_code.out`, and runs both.

## Design notes

- Every expression-generating visit leaves its result in `EAX` on exit. A
  binary operator evaluates the right operand first, `PUSH`es it, evaluates
  the left operand into `EAX`, `POP`s the right operand into `EBX`, then
  operates `EAX`/`EBX` — the exact pattern used throughout the FASM tutorial.
- All locals live on the stack (`[EBP-N]`), allocated on function entry via
  `SUB ESP, N`. Since the total frame size is only known after the whole
  function body has been visited, each function's body is generated into an
  in-memory buffer first (`CodeGenVisitor::funcBodyBuffer` — the "at most one
  temporary file" the spec allows is implemented as an in-memory buffer, so
  zero on-disk temp files are actually needed) and only written out, wrapped
  in the now-known prologue/epilogue, once the function is fully visited.
- `main()` is emitted as the literal `main:` label (matching `entry main`);
  every other function is emitted as `func_<name>` to avoid colliding with
  FASM instruction mnemonics (e.g. a C function named `add`).
- Relational operators and `!` use jumping code (`CMP`/`Jcc` into a 0/1
  result), as required by the spec for boolean expressions.
