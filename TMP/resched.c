/* resched.c  -  resched */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lab1.h>

int scheduler_type = (int)LINUXSCHED;

void setschedclass(int sched_class){
	scheduler_type = sched_class;
}

int getschedclass(){
	return scheduler_type;
}

#define RECORD_PREVIOUS 0
#define CALC_QUANTUM    1
#define MAX_GOODNESS    2
#define CURR_PROC       3
#define CONTEXT_SWITCH  4
#define SWITCH_NULL     5
#define RETURN_OK       6


unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);

/*-----------------------------------------------------------------------
 *  * resched  --  reschedule processor to highest priority ready process
 *   *
 *    * Notes:	Upon entry, currpid gives current process id.
 *     *		Proctab[currpid].pstate gives correct NEXT state for
 *      *			current process if other than PRREADY.
 *       *------------------------------------------------------------------------
 *        */
int resched()
{
	if(getschedclass() == AGESCHED)    /* Aging Scheduler */
        {
		register struct	pentry	*optr;	/* pointer to old process entry */
	        register struct	pentry	*nptr;	/* pointer to new process entry */

		int proc_id = 0;
		
		for(proc_id = rdyhead; proc_id != rdytail; proc_id = q[proc_id].qnext)
		{
			if(q[proc_id].qkey >0) q[proc_id].qkey = q[proc_id].qkey + 1;  /* incrementing key value for processes in ready queue */

		}

		for(proc_id = 1; proc_id < NPROC; proc_id++)      /* incrementing priority for processes in ready queue */
		{
			if((&proctab[proc_id])->pstate == PRREADY) (&proctab[proc_id])->pprio = (&proctab[proc_id])->pprio + 1;

		}

		/* no switch needed if current process priority higher than next*/

		if ( ( (optr = &proctab[currpid])->pstate == PRCURR) &&
	   		(lastkey(rdytail)<optr->pprio)) {
			return(OK);
		}

		/* force context switch */

		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}

		/* remove highest priority process at end of ready list */

		nptr = &proctab[ (currpid = getlast(rdytail)) ];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
		#ifdef	RTCLOCK
			preempt = QUANTUM;		/* reset preemption counter	*/
		#endif

		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);

		/* The OLD process returns here when resumed. */
		return OK;
	}

	else if(getschedclass() == LINUXSCHED)    /*Linux like Scheduler */
        {
        	register struct	pentry	*optr;	/* pointer to old process entry */
		register struct	pentry	*nptr;	/* pointer to new process entry */
        
		int beginEpoch = 0;
		int sched_prog = RECORD_PREVIOUS;
		int pCount = 0;
		int goodness = 0;
		int max_goodness_pid = 0;

		while(sched_prog != RETURN_OK)
 		{

                	if(sched_prog == RECORD_PREVIOUS) 
			{
                    		optr = &proctab[currpid];
                    		int pprio = optr->goodness - optr->counter;
                    		optr->counter = preempt;

                   		if (optr->counter == 0) optr->goodness = 0;
                    		else optr->goodness = pprio + preempt;
			    	sched_prog = MAX_GOODNESS;
			}

			else if(sched_prog == CALC_QUANTUM) 
 			{
			       if (beginEpoch == 0) 
			       {
			       		beginEpoch = 1;   /* starting epoch */
					for (pCount = 0; pCount < NPROC; pCount++) 
					{
				    		if ((&proctab[pCount])->pstate != PRFREE) 
						{
							(&proctab[pCount])->quantum = (&proctab[pCount])->counter / 2 + (&proctab[pCount])->pprio;
							(&proctab[pCount])->goodness = (&proctab[pCount])->counter + (&proctab[pCount])->pprio;
				   		 }
					}
					preempt = optr->counter;
			        }
			    	sched_prog = RECORD_PREVIOUS;
			}
			
			else if(sched_prog == MAX_GOODNESS)
			{
			   	 for (pCount = 0 ; pCount < NPROC ; pCount++)
				 {
					if(((&proctab[pCount])->goodness > goodness) && (&proctab[pCount])->pstate == PRREADY)
					{
				    		goodness = (&proctab[pCount])->goodness;
				    		max_goodness_pid = pCount;
					}
			    	 }
		
			    	 if((optr->pstate != PRCURR || optr->counter == 0) && goodness == 0 && beginEpoch == 0) sched_prog=CALC_QUANTUM;
				 
				 else if(goodness == 0) sched_prog = SWITCH_NULL;
			         
				 else if(optr->pstate == PRCURR && optr->goodness >= goodness && optr->goodness > 0) 
				 {
				 	preempt = optr->counter;
			         	sched_prog = RETURN_OK;
			    	 }
			    
				else if(goodness > 0 && (optr->pstate != PRCURR || optr->counter == 0 || optr->goodness < goodness))
				{
					sched_prog = CONTEXT_SWITCH;
			    	} 
			}

			else if(sched_prog == SWITCH_NULL)
			{
				if (currpid != NULLPROC) 
				{
					max_goodness_pid = NULLPROC;
					sched_prog = CONTEXT_SWITCH;
			    	}
				else sched_prog = RETURN_OK;
			}

			else if(sched_prog == CONTEXT_SWITCH)              /* force context switch */  
			{
			    
			        if (optr->pstate == PRCURR) 
  				{
					optr->pstate = PRREADY;
					insert(currpid,rdyhead,optr->pprio);
			   	}

			    	/* remove highest priority process at end of ready list */
			    	nptr = &proctab[currpid = max_goodness_pid];
			    	dequeue(max_goodness_pid);
			    	nptr->pstate = PRCURR;		/* mark it currently running	*/

			    	#ifdef	RTCLOCK
			   	preempt = nptr->quantum;		
			    	if(currpid == NULLPROC)   preempt = QUANTUM;		/* reset preemption counter	*/
			    	#endif

			    	ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
			    	sched_prog = RETURN_OK;
			} 

        	}
		
		return OK;

	}
}

