#include <stdio.h>
#include <string.h>
#include "token.h"

int tokenise(char *line, char *token[])
{
    int i = 0;
    char *tk = strtok(line, tokenSeparators);

    token[i] = tk;
    
    while (tk != NULL)
    {
        i++;
        if (i >= MAX_ARGS)
        {
            i = -1;
            break;
        }

        tk = strtok(NULL, tokenSeparators);
        token[i] = tk;
    }
    return i;
}