## Brainfuck Bootstrap Task

Goal: produce a bootstrap compiler written in Brainfuck.

Expected output of the project:
- 3 built compiler artifacts (three compilation stages).
- 2 compiler programs written during the process.

## What You Have In This Workspace

- `instruction.md`: this task description.
- `lang.md`: Brainfuck syntax and semantics used by evaluation.
- `tests/`: example problem programs and their expected behavior.

## General Instructions

- Follow the language specification in `lang.md` exactly.
- The solution must not use the internet at any time (no network access during development, build, or runtime), and must not search for or use any other implementation of such a compiler available on the computer.
- Use the examples in `tests/` to validate your implementation strategy.
- If your compiler/runtime has errors, report them on stderr and return a non-zero exit code.
- The compiled program must not contain any runtime interpreter. The compiled program must execute as native code.
- Each compiler source must be a standalone compiler implementation, not a wrapper around another compiler or interpreter.
- In particular, the Brainfuck-written compiler must be valid standalone Brainfuck and executable on any Brainfuck interpreter/compiler that follows `lang.md`.
- The Brainfuck-written compiler must not be a marked bootstrap (i.e. no special-case handling in the C Brainfuck compiler for this specific compiler source).
- Prefer using comments in Brainfuck source files to document structure and important sections.

## Helper / Suggestions

- Use comments to structure the Brainfuck source clearly.
- Use multiple source files and assemble them at the end.
- You can use an intermediate IR to simplify code generation.

## Bootstrap Process

The bootstrap must be completed in 3 stages:

1. Stage 0 (host compiler in C)
- Write a compiler from Brainfuck to Linux i386 executable.
- This first compiler must be written in C.
- Build it with the host C toolchain.

2. Stage 1 (self-host candidate produced by Stage 0)
- Write the same Brainfuck-to-Linux-i386 compiler in Brainfuck.
- Use the Stage 0 C compiler to compile this Brainfuck compiler.
- The result is the first executable generated from Brainfuck source.

3. Stage 2 (bootstrap completion)
- Use the Stage 1 executable compiler to compile the Brainfuck compiler source again.
- This second self-compilation completes the bootstrap chain.

## Required Deliverables

- Compiler source A: Brainfuck-to-Linux-i386 compiler written in C.
- Compiler source B: same compiler written in Brainfuck.
- Built artifact 1: executable produced from compiler source A.
- Built artifact 2: executable produced from compiler source B using artifact 1.
- Built artifact 3: executable produced from compiler source B using artifact 2.

## Success Condition

Bootstrap is considered complete when artifact 2 and artifact 3 are both valid compilers and artifact 3 is produced by self-compilation.
