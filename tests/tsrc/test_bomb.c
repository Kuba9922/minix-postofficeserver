#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <po.h>

#define PROCSN 10

pid_t pids[PROCSN+1];

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
	pid_t chpid, sender, fchild;

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
	
	printf("Testing simple bomb: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		r = retrieve_wait(&p1,&sender);
		fail("Bomb survived.");
		return 1; // never happens
	}
	r = send_bomb(chpid,1000000);
	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");
	

	printf("Testing timed bomb: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		sleep(1);
		return 0;
	}
	r = send_bomb(chpid,1000);

	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");


	printf("Testing send_back bomb: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		send_bomb(getppid(),1000000);
		kill(getppid(),SIGUSR1);
		sleep(2);
		printf("child exits happily\n");
		return 0;
	}
	bl=0;
	while (bl==0) sigsuspend(&mask);
	r = send_back();
	if (r == -1) fail("Send back failed.");

	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) {printf("exit code: %d\n", r); fail("Incorrect finish process status."); }
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");
	

	printf("Testing forward bomb: \n");
	pid_t child1, child2;

	child1 = fork();
	if (child1==-1) fail("Fork failed.");
	if (child1==0){
		bl=0;
		while (bl==0) sigsuspend(&mask);
		r = forward(getppid());
		kill(getppid(), SIGUSR1);
		return 0;
	}
	child2 = fork();
	if (child2==-1) fail("Fork failed.");
	if (child2==0){
		bl=0;
		while (bl==0) sigsuspend(&mask);
	
		r = forward(child1);
		if (r!=0) fail("Forward failed.");
		kill(child1, SIGUSR1);
		pause();
		return 0;
	}	
	r = send_bomb(child2, 100000);
	if (r!=0) fail("Sending bomb failed.");
	kill(child2, SIGUSR1);
	bl=0;
	while (bl==0) sigsuspend(&mask);
	fchild = waitpid(-1, &r, 0);
	if (fchild!=child1) fail("Incorrect child 1 waitpid result.");
	if (!WIFEXITED(r)) fail("Incorrect finish process status.");
	r = forward(child2);
	if (r!=0) fail("Forward failed.");
	fchild = waitpid(-1, &r, 0);
	if (fchild!=child2) fail("Incorrect child 2 waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");


	printf("Testing negative timer bomb: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		send_bomb(getppid(),-1);
		sleep(1);
		return 0;
	}
	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");
	

	printf("Testing bomb to nonempty mailbox: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		r = post(&p1,getppid());
		if (r!=0) fail("Post failed.");
		r = send_bomb(getppid(),100000);
		if (r!=0) fail("Bomb to nonempty mailbox succeeded.");
		// the bomb gets to sender mailbox and will blow on timer
		sleep(1);
		return 0;
	}
	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");


	printf("Testing bomb to nonempty mailbox with nonempty mailbox of the sender: \n");
	chpid = fork();
	if (chpid==-1) fail("Fork failed.");
	if (chpid==0){
		bl=0;
		while (bl==0) sigsuspend(&mask);
		r = send_bomb(getppid(),10000000);
		if (r!=0) fail("Bomb to nonempty mailbox succeeded.");
		// sender mailbox should be occupied so the bomb should blow up now
		sleep(1);
		return 0;
	}
	post(&p1, chpid);
	kill(chpid, SIGUSR1);

	fchild = waitpid(-1, &r, 0);
	if (fchild!=chpid) fail("Incorrect waitpid result.");
	if (!WIFSIGNALED(r)) fail("Incorrect finish process status.");
	if (!WTERMSIG(r)) fail("Killed by incorrect signal.");
	printf("OK\n");

	return 0;
}
