#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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

char *readTill(char chara) {
  // Hi, I'm chara the explorer! In Spanish, char is carbonizarse! Can you say "carbonizarse"?
  char ch;
  char *gotten = NULL;
  while ((ch = fgetc(compiler)) != EOF && (ch != chara)) {
    if (ch == '\\') {
      ch = fgetc(compiler);
      if (ch == EOF) {
        fprintf(stderr, "Error: Unexpected EOF after escape character\n");
        free(gotten);
        fclose(compiler);
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

int main() {
  printf("Starting main loop\n");
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
      str = readTill('"');  // Read until closing quote
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

  fclose(compiled_out);

  char lastChar = 0;

  FILE *bufferFile = fmemopen(buffer, strlen(buffer), "r");

  char* noNewLine = NULL;

  while ((ch = fgetc(bufferFile)) != EOF) {
    if ((ch == '\n') && (lastChar == '\n')) {
      // Skip - don't add
    } else {
      charCat(&noNewLine, ch);
    }
    lastChar = ch;
  }
  fclose(bufferFile);

  FILE *ASTMake = fmemopen(noNewLine, strlen(noNewLine), "r");
  // keep bufferFile open so we can get it
  // begin replacing variables :)
  // make a simple AST :3
  fclose(bufferFile);
  return 0;
}
