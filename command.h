#ifndef COMMAND_H
#define COMMAND_H

#define MAX_NUM_COMMANDS  1000

// command separators
#define pipeSep  "|"                            // pipe separator "|"
#define conSep   "&"                            // concurrent execution separator "&"
#define seqSep   ";"                            // sequential execution separator ";"

struct CommandStruct {
   int first;      // index to the first token in the array "token" of the command
   int last;       // index to the last token in the array "token" of the command
   char *sep;	   // the command separator that follows the command,  must be one of "|", "&", and ";"
   char **argv;       // an array of tokens that forms a command
   char *stdin_file;   // if not NULL, points to the file name for stdin redirection                        
   char *stdout_file;  // if not NULL, points to the file name for stdout redirection     
};

typedef struct CommandStruct Command;  // command type

int separateCommands(char *token[], Command command[]);

void fillCommandStructure(Command *cp, int first, int last, char *sep);

int separator(char *token);

#endif /* COMMAND_H */
