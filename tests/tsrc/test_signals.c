#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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
	pid_t sender, fchild;

	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);

	setsid();

	chpid = fork();

	if (chpid == -1) return -1;
	if (chpid == 0 ) {
		r = retrieve_wait(&p1,&sender);
		if (r!=-1) fail("Invalid return value.");
		else if (errno!=EINTR) {
			printf("errno %d\n", errno); 
			fail("Invalid errno value.");
		}
		else if (bl!=1) fail("Handler failed to run.");
		bl=0;
		kill(getppid(), SIGUSR1);
		r = retrieve_wait(&p1,&sender);
		if (r!=0) fail("Invalid return value.");
		
		r = retrieve_wait(&p1,&sender);
		if (r!=-1) fail("Invalid return value.");
		else if (errno!=EINTR) fail("Invalid errno value.");
		else if (bl!=1) fail("Handler failed to run.");
		return 0;
	}
	
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	sigemptyset(&mask);

	printf("Testing signal handling:\n");
	fill_package(&p1,0);
	sleep(1);
	kill(chpid, SIGUSR1);
	while (bl==0) sigsuspend(&mask);
	r = post(&p1, chpid);
	if (r!=0) fail("Post failed.");
	sleep(1);
	kill(chpid, SIGUSR1);
	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Unexpeced waitpid result.");
	if (!WIFEXITED(r)) fail("Child did not finish by exit.");
	if (WEXITSTATUS(r)!=0) fail("Incorrect child exit status.");
	printf("OK\n");
}
