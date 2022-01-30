# CC := gcc -fopenmp -openacc
CC := nvc -Minfo=all -acc -fopenmp
OPT := -lm -O3

all:
	make nbody
	make nbody_omp
	make nbody_acc

nbody: nbody.c
	$(CC) nbody.c -o nbody $(OPT)

nbody_omp: nbody_omp.c
	$(CC) nbody_omp.c -o nbody_omp $(OPT)

nbody_acc: nbody_acc.c
	$(CC) nbody_acc.c -o nbody_acc $(OPT)

clean:
	rm -f nbody nbody_omp nbody_acc
