GCC := gcc -fopenmp -openacc
NVC := nvc -Minfo=all -acc -fopenmp
OPT := -lm -O3

all:
	make nbody
	make nbody_omp
	make nbody_acc
	make nbody_cuda

nbody: nbody.c
	$(GCC) nbody.c -o nbody $(OPT)

nbody_omp: nbody_omp.c
	$(GCC) nbody_omp.c -o nbody_omp $(OPT)

nbody_acc: nbody_acc.c
	$(NVC) nbody_acc.c -o nbody_acc $(OPT)

nbody_cuda: nbody_cuda.cu
	nvcc nbody_cuda.cu -o nbody_cuda $(OPT)

clean:
	rm -f nbody nbody_omp nbody_acc nbody_cuda
