all: my_shell

my_shell: assign1_part2.c
	gcc -o my_shell assign1_part2.c -std=gnu11

clean:
	rm -f my_shell 
	
run: my_shell
	./my_shell
