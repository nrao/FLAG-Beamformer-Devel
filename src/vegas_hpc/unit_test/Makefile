
all: test1
test1: sw_machine_test.c ../src/SwitchingStateMachine.c ../src/SwitchingStateMachine.h
	gcc -g -o sw_machine_test sw_machine_test.c -I../src/ ../src/SwitchingStateMachine.c

clean:
	rm sw_machine_test
