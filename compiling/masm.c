#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "vect.h"

// Global FILE pointers
FILE *compiler;
FILE *compiled_out;
int charCat(char **string, char character) { // Concat a character to a charrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr (arf)
  size_t stringLen;
  char *temp;

  stringLen = (*string) ? strlen(*string) : 0;

  temp = realloc(*string, stringLen + 2);

  if (temp == NULL) {
      return -1;
  }

  *string = temp;

  (*string)[stringLen] = character;
  (*string)[stringLen + 1] = '\0';

  return 0;
}

char *readTill(FILE *f, char chara) {
  // Hi, I'm chara the explorer! In Spanish, char is carbonizarse! Can you say "carbonizarse"?
  char ch;
  char *gotten = NULL;
  while ((ch = fgetc(f)) != EOF && (ch != chara)) {
    if (ch == '\\') {
      ch = fgetc(f);
      if (ch == EOF) {
        fprintf(stderr, "Error: Unexpected EOF after escape character\nsome files not closed-\n");
        free(gotten);
        fclose(f);
        fclose(compiled_out);
        exit(1);
      }
      if (ch == 'n') {
        charCat(&gotten, '\n');
      } else if (ch == 't') {
        charCat(&gotten, '\t');
      } else if (ch == 'r') {
        charCat(&gotten, '\r');
      } else if (ch == '\\') {
        charCat(&gotten, '\\');
      } else {
        charCat(&gotten, ch);
      }
    } else {
      charCat(&gotten, ch);
    }
  }
  return gotten;
}

bool checkNext(FILE *buff, char toCheck[]) {
  for (int i = 0; i < strlen(toCheck); ++i) {
    if (toCheck[i] == fgetc(buff)) {
      continue;
    } else {
      for (int undone = i; undone > 0; --undone) {
        ungetc(toCheck[undone], buff);
      }
      return false;
    }
  }
  return true;
}

const uint8_t opgen(uint8_t reg, uint8_t op) {
  return (reg << 5) | op;
}

uint8_t eq, not_eq, less_than, and, or, jmp, jmp_if_not, not, jmp_if, // logic
        get, set, dec, unset, push, // variable
        call, tail, ret, builtin, // function
        halt, nop, pop, index_, yield, // general
        add, sub, mult/*, div, addm, divm, multm, subm, mod, pow*/; // math

int main() {
  eq = opgen(0,0);
  not_eq = opgen(0,1);
  less_than = opgen(0,2);
  and = opgen(0,3);
  or = opgen(0,4);
  jmp = opgen(0,5);
  jmp_if_not = opgen(0,6);
  not = opgen(0,7);
  jmp_if = opgen(0,8);

  get = opgen(1,0);
  set = opgen(1,1);
  dec = opgen(1,2);
  unset = opgen(1,3);
  push = opgen(1,4);

  call = opgen(2,0);
  tail = opgen(2,1);
  ret = opgen(2,2);
  builtin = opgen(2,3);

  halt = opgen(4,0);
  nop = opgen(4,1);
  pop = opgen(4,2);
  index_ = opgen(4,3);
  yield = opgen(4,4);

  add = opgen(5, 0);
  sub = opgen(5, 1);
  mult = opgen(5, 2);
  /*div = opgen(5, 3);
  addm = opgen(5, 4);
  divm = opgen(5, 5);
  multm = opgen(5, 6);
  subm = opgen(5, 7);
  mod = opgen(5, 8);
  pow = opgen(5, 9);*/

  char ch;
  char *str = NULL;

  char *buffer = NULL;
  size_t bufferSize = 0;

  bool seenChar = false;

  bool lastWasNL = false;

  if ((compiler = fopen("compiler.asm", "r")) == NULL) {
    perror("Error opening file compiler.asm");
    exit(1); // (cat)arf; (dog)moo;
  }
  if ((compiled_out = fopen("comp_out", "w+")) == NULL) {
    printf("Error opening comp_out");
  }

  while ((ch = fgetc(compiler)) != EOF) {
    while ((ch == ' ') && (!seenChar)) {
      ch = fgetc(compiler);
      if (ch != ' ') {
        seenChar = true;
      }
    }

    if (ch == '"') {
      charCat(&buffer, '"'); // Print opening quote
      str = readTill(compiler, '"');  // Read until closing quote
      if (str) {
        for (char *p = str; *p; ++p) {
          charCat(&buffer, *p);
        }
        free(str);
        str = NULL;
      }
      charCat(&buffer, '"'); // Print closing quote
    } else {
      charCat(&buffer, ch);
    }
    if (ch == '\n') {
      seenChar = false;
    }
  }
  str = NULL;
  fclose(compiler);

  char lastChar = 0;

  FILE *bufferFile = fmemopen(buffer, strlen(buffer), "r");

  char *noNewLine = NULL;

  while ((ch = fgetc(bufferFile)) != EOF) {
    if ((ch == '\n') && (lastChar == '\n')) {
      ch = fgetc(bufferFile);
    } else {
      charCat(&noNewLine, ch);
    }
    lastChar = ch;
  }
  fclose(bufferFile);

  // processed
  FILE *rmNL = fmemopen(noNewLine, strlen(noNewLine), "r");

  char *noComment = NULL;
  bool inQuote = false;

  // remove comments

  while ((ch = fgetc(rmNL)) != EOF) {
    if (ch == '\"') {
      inQuote = !inQuote;
    }
    if (!inQuote && ch == ';') {
      while (ch != '\n' && ch != EOF) {
        ch = fgetc(rmNL);
      }
      charCat(&noComment, '\n');
    } else {
      charCat(&noComment, ch);
    }
  }
  for (int i = 0; i < strlen(noNewLine); ++i) {
    putchar(noComment[i]);
  }

  fclose(rmNL);
  FILE *final = fmemopen(noComment, strlen(noComment), "r");

  // output
  char *out_ = '\0';
  FILE *out = fmemopen(out_, 1, "w+");


  // header
  char *head = "  \xc2\xbd" "6e" "\xc3\xa8" "40\0";
  FILE *header = fmemopen(head, strlen(head), "w");

  // VEC(char) ASTTree = {0};
  char emitted_bytes;

  inQuote = false;

  uint64_t main_end = 0;

  while ((emitted_bytes = fgetc(final)) != EOF) {
    if (emitted_bytes == '\"') {
      inQuote = !inQuote;
    }
    if (checkNext(final, ".") && !inQuote) {
      while (emitted_bytes != ':') {
        if (emitted_bytes == ' ' || emitted_bytes == '\n') {
          perror("Namespace MUST start with '.' and end with ':', and cannot contain spaces\n");
        } else {
          // push(&final, fgetc(final));
          fputc(emitted_bytes, out);
          main_end++;
        }
        emitted_bytes = fgetc(final);
      }
    } else if (checkNext(final, "set")  && !inQuote) {
      fputc(set, out);
      fgetc(final);
      while (emitted_bytes != ' ' && emitted_bytes != '\n' && emitted_bytes != EOF) {
        fputc(emitted_bytes, out);
        emitted_bytes = fgetc(final);
      }
    } else if (checkNext(final, "push")) {
      fgetc(final);
      fputc(push, out);
      if (checkNext(final, "\"")) {
        char *str = readTill(final, '"');
        uint64_t len = strlen(str);
        fwrite(&len, 8, 1, out);
        fwrite(str, 1, len, out);
        free(str);
      }
    }
  }
  //rewind(out);
  while ((ch = fgetc(out)) != EOF) {
    putchar(ch);
    fputc(ch, compiled_out);
  }
  rewind(out);
  rewind(compiled_out);
  rewind(final);
  fflush(out);
  fflush(compiled_out);
  fflush(final);
  main_end = ftell(compiled_out);

  fclose(compiled_out);
  fclose(final);
  fclose(out);
  return 0;
}
