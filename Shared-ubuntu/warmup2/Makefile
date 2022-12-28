#
# This is the Makefile that can be used to create the "listtest" executable
# To create "listtest" executable, do:
#	make listtest
#

my402list.o: my402list.c my402list.h
	gcc -g -c -Wall my402list.c

clean:
	rm -f *.o warmup2

warmup2.o: warmup2.c my402list.h PacketArrivalThread.h TokenDepositThread.h ServerThread.h CatchCntlThread.h Packet.h
	gcc -g -c -Wall warmup2.c

warmup2: warmup2.o my402list.o PacketArrivalThread.o TokenDepositThread.o ServerThread.o Packet.o CatchCntlThread.o
	gcc -o warmup2 -g warmup2.o my402list.o PacketArrivalThread.o TokenDepositThread.o ServerThread.o Packet.o CatchCntlThread.o -lpthread -lm

PacketArrivalThread.o: PacketArrivalThread.c PacketArrivalThread.h Packet.h
	gcc -g -c -Wall PacketArrivalThread.c

TokenDepositThread.o: TokenDepositThread.c TokenDepositThread.h Packet.h
	gcc -g -c -Wall TokenDepositThread.c

ServerThread.o: ServerThread.c ServerThread.h Packet.h
	gcc -g -c -Wall ServerThread.c

Packet.o: Packet.c Packet.h
	gcc -g -c -Wall Packet.c

CatchCntlThread.o: CatchCntlThread.c CatchCntlThread.h Packet.h
	gcc -g -c -Wall CatchCntlThread.c