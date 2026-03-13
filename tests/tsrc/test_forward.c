#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <po.h>

#define PROCSN 3

pid_t  pids[PROCSN+1];

static void fail(char * com){
	if (*com!=0) printf("%s FAIL\n", com);
	else printf("FAIL\n");
	kill(0, SIGKILL);
	exit(1);
}

volatile int bl;
void handler(int sig){
	bl=1;
}

static void fill_package(package *pp, int seed){
	srand(seed);
	int* pi = (int *)pp;

	for (int i=0; i< (sizeof(package)/sizeof(int)); i++){
		(*pi) = rand();
		pi++;
	}
}

int main(){
	package p1,p2;
	int r;
	pid_t chpid, sender;

	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	sigemptyset(&mask);

	setsid();

	int runs_left = 2;
	pids[0] = getpid();
	for (int i=1;i < PROCSN+1; i++){
		chpid = fork();
		if (chpid == -1) {
			printf("fork failed, aborting.\n");
			kill(0,SIGINT);
			exit(1);  	
		} else if (chpid ==0){
			// child code
			while(runs_left--){
				bl=0;
				while (bl==0) sigsuspend(&mask);
				
				r = forward(pids[i-1]);
				if (r!=0) fail("Intermediate forward failed.");
				kill(pids[i-1], SIGUSR1);
			}
			//try to retrieve, expect failure
			r = retrieve(&p1, &sender);
			if (r==0) fail("Retrieve succeeded from the mailbox that should be empty.");
			return 0;
		}

		pids[i]=chpid;
	}

	printf("Testing forward:\n");
	fill_package(&p1, 7);
	if (post(&p1,pids[PROCSN])!=0) fail("First post failed.");
	kill(pids[PROCSN], SIGUSR1);

	runs_left--;
	while(runs_left--){
		bl=0;
		while (bl==0) sigsuspend(&mask);
				
		r = forward(pids[PROCSN]);
		if (r!=0) fail("Intermediate forward failed.");
		kill(pids[PROCSN], SIGUSR1);
	}

	r = retrieve_wait(&p2, &sender);
	printf(".final retrieve: ");
	if (r!=0) fail("");
	else if (sender!=getpid()) fail("Incorrect sender value.");
	else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
	else printf("OK\n");
	printf("OK\n");

	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		bl=0;
		while (bl==0) sigsuspend(&mask);
		r = post(&p1, getppid());
		if (r!=0) fail("Post failed");
		kill(getppid(), SIGUSR1);
		bl=0;
		while (bl==0) sigsuspend(&mask);
		return 0;
	}

	printf("Testing forwarding from empty mailbox:\n");
	fill_package(&p1, 7);
	r = forward(chpid);
	if (r!=-1) fail("Forwarding from empty mailbox succeeded.");
	else if (errno!=ENOMSG) fail("Invalid errno value.");
	printf("OK\n");
	
	printf("Testing forwarding to occupied mailbox:\n");
	r = post(&p1, chpid);
	if (r!=0) fail("Post failed");
	kill(chpid, SIGUSR1);
	bl=0;
	while (bl==0) sigsuspend(&mask);
	r = forward(chpid);
	if (r!=-1) fail("Forwarding to occupied mailbox succeeded.");
	else if (errno!=EBUSY) fail("Invalid errno value.");
	printf(".testing mailbox after failed forward: ");
	r = retrieve(&p2, &sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	else printf("OK\n");
	printf("OK\n");
	
	kill(chpid, SIGUSR1);
	return 0;
}
