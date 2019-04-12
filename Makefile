OBJ	= streamer.o v4l2.o network.o 
CC = arm-linux-gnueabihf-gcc
override CFLAGS += -Wall -g
output : $(OBJ)	
	$(CC) $^ -o $@ -lpthread
	@echo "###############compile successfully!###############"
streamer.o	:	streamer.h v4l2.h network.h
v4l2.o	:	streamer.h v4l2.h	streamer.h
network.o	:	v4l2.h network.h streamer.h
.PHONY: clean
clean	:
		rm output $(OBJ)
		@echo "###############clean successfully!###############"
