## Brainfuck Language Specification

This document defines the Brainfuck syntax and runtime behavior expected by this benchmark.

## Syntax

A Brainfuck source file is a sequence of characters.
Only the eight commands below are executable:

- `>` move the data pointer one cell to the right
- `<` move the data pointer one cell to the left
- `+` increment current cell value by 1
- `-` decrement current cell value by 1
- `.` output current cell as one byte/character
- `,` read one byte from input into current cell
- `[` if current cell is 0, jump to matching `]`
- `]` if current cell is not 0, jump back to matching `[`

Any character other than these eight instructions must be ignored by the implementation.
This includes whitespace, punctuation, letters, and any other non-instruction character, so they may be used as comments.

## Memory Model

- Tape starts with one cell initialized to 0.
- Pointer starts at cell index 0.
- Moving right beyond current tape size extends the tape with new zeroed cells.
- Moving left from index 0 is a runtime error.

## Cell Semantics

- Cells are 8-bit unsigned values.
- Arithmetic wraps modulo 256:
  - `255 + 1 = 0`
  - `0 - 1 = 255`

## Input/Output Semantics

- `,` reads one byte from stdin.
- If no input byte is available (EOF), `,` stores `0` in the current cell.
- `.` writes the current cell value as a single output byte/character.

## Loop Semantics

- Brackets must be balanced.
- Unmatched `[` or `]` is a compile error.
- `[` checks current cell before entering loop body.
- `]` checks current cell to decide whether to continue looping.

## Error Conditions

- Compile error: unbalanced brackets.
- Runtime error: pointer moved below 0.

## Notes

- Programs should print exactly the required output. Extra whitespace or extra characters can fail tests.
- For this benchmark, treat stdin/stdout as raw byte streams even when test inputs are text.
