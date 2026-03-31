## Brainfuck Example Programs

This folder contains reference-style Brainfuck examples copied from `eval/brainfuck/reference`.

Requested set:
- E01
- E02
- E03 
- E06

## E01 - Print Hello World

Source: `E01_hello_world.bf`

Description (from eval):
No input is provided. Write exactly the string `Hello World!` to stdout with no extra whitespace before or after it.

Sample:
- Input: ``
- Output: `Hello World!`

## E02 - Echo Line

Source: `E02_echo_line.bf`

Description (from eval):
Read a single line of text (which may be empty) and echo it back exactly as received, preserving every character including spaces and punctuation.

Samples:
- Input: `Hello`
- Output: `Hello`
- Input: `   spaces   `
- Output: `   spaces   `

## E03 - Hello Name

Source: `E03_hello_name.bf`

Description (from eval):
Read one line containing a name. Output `Hello, NAME!` where NAME is the input text exactly as given (case-sensitive, no trimming).

Samples:
- Input: `Ada`
- Output: `Hello, Ada!`
- Input: `123`
- Output: `Hello, 123!`

## E06 - Even Or Odd

Source: `E06_even_or_odd.bf`

Description (from eval):
Read a single integer `n`. If `n` is divisible by 2, output the word `even`; otherwise output `odd`.

Samples:
- Input: `4`
- Output: `even`
- Input: `7`
- Output: `odd`
- Input: `-6`
- Output: `even`
