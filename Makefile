LIBS = -lx264
v4l2:main.o yuv.o
	cc -o v4l2 main.o yuv.o $(LIBS)
yuv.o:yuv.c yuv.h
	cc -c yuv.c
main.o:main.c yuv.h
	cc -c main.c
clean:
	rm v4l2 main.o yuv.o
