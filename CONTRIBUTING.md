# Contributing

## Goal

The end goal of this project is to show how a compiler pipeline works at a fundamental level.

Not just that it works, but how each transformation happens, step by step, without hiding anything behind abstraction.

Human Code → Readable Logic → Structured Representation (AST) → Machine-Level Instructions (IR) → Binary Output

Reaching this goal means:

* Every stage should be **visible and traceable**
* You should be able to follow how a single line of code changes form across stages
* No part of the pipeline should feel like a “black box”
* The implementation should prioritize **clarity over cleverness**
* A reader should be able to understand the system **from first principles**, not by trusting it

Every contribution should improve one of these:

* clarity of transformation
* correctness of representation
* or transparency of the process

If it adds complexity without increasing understanding, it moves the project away from the goal.

## Rules

* No unnecessary abstractions
* No external libraries
* No bloated features
* Keep everything in the spirit of understanding the pipeline

## What is welcome

* Fixes
* Clear improvements to existing stages
* Better clarity in logic or structure

## What is not

* Overengineering
* “Smart” shortcuts that hide the process

If your change makes the pipeline harder to understand, it’s not a good change.
