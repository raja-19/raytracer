main: main.c
	gcc glad.c main.c -o main -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -ldl -lm
