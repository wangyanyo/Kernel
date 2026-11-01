FILES = ./build/kernel.asm.o ./build/kernel.o ./build/sched.o ./build/heap.o ./build/elfloader.o \
	./build/elf.o ./build/classic.o ./build/keyboard.o ./build/isr80h.o ./build/print.o \
	./build/task.asm.o ./build/tss.asm.o ./build/gdt.o ./build/gdt.asm.o \
	./build/fat16.o ./build/file.o ./build/streamer.o ./build/string.o ./build/pparser.o ./build/disk.o \
	./build/paging.o ./build/paging.asm.o ./build/kheap.o ./build/io.asm.o ./build/idt.asm.o \
	./build/idt.o

INCLUDES = -I./include -I./driver
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all: ./build/bin/boot.bin ./build/bin/kernel.bin user_programs
	rm -rf ./build/bin/os.bin
	dd if=./build/bin/boot.bin >> ./build/bin/os.bin
	dd if=./build/bin/kernel.bin >> ./build/bin/os.bin
	dd if=/dev/zero bs=1048576 count=16 >> ./build/bin/os.bin
	sudo mount -t vfat ./build/bin/os.bin /mnt/d
	sudo cp -r ./root/* /mnt/d
	sudo cp ./programs/blank/blank.elf /mnt/d
	sudo cp ./programs/shell/shell.elf /mnt/d
	sudo umount /mnt/d

./build/bin/kernel.bin: $(FILES)
	i686-elf-ld -g -relocatable $(FILES) -o ./build/kernelfull.o
	i686-elf-gcc -T ./linker.ld -o ./build/bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/kernelfull.o

./build/bin/boot.bin: ./arch/x86/boot/boot.asm
	nasm -f bin ./arch/x86/boot/boot.asm -o ./build/bin/boot.bin

./build/kernel.asm.o: ./arch/x86/kernel/kernel.asm
	nasm -f elf -g ./arch/x86/kernel/kernel.asm -o ./build/kernel.asm.o

./build/kernel.o: ./init/kernel.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./init/kernel.c -o ./build/kernel.o

./build/idt.asm.o: ./arch/x86/kernel/idt.asm
	nasm -f elf -g ./arch/x86/kernel/idt.asm -o ./build/idt.asm.o

./build/idt.o: ./arch/x86/kernel/idt.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./arch/x86/kernel/idt.c -o ./build/idt.o

./build/print.o: ./lib/print.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./lib/print.c -o ./build/print.o

./build/io.asm.o: ./arch/x86/kernel/io.asm
	nasm -f elf -g ./arch/x86/kernel/io.asm -o ./build/io.asm.o

./build/heap.o: ./mm/heap.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./mm/heap.c -o ./build/heap.o

./build/kheap.o: ./mm/kheap.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./mm/kheap.c -o ./build/kheap.o

./build/paging.asm.o: ./arch/x86/kernel/paging.asm
	nasm -f elf -g ./arch/x86/kernel/paging.asm -o ./build/paging.asm.o

./build/paging.o: ./arch/x86/kernel/paging.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./arch/x86/kernel/paging.c -o ./build/paging.o

./build/disk.o: ./kernel/disk.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./kernel/disk.c -o ./build/disk.o

./build/pparser.o: ./fs/pparser.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./fs/pparser.c -o ./build/pparser.o

./build/string.o: ./lib/string.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./lib/string.c -o ./build/string.o

./build/streamer.o: ./kernel/streamer.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./kernel/streamer.c -o ./build/streamer.o

./build/file.o: ./fs/file.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./fs/file.c -o ./build/file.o

./build/fat16.o: ./fs/fat/fat16.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./fs/fat/fat16.c -o ./build/fat16.o

./build/gdt.o: ./arch/x86/kernel/gdt.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./arch/x86/kernel/gdt.c -o ./build/gdt.o

./build/gdt.asm.o: ./arch/x86/kernel/gdt.asm
	nasm -f elf -g ./arch/x86/kernel/gdt.asm -o ./build/gdt.asm.o

./build/tss.asm.o: ./arch/x86/kernel/tss.asm
	nasm -f elf -g ./arch/x86/kernel/tss.asm -o ./build/tss.asm.o

./build/sched.o: ./kernel/sched.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./kernel/sched.c -o ./build/sched.o

./build/task.asm.o: ./arch/x86/kernel/task.asm
	nasm -f elf -g ./arch/x86/kernel/task.asm -o ./build/task.asm.o

./build/isr80h.o: ./kernel/isr80h.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./kernel/isr80h.c -o ./build/isr80h.o

./build/keyboard.o: ./kernel/keyboard.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./kernel/keyboard.c -o ./build/keyboard.o

./build/classic.o: ./driver/keyboard/classic.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./driver/keyboard/classic.c -o ./build/classic.o

./build/elf.o: ./fs/elf.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./fs/elf.c -o ./build/elf.o

./build/elfloader.o: ./fs/elfloader.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./fs/elfloader.c -o ./build/elfloader.o

user_programs:
	cd ./programs/stdlib && $(MAKE) all
	cd ./programs/blank && $(MAKE) all
	cd ./programs/shell && $(MAKE) all

user_programs_clean:
	cd ./programs/stdlib && $(MAKE) clean
	cd ./programs/blank && $(MAKE) clean
	cd ./programs/shell && $(MAKE) clean

clean: user_programs_clean
	rm -rf ./bin/boot.bin
	rm -rf ./bin/kernel.bin
	rm -rf ./bin/os.bin
	rm -rf ./build/kernelfull.o
	rm -rf ${FILES}