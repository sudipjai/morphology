CC = gcc -fPIC -std=gnu99
CFLAGS = -lpthread

morphology: morphology.c
	@echo "Building ..."
	$(CC) -o morphology morphology.c $(CFLAGS)
clean:
	rm -rf morphology
