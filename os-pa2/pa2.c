/**********************************************************************
 * Copyright (c) 2019-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;


/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;


/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)  //// 리소스 할당하는 함수. 리소스를 정상적으로 할당하면 true 리턴. 아니라면 false 리턴
{    //// 먼저 요청받은 순서대로 리소스를 할당. 우선순위 고려하지 않음.
	printf("acquire");
	struct resource *r = resources + resource_id;   /// 리소스 배열에서 요청받은 리소스 번호에 해당하는 리소스를 r에 넣음. 

	if (!r->owner) {   //// 현재 리소스를 가지고 있는 프로세스가 아무도 없을 때!
		/* This resource is not owned by any one. Take it! */
		r->owner = current;   /////해당하는 리소스의 오너를 현재 프로세스로 변경.
		return true;   /// 리소스 할당에 성공했으므로 ture를 반환하고 빠져나옴. 
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */ //////// 리소스가 이미 다른 프로세스에 잡혀있는 상태/ 
	current->status = PROCESS_WAIT;   ///현재 프로세스는 웨이트 상태/ 

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);   /// 현재 프로세스를 웨이트 큐의 끝으로 추가.

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run. /// 리소스를 먼저 할당한 후, 스케줄링하러 schedule 함수 호출
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;   /// 리소스 배열에서 해당하는 리소스 찾기

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}
 
static struct process *fifo_schedule(void) /// 다음에 실행할 프로세스를 지정하는 함수. 
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	//dump_status();   ///*************************** 덤프 출력

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) { /// current 프로세스가 wait 상태이거나 비어 있는(NULL) 경우, 다름 프로세스만 정하면 된당.
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) { // 아직 lifespan 안 끝난 경우, lifetime 끝날 때까지 current로 계속 진행함. 
		return current;   //// current 리턴. 
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {  /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
	}

	/* Return the next process to run */
	return next;   /// next 프로세스 지정하여 리턴. 
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	/**
	 * Implement your own SJF scheduler here.
	 */
	dump_status();
	struct process *next = NULL;
	if (!current || current->status == PROCESS_WAIT) { /// current 프로세스가 wait 상태이거나 비어 있는(NULL) 경우, 다름 프로세스만 정하면 된당.
		goto pick_next;
	}
	if (current->age < current->lifespan) { // 아직 lifespan 안 끝난 경우, lifetime 끝날 때까지 current로 계속 진행함. 
		return current;   //// current 리턴. 
	}
pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {  /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
		struct process *p;
		unsigned int shortest=100;
		list_for_each_entry(p, &readyqueue, list) {
			if(p->lifespan<shortest){
				shortest = p->lifespan;
				next = p;
			}
		}


		list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
		return next;
	}

	/* Return the next process to run */
	return NULL;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule,		 /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
void srtf_forked(struct process *process){
	process->prio = 5;
}



static struct process *srtf_schedule(void){
	//dump_status();
	struct process *next = NULL;
	if(!current||current->status==PROCESS_WAIT){
		goto pick_next;
	}
	if(current->age < current->lifespan){ /// 프로세스가 실행중인 상태라면, 
		struct process *p;
		int isfork = 0;
		list_for_each_entry(p, &readyqueue, list){  /// 레디 큐 순회
			if(p->prio>0){    //// 새로 포크된 프로세스가 있다면, 
				int remaintime = p->lifespan;    /// 새로 포크된 프로세스의 remaintime
				if((current->lifespan-current->age)>remaintime){
					list_move_tail(&current->list, &readyqueue);
					list_del_init(&p->list);
					return p;   
				}
				p->prio = 0; isfork=1;

			}
			//else return current; // 새로 포크된 프로세스가 없는 경우 current를 그대로 실행. 
		}
		if(isfork==0) return current;
		
		goto pick_next;
	}

pick_next:
if (!list_empty(&readyqueue)) {  /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
		struct process *p;
		unsigned int shortest=100;
		list_for_each_entry(p, &readyqueue, list) {
			p->prio = 0;
			if((p->lifespan-p->age)<shortest){
				shortest = p->lifespan-p->age;
				next = p;
			}
		}


		list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
		return next;
	}

	/* Return the next process to run */
	return NULL;
}




struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = srtf_schedule,
    .forked = srtf_forked,
	/* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
	/* Obviously, you should implement srtf_schedule() and attach it here */
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void){
dump_status();
struct process *next = NULL;
if(!current||current->status==PROCESS_WAIT){
	goto pick_next;
}
if(current->age < current->lifespan){
list_move_tail(&current->list , &readyqueue);
goto pick_next;
}

pick_next:
		if (!list_empty(&readyqueue)) { 
		//printf("들어옴\n"); /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
			struct process *p;
			//unsigned int pr=0;
			list_for_each_entry(p, &readyqueue, list) {
				//printf("%d\n", p->pid);
				//printf("%d %d\n", p->prio, p->prio_orig);
				next = p;
				break;
			}


			list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
			return next;
		}

return NULL;
}


 
struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule,/* Obviously, you should implement rr_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/

void prio_release(int resource_id)
{printf("release");
	struct resource *r = resources + resource_id;   
	assert(r->owner == current);

	r->owner = NULL;

	struct process *waiter = NULL;
	if (!list_empty(&r->waitqueue)) {
		struct process *p;
		unsigned int pr = 0;
		list_for_each_entry(p, &r->waitqueue, list){
			if((p->prio)>pr || pr == 0){
				pr = p->prio;
				waiter = p;
			}
		}
		
		assert(waiter->status == PROCESS_WAIT);

		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

static struct process *prio_schedule(void){
	
struct process *next = NULL;
if(!current||current->status==PROCESS_WAIT){
	goto pick_next;
}
if(current->age < current->lifespan){
list_move_tail(&current->list , &readyqueue);
goto pick_next;
}

pick_next:
		if (!list_empty(&readyqueue)) { 
		//printf("들어옴\n"); /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
			struct process *p;
			unsigned int pr=0;
			list_for_each_entry(p, &readyqueue, list) {
				//printf("%d\n", p->pid);
				//printf("%d %d\n", p->prio, p->prio_orig);
				if((p->prio_orig)>pr || pr ==0){
					//printf("우선순위 확인\n");
					pr = p->prio_orig;
					next = p;
				}
			}


			list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
			return next;
		}

return NULL;
}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = prio_release, /* Use the default FCFS release() */
	.schedule = prio_schedule,
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/


static struct process *pa_schedule(void){
//printf("schedule");
dump_status();
//printf("priority : %d of %d\n", current->prio, current->pid);
struct process *next = NULL;
if(!current||current->status==PROCESS_WAIT){
	goto pick_next;
}
if(current->age < current->lifespan){
list_move_tail(&current->list , &readyqueue);
goto pick_next;
}

pick_next:
		if (!list_empty(&readyqueue)) { 
		//printf("들어옴\n"); /// 레디 큐가 비어있지 않으면, 레디큐의 첫번째 entry를 next 프로세스로 지정. 
			struct process *p;
			unsigned int pr=0;
			list_for_each_entry(p, &readyqueue, list) {
				//printf("%d\n", p->pid);
				//printf("%d %d\n", p->prio, p->prio_orig);
				if((p->prio)>pr||pr==0){
					//printf("우선순위 확인\n");
					pr = p->prio;
					next = p;
				}
			}
			list_del_init(&next->list);  /// 다음 프로세스로 지정한 프로세스는 레디 큐에서 제외. 
			list_for_each_entry(p, &readyqueue, list){
				p->prio++;
			}
			next->prio = next->prio_orig;
			return next;
		}

return NULL;
}





//-----------------------------------------------------------------------------------------------------------------
struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = prio_release, /* Use the default FCFS release() */
	.schedule = pa_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
bool pcp_acquire(int resource_id) {  	
	printf("acquire");
	struct resource *r = resources + resource_id;  
	if (!r->owner) {  
		r->owner = current; 
		current->prio = MAX_PRIO;
		return true; 
	}
	current->status = PROCESS_WAIT; 
	list_add_tail(&current->list, &r->waitqueue);
	return false;
}

void pcp_release(int resource_id)
{	struct resource *r = resources + resource_id;   
	assert(r->owner == current);
	r->owner->prio = r->owner->prio_orig;
	r->owner = NULL;

	struct process *waiter = NULL;
	if (!list_empty(&r->waitqueue)) {
		struct process *p;
		unsigned int pr = 0;
		list_for_each_entry(p, &r->waitqueue, list){
			if((p->prio)>pr || pr == 0){
				pr = p->prio;
				waiter = p;
			}
		}
		
		assert(waiter->status == PROCESS_WAIT);

		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}

}


static struct process *pcp_schedule(void){
//dump_status();
printf("schedule");
struct process *next = NULL;
if(!current||current->status==PROCESS_WAIT){
	goto pick_next;
}
if(current->age < current->lifespan){
list_move_tail(&current->list , &readyqueue);
goto pick_next;
}
pick_next:
		if (!list_empty(&readyqueue)) { 
			struct process *p;
			unsigned int pr=0;
			int start = 1;

			list_for_each_entry(p, &readyqueue, list) {
				if(p->prio>pr){
					pr = p->prio;
					next = p;
				}
				if(start == 1&& pr==0){
					start = 0;
					pr = p->prio;
					next = p;
				}


			}
			list_del_init(&next->list);
			return next;
		}

return NULL;
}


struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire, /* Use the default FCFS acquire() */
	.release = pcp_release, /* Use the default FCFS release() */
	.schedule = pcp_schedule,
	
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/

bool pip_acquire(int resource_id) {  	
	//printf("acquire");
	struct resource *r = resources + resource_id;  
	if (!r->owner) {   /// 비어있으면 그냥 잡으면 된다. 
		r->owner = current; 
		return true; 
	}
	else{ //// contending이 발생하는 경우, high priority process의 priority를 inheritance해줘야한다. 
		current->status = PROCESS_WAIT; 
		list_add_tail(&current->list, &r->waitqueue);

		if(!list_empty(&r->waitqueue)){
			struct process *p;
			unsigned int pr = r->owner->prio;
			int start = 1;
			list_for_each_entry(p, &r->waitqueue, list){
				if(p->prio >= pr){
				pr = p->prio;
				}
				if(start ==1 && pr == 0){
				start = 0;
				pr = p->prio;
				}
				//printf("\n@@@@ priority = %d", pr);
			}
			r->owner->prio = pr;
			return false;
		}
		
	}
	return false;

}

void pip_release(int resource_id)
{	//printf("release");
	struct resource *r = resources + resource_id;   
	//assert(r->owner == current);
	r->owner->prio = r->owner->prio_orig;
	r->owner->status = PROCESS_READY;
	r->owner = NULL;
	struct process *waiter = NULL;
	if (!list_empty(&r->waitqueue)) {
		struct process *p;
		unsigned int pr = 0;
		list_for_each_entry(p, &r->waitqueue, list){
			if((p->prio)>pr || pr == 0){
				pr = p->prio;
				waiter = p;
			}
		}
		//assert(waiter->status == PROCESS_WAIT);
		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}


static struct process *pip_schedule(void){
dump_status();
////printf("schedule");
struct process *next = NULL;
if(!current||current->status==PROCESS_WAIT){
	goto pick_next;
}
if(current->age < current->lifespan){
list_move_tail(&current->list , &readyqueue);
goto pick_next;
}
pick_next:
		if (!list_empty(&readyqueue)) { 
			struct process *p;
			unsigned int pr=0;
			int start = 1;

			list_for_each_entry(p, &readyqueue, list) {
				if(p->prio>pr){
					pr = p->prio;
					next = p;
				}
				if(start == 1&& pr==0){
					start = 0;
					pr = p->prio;
					next = p;
				}


			}
			list_del_init(&next->list);
			return next;
		}

return NULL;
}





struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire, /* Use the default FCFS acquire() */
	.release = pip_release, /* Use the default FCFS release() */
	.schedule = pip_schedule,
};
