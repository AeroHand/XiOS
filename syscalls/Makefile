ALL: cat grep hello ls pingpong sched shell sigtest shutdown play stop pause resume

%.o: %.c
	gcc -c -Wall -g -o $@ $<

%.o: %.S
	gcc -c -Wall -g -o $@ $<

cat.exe: ece391cat.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o cat.exe ece391cat.o ece391syscall.o ece391support.o
cat: cat.exe
	../elfconvert cat.exe
	mv cat.exe.converted to_fsdir/cat

grep.exe: ece391grep.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o grep.exe ece391grep.o ece391syscall.o ece391support.o
grep: grep.exe
	../elfconvert grep.exe
	mv grep.exe.converted to_fsdir/grep

hello.exe: ece391hello.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o hello.exe ece391hello.o ece391syscall.o ece391support.o
hello: hello.exe
	../elfconvert hello.exe
	mv hello.exe.converted to_fsdir/hello

ls.exe: ece391ls.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o ls.exe ece391ls.o ece391syscall.o ece391support.o
ls: ls.exe
	../elfconvert ls.exe
	mv ls.exe.converted to_fsdir/ls

pingpong.exe: ece391pingpong.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o pingpong.exe ece391pingpong.o ece391syscall.o ece391support.o
pingpong: pingpong.exe
	../elfconvert pingpong.exe
	mv pingpong.exe.converted to_fsdir/pingpong

sched.exe: ece391sched.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o sched.exe ece391sched.o ece391syscall.o ece391support.o
sched: sched.exe
	../elfconvert sched.exe
	mv sched.exe.converted to_fsdir/sched

shell.exe: ece391shell.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o shell.exe ece391shell.o ece391syscall.o ece391support.o
shell: shell.exe
	../elfconvert shell.exe
	mv shell.exe.converted to_fsdir/shell

sigtest.exe: ece391sigtest.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o sigtest.exe ece391sigtest.o ece391syscall.o ece391support.o
sigtest: sigtest.exe
	../elfconvert sigtest.exe
	mv sigtest.exe.converted to_fsdir/sigtest

shutdown.exe: ece391shutdown.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o shutdown.exe ece391shutdown.o ece391syscall.o ece391support.o
shutdown: shutdown.exe
	../elfconvert shutdown.exe
	mv shutdown.exe.converted to_fsdir/shutdown

play.exe: ece391play.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o play.exe ece391play.o ece391syscall.o ece391support.o
play: play.exe
	../elfconvert play.exe
	mv play.exe.converted to_fsdir/play

stop.exe: ece391stop.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o stop.exe ece391stop.o ece391syscall.o ece391support.o
stop: stop.exe
	../elfconvert stop.exe
	mv stop.exe.converted to_fsdir/stop

pause.exe: ece391pause.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o pause.exe ece391pause.o ece391syscall.o ece391support.o
pause: pause.exe
	../elfconvert pause.exe
	mv pause.exe.converted to_fsdir/pause

resume.exe: ece391resume.o ece391syscall.o ece391emulate.o ece391support.o
	gcc -g -nostdlib -o resume.exe ece391resume.o ece391syscall.o ece391support.o
resume: resume.exe
	../elfconvert resume.exe
	mv resume.exe.converted to_fsdir/resume

clean::
	rm -f *~ *.o

clean::
	rm -f *~ *.o

clear: clean
	rm -f *.converted
	rm -f *.exe
	rm -f to_fsdir/*
