# Assembly-code

---

## What this is

A minimal compiler pipeline written in C.
It takes a simple C-like program and walks it down to **binary**.
If you’ve ever written code and trusted the compiler like it’s some divine entity, this breaks that illusion.

---

## What it does

```
Source Code
   ↓
Lexer
   ↓
Parser
   ↓
AST
   ↓
IR
   ↓
Binary
```

---

## Why this exists

Most people write code and stop at “it works”.
They don’t ask what the machine actually sees.
No. It does exactly what you tell it, just at a level you stopped paying attention to.

---

## What’s inside

* `main.c` → full pipeline implementation
* `README.md` → what you’re reading

That’s it.

---

## How to run

```
gcc main.c -o main
./main
```

Or pass your own input:

```
./main "int main(){ int x; x = 3 + 2; return x; }"
```

---

## A little history (since you probably skipped it in class)

Programming didn’t start with C.
It started with raw binary. Then assembly. Then compilers.
Each layer was built to make things easier.
And with each layer, people understood less of what was actually happening.

---

## The point

This project strips that comfort away.
You don’t get to pretend the compiler is magic.
You see exactly how your code becomes something a machine can execute.

---

