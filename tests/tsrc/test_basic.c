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
	kill(0, SIGKILL);
	exit(1); // should never happen
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
	setsid();

	chpid = fork();

	if (chpid == -1) return -1;
	if (chpid == 0 ) {
		int c=0;
		fill_package(&p1,1);
		while (c<2){
			bl=0;
			while (bl==0) sigsuspend(&mask);
			r = post(&p1, getppid())!=0;
			if (r!=0) {
				printf("Post in child failed!\n");
				kill(getppid(), SIGINT);
				return -1;
			};
			kill(getppid(), SIGUSR1);
			c++;
		};
		bl=0;
		while (bl==0) sigsuspend(&mask);
		r = retrieve(&p2, &sender);
		kill(getppid(), SIGUSR1);
		bl=0;
		while (bl==0) sigsuspend(&mask);
		r = post(&p1, getppid())!=0;
		kill(getppid(), SIGUSR1);
		bl=0;
		while (bl==0) sigsuspend(&mask);
		return 0;
	}

	printf("Testing post:\n");
	fill_package(&p1,0);
	r = post(&p1, chpid);
	if (r!=0) fail("");
	else printf("OK\n");

	printf("Testing retrieve:\n");
	fill_package(&p1,1);
	kill(chpid, SIGUSR1);
	bl=0;
	while (bl==0) sigsuspend(&mask);
	r = retrieve(&p2, &sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
	else printf("OK\n");

	memset(&p2, 0, sizeof(package));
	printf("Testing check:\n");
	kill(chpid, SIGUSR1);
	bl=0;
	while (bl==0) sigsuspend(&mask);
	r = check(&sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	printf("OK, retrieving:");
	r = retrieve(&p2, &sender);
	if (r!=0) fail("");
	else if (sender!=chpid) fail("Incorrect sender value.");
	else if (memcmp(&p1, &p2, sizeof(package))!=0) fail("Incorrect package content.");
	else printf("OK\n");

	printf("Testing post to nonempty mailbox:\n");
	r = post(&p1, chpid);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EBUSY) fail("Invalid errno value.");
	else printf("OK\n");

	printf("Testing post to nonexistant process:\n");
	r = post(&p1, -1);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=ESRCH) fail("Invalid errno value.");
	else printf("OK\n");
	
	kill(chpid, SIGUSR1); // to force retrieve in child
	bl=0;
	while (bl==0) sigsuspend(&mask);
	
	printf("Testing post with NULL message:\n");
	r = post(NULL, chpid);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");
	
	printf("Testing post with incorrect package pointer:\n");
	r = post((package *)1, chpid);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");
	
	printf("Testing retrieve from empty mailbox:\n");
	r = retrieve(&p2, &sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=ENOMSG) fail("Invalid errno value.");
	else printf("OK\n");

	kill(chpid, SIGUSR1); // to force post in child
	bl=0;
	while (bl==0) sigsuspend(&mask);

	printf("Testing retrieve package into NULL:\n");
	r = retrieve(NULL, &sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");
	
	printf("Testing retrieve into incorrect package pointer:\n");
	r = retrieve((package *)1, &sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=EINVAL) fail("Invalid errno value.");
	else printf("OK\n");

	retrieve(&p2, &sender);	
	printf("Testing check on empty mailbox:\n");
	r = check(&sender);
	if (r!=-1) fail("Invalid return value.");
	else if (errno!=ENOMSG) fail("Invalid errno value.");
	else printf("OK\n");

	kill(chpid, SIGUSR1); // to finish child
}
