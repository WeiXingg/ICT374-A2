#ifndef TOKEN_H
#define TOKEN_H

#define MAX_ARGS 1000
#define tokenSeparators " \t\n"

int tokenise(char* line, char *token[]);

#endif