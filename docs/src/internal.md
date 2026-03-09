# Internals
This page is about the inside of the Minis Compiler, and how it works.

## Classes

## Parsing
Minis parsing is done in multiple passes, where the first one is a syntax check.
After the syntax check, it begins to specify the tree.
Things like...
```minis
int x = 10;
x == 10 ? continue : print("not 10 :(\n");
```
After the first parse, it will become...
```minis-ast
IntKW x eq 10 Semicolon x eq eq 10 QuestionMark ContinueKW Colon fnCall print LParen str "not 10 :(\n" RParen Semicolon
```
This is super basic and literal. It is unoptomized. The next check is an optomization pass, where the AST drops so much literalness, and pattern matches things.
Once this happens, over multiple passes, it becomes...
```minis-ast
intVarMake x 10 TernaryStart TernaryCond intVarGet x isEQ 10 TernaryTrue continueKW TernaryFalse fnCall print [str "not 10 :("] TernaryEnd
```
