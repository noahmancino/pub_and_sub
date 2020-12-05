project3: quacker.c project3.h
	gcc -pthread -o quacker quacker.c project3.h

clean:
	rm quacker