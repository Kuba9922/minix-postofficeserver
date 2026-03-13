#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <po.h>

pid_t chpid;

static void fail(char * com){
	if (*com!=0) printf("%s FAIL\n", com);
	else printf("FAIL\n");
	if (chpid!=0) kill(chpid, SIGKILL);
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
	pid_t sender;

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

	chpid = fork();

	if (chpid == -1) return -1;
	if (chpid == 0 ) {
		int c=0;
		fill_package(&p1,1);
		sleep(1);
		if (post(&p1, getppid())!=0) {
			printf("Post in child failed!\n");
			kill(getppid(), SIGINT);
			return -1;
		};

		bl=0;
		while (bl==0) sigsuspend(&mask);
		
		fill_package(&p1,2);
		if (post(&p1, getppid())!=0) {
			printf("Post in child failed!\n");
			kill(getppid(), SIGINT);
			return -1;
		};
		kill(getppid(), SIGUSR1);

		bl=0;
		while (bl==0) sigsuspend(&mask);
		return 0;
	}

	printf("Testing blocking on retrieve:\n");
	fill_package(&p1,1);
	r = retrieve_wait(&p2, &sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
	else printf("OK\n");

	kill(chpid, SIGUSR1);
	bl=0;
	while (bl==0) sigsuspend(&mask);
	fill_package(&p1,2);
	printf("Testing blocking retrieve on nonempty mailbox:\n");
	r = retrieve_wait(&p2, &sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
	else printf("OK\n");


	printf("Testing blocking retrieve into NULL package:\n");
	r = retrieve_wait(NULL, &sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");
	
	printf("Testing blocking retrieve into incorrect package pointer:\n");
	r = retrieve_wait((package *)1, &sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");

	kill(chpid, SIGUSR1);
}
