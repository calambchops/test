// services.c, 159

#include "spede.h"
#include "kernel_types.h"
#include "kernel_data.h"
#include "services.h"
#include "tools.h"
#include "proc.h"

int tick_count = 0;
// to create process, alloc PID, PCB, and process tack
// build trapframe, initialize PCB, record PID to ready_pid_q (unless 0)



void SemwaitService(int sem_num) {
      if(sem_num==STDOUT)
	{
	  if(video_sem.val>0) //use & for video_sem?
	     video_sem.val--;	
	  else
	  {
	     EnQ(run_pid, &video_sem.wait_q); 
	     pcb[run_pid].state = SLEEP;
	     run_pid = -1;
	  }
	}
         
      else
      cons_printf("Kernel Panic: non-such semaphore number!");
   }


void SempostService(int sem_num) {
      if(sem_num==STDOUT)
	{
	  if(video_sem.wait_q.size == 0) //maybe 0?	
	     video_sem.val++;
	  else
	  {
	      run_pid = DeQ(&video_sem.wait_q); //one parameter
	      pcb[run_pid].state = READY; //ready
	      EnQ(run_pid, &ready_pid_q);
	  }	

	}
      else
      cons_printf("Kernel Panic: non-such semaphore number!");
}

void GetpidService(int *p) {
  //use ebx for currently-running PID
  //*p = run_pid;
pcb[run_pid].trapframe_p->ebx = run_pid;
}

void SleepService(int centi_sec) {
  pcb[run_pid].wake_time = current_time + (100 * centi_sec);
  pcb[run_pid].state = SLEEP;
  run_pid = -1;
}

// count runtime of process and preempt it when reaching time limit
void TimerService(void) {
   int i;
   current_time++;
   
   for(i=0; i<PROC_NUM; i++) {
      if((pcb[i].state == SLEEP) && (pcb[i].wake_time == current_time)) {
        EnQ(i, &ready_pid_q);
        pcb[i].state = READY;
      }
   }

   outportb(0x20, 0x60);
 //  tick_count++;
 //  if(tick_count == 75) {
 //     cons_printf(". ");
 //     tick_count = 0;
 //  }

   if(run_pid == 0) {
      return;
   }
   pcb[run_pid].runtime++;
   if(pcb[run_pid].runtime == TIME_LIMIT) {
      pcb[run_pid].state = READY;
      EnQ(run_pid, &ready_pid_q);
      run_pid = -1;
   }
}

void WriteService(int fileno, char *str, int len) {
  int i, j;
  static unsigned short *vga_p = (unsigned short *)0xb8000;   // top-left
  if(fileno == STDOUT) {
    for(i=0; i<strlen(str); i++) {
      *vga_p = str[i] + 0xf00;
      vga_p++;
      if(vga_p >= (unsigned short *)0xb8000 + 25*80) {  // bottom-right
        // erase the whole screen
        vga_p = (unsigned short *)0xb8000;       // roll back to top-left
        for(j=0xb8000; j<0xb8000+25*80; j++) {
          vga_p[j] = ' ';
        }
        vga_p = (unsigned short *)0xb8000;
      }
    }
  }
}


void NewProcService(func_p_t proc_p) {//arg: where process code starts
   int pid;
  
   if(avail_pid_q.size == 0) {
      cons_printf("Kernel Panic: no more process!\n");
      return;
   }
   pid = DeQ(&avail_pid_q);
   MyBzero((char *)&pcb[pid], sizeof(pcb_t));
   MyBzero((char *)&proc_stack[pid][0], PROC_STACK_SIZE);
//   MyBzero((char *)&proc_stack[pid], sizeof(proc_stack[pid]));//PROC_STACK_SIZE);
   pcb[pid].state = READY;
   if(pid != 0) {
      EnQ(pid,&ready_pid_q);
   }

   pcb[pid].trapframe_p = (trapframe_t *)&proc_stack[pid][PROC_STACK_SIZE - sizeof(trapframe_t)];
   pcb[pid].trapframe_p->efl= EF_DEFAULT_VALUE | EF_INTR;
   pcb[pid].trapframe_p->eip =(int)proc_p;
   pcb[pid].trapframe_p->cs = get_cs();
}



void SyscallService(trapframe_t *p) 
{
  switch(p->eax) {
    case SYS_GETPID:
      GetpidService( (int *)p->ebx);
     //   GetpidService((char *)p->ebx);
      break;
    case SYS_SLEEP:
      SleepService(p->ebx);
      break;
    case SYS_WRITE:
      WriteService(p->ebx, (char *)p->ecx, p->edx);
      break;
    case SYS_SEMPOST:
      SempostService(p->ebx); 
      break;
    case SYS_SEMWAIT:
      SemwaitService(p->ebx); 
      break;
    default:
      break;
  }
}

