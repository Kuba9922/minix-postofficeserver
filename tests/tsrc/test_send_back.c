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
		if (post(&p1, getppid())!=0) {
			printf("Post in child failed!\n");
			kill(getppid(), SIGINT);
			return -1;
		};
		kill(getppid(), SIGUSR1);
		// wait for permission to retrieve
		bl=0;
		while (bl==0) sigsuspend(&mask);

		r = retrieve(&p2, &sender);
		if (r!=0) fail("");
		else if (sender!=getpid()) {
			printf("sender should be %d but is %d.\n", getpid(), sender);
			fail("Incorrect sender value.");
		}
		else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
		else printf(".package delivered OK\n");
		
		// succesfull receive confirmation
		kill(getppid(), SIGUSR1);

		// wait for the next test
		bl=0;
		while (bl==0) sigsuspend(&mask);

		if (post(&p1, getppid())!=0) {
			fail("Post in child failed!\n");
		};
		kill(getppid(), SIGUSR1);
		bl=0;
		while (bl==0) sigsuspend(&mask);
		
		return 0;
	}

	// parent waits for package sending confirmation
	bl=0;
	while (bl==0) sigsuspend(&mask);

	// package should be in the mailbox
	printf("Testing send_back:\n");
	r = send_back();
	if (r!=0) fail("");
	else printf(".sending back OK\n");
	// mailbox should be empty now
	r=check(&sender);
	if (r!=-1) fail("mailbox not empty after send_back");
	printf(".mailbox empty OK\n");
	// allow child to receive
	kill(chpid, SIGUSR1);
	// wait for receive confirmation
	bl=0;
	while (bl==0) sigsuspend(&mask);
	printf("OK\n");

	printf("Testing send_back into nonempty mailbox:\n");
	r = post(&p1, chpid);
	if (r!=0) fail("post failed.");
	// allow child to send
	kill(chpid, SIGUSR1);
	// waiting for confirmation of package sending from the child
	bl=0;
	while (bl==0) sigsuspend(&mask);
	r = send_back();
	if (r!=-1) fail("send_back to nonempty mailbox succeeded.");
	if (errno!=EBUSY) fail("Incorrect errno value.");
	// mailbox should still contain the package from the child
	r=check(&sender);
	if (r!=0) fail("mailbox empty after failed send_back.");
	else if (sender!=chpid) fail("Incorrect sender value.");
	printf(".mailbox nonempty OK\n");
	printf("OK\n");

	kill(chpid, SIGUSR1);
}
