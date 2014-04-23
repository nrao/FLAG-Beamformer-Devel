
all: sw_machine_test blk_machine_test
sw_machine_test: sw_machine_test.c ../src/SwitchingStateMachine.c ../src/SwitchingStateMachine.h
	gcc -g -o sw_machine_test sw_machine_test.c -I../src/ ../src/SwitchingStateMachine.c

blk_machine_test: blk_machine_test.cc ../src/BlankingStateMachine.cc ../src/BlankingStateMachine.h
	g++ -g -o blk_machine_test blk_machine_test.cc -I../src/ ../src/BlankingStateMachine.cc
clean:
	rm sw_machine_test blk_machine_test
