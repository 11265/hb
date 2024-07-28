AS = xcrun -sdk iphoneos clang -arch arm64 -mios-version-min=13.0 -c
LD = xcrun -sdk iphoneos clang -arch arm64 -mios-version-min=13.0 -o
OBJS = main.o get_pid.o print_number.o

all: getpid

getpid: $(OBJS)
    $(LD) getpid $(OBJS) -e _main -lSystem -nostdlib

main.o: main.s api.h
    $(AS) main.s -o main.o

get_pid.o: get_pid.s api.h
    $(AS) get_pid.s -o get_pid.o

print_number.o: print_number.s api.h
    $(AS) print_number.s -o print_number.o

clean:
    rm -f $(OBJS) getpid
