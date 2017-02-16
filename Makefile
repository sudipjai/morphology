CC = gcc -fPIC -std=gnu99

morphology: morphology.c
	@echo "Building ..."
	$(CC) -o morphology morphology.c
clean:
	rm -rf morphology
