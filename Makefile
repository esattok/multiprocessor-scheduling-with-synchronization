all: mps mps_cv

mps: mps.c
	gcc -Wall -g -o mps mps.c -lpthread -lrt -lm

mps_cv: mps_cv.c
	gcc -Wall -g -o mps_cv mps_cv.c -lpthread -lrt -lm

clean: 
	rm -fr mps mps_cv *~ *.o