simpleShell: token.o command.o simpleShell.o 
	gcc token.o command.o simpleShell.o -o simpleShell

simpleShell.o: simpleShell.c token.h command.h
	gcc -c simpleShell.c 

token.o: token.c token.h
	gcc -c token.c

command.o: command.c command.h
	gcc -c command.c

clean:
	rm *.o