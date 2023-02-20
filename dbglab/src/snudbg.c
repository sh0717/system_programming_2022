#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <sys/personality.h>

#include "snudbg.h"
#include "procmaps.h"
const char* prompt= ">>> ";
int num_bps = 0;
breakpoint_t bps[MAX_BPS];

/* HINT: No need to change this function */
void die(char* message) {
    WARN("Failed with message: '%s'\n", message);
    exit(-1);
}

/* HINT: No need to change this function */
void handle_regs(struct user_regs_struct *regs) {
    fprintf(stdout, "\t");
    PRINT_REG(rax);
    PRINT_REG(rbx);
    PRINT_REG(rcx);
    PRINT_REG(rdx);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(rbp);
    PRINT_REG(rsp);
    PRINT_REG(rsi);
    PRINT_REG(rdi);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(r8);
    PRINT_REG(r9);
    PRINT_REG(r10);
    PRINT_REG(r11);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(r12);
    PRINT_REG(r13);
    PRINT_REG(r14);
    PRINT_REG(r15);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(rip);
    PRINT_REG(eflags);
    fprintf(stdout, "\n");
}

/* HINT: No need to change this function */
void no_aslr(void) {
    unsigned long pv = PER_LINUX | ADDR_NO_RANDOMIZE;

    if (personality(pv) < 0) {
        if (personality(pv) < 0) {
            die("Failed to disable ASLR");
        }
    }
    return;
}

/* HINT: No need to change this function */
void tracee(char* cmd[]) {
    LOG("Tracee with pid=%d\n", getpid());

    no_aslr();
    
    if(ptrace(PTRACE_TRACEME, NULL, NULL, NULL)<0){
        die("Error traceing myself");
    }

    LOG("Loading the executable [%s]\n", cmd[0]);
    execvp(cmd[0], cmd);
}

/* INSTRUCTION: YOU SHOULD NOT CHANGE THIS FUNCTION */    
void dump_addr_in_hex(const ADDR_T addr, const void* data, size_t size) {
    uint i;
    for (i=0; i<size/16; i++) {
        printf("\t %llx ", addr+(i*16));
        for (uint j=0; j<16; j++) {
            printf("%02x ", ((unsigned char*)data)[i*16+j]);
        }
        printf("\n");
    }

    if (size%16 != 0) {
        // the rest
        printf("\t %llx ", addr+(i*16));
        for (uint j=0; j<size%16; j++) {
            printf("%02x ", ((unsigned char*)data)[i*16+j]);
        }
        printf("\n");
    }
}

/* HINT: No need to change this function */
void handle_help(void) {
    LOG("Available commands: \n");
    LOG("\t regs | get [REG] | set [REG] [value]\n");
    LOG("\t read [addr] [size] | write [addr] [value] [size]\n");
    LOG("\t step | continue | break [addr]\n");
    LOG("\t help\n");
    return;
}


void set_debug_state(int pid, enum debugging_state state) {
    if(state == SINGLE_STEP) {
        if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL)<0) {
            die("Error tracing syscalls");
        }
    } else if (state == NON_STOP) {
        ptrace(PTRACE_CONT,pid,NULL,NULL);
        // TODO
    }
    return;
}


/* 
   Read the memory from @pid at the address @addr with the length @len.
   The data read from @pid will be written to @buf.
*/
void handle_read(int pid, ADDR_T addr, unsigned char *buf, size_t len) {
    // TODO: Use the function dump_addr_in_hex() to print the memory data
    unsigned long data=0;
    unsigned char* ptr;
    int LONG_size=sizeof(long);
    int i=len/LONG_size;
    for(int j=1;j<=i;j++){
        data=ptrace(PTRACE_PEEKTEXT, pid, addr+(j-1)*LONG_size,NULL);
        ptr=(unsigned char*)(&data);

        for(int w=0;w<LONG_size;w++){
            buf[(j-1)*LONG_size+w]=*(ptr+w);
        }
    }
    
    int rest=len%LONG_size;
    data=ptrace(PTRACE_PEEKTEXT,pid,addr+(i)*LONG_size,NULL);
    ptr=(unsigned char*)(&data);
    int k=i*LONG_size;
    while(rest>0){
        buf[k]=*ptr;
        ptr++;
        k++;
        rest=rest-1;
    }
    
    /*dump_addr_in is printer*/
    dump_addr_in_hex(addr,buf,len);
    
    
    TODO_UNUSED(pid);
    TODO_UNUSED(addr);
    TODO_UNUSED(buf);
    TODO_UNUSED(len);
    
    return;
}

/* 
   Write the memory to @pid at the address @addr with the length @len.
   The data to be written is placed in @buf.
*/
void handle_write(int pid, ADDR_T addr, unsigned char *buf, size_t len) {
    // TODO
    unsigned long* ptr_data=(unsigned long *)(buf);
    int LONG_size=sizeof(long);
    int i=len/LONG_size;
    for(int j=1;j<=i;j++){
        ptrace(PTRACE_POKEDATA,pid,addr+(j-1)*LONG_size, (void*)(*(ptr_data+(j-1))));
    }
    int rest=len%LONG_size;
    unsigned long data=ptrace(PTRACE_PEEKTEXT,pid,addr+i*LONG_size,NULL);
    unsigned char* ptr=(unsigned char*)(&data);
    unsigned char* ptr2=ptr;
    int k=i*LONG_size;
    while(rest>0){
        *ptr2=buf[k];
        ptr2++;
        k++;
        rest--;
    }
    ptrace(PTRACE_POKEDATA,pid,addr+i*LONG_size,(void*)data);
    
    ////
    TODO_UNUSED(pid);
    TODO_UNUSED(addr);
    TODO_UNUSED(buf);
    TODO_UNUSED(len);
    return;
}

/* 
   Install the software breakpoint at @addr to pid @pid.
*/
void handle_break(int pid, ADDR_T addr) {
    // TODO
    printf("%s [*] HANDLE CMD: break [%llx]\n",prompt,addr);

    unsigned char CC[1];
    CC[0]=0xcc;
    
    unsigned int data;
    unsigned char* ptr;
    data=ptrace(PTRACE_PEEKTEXT,pid,addr,NULL);
    ptr=(unsigned char*)(&data);

    if(num_bps<MAX_BPS){
    bps[num_bps].addr=addr;
    bps[num_bps++].orig_value=*ptr;
    /*store origin value*/

    handle_write(pid,addr,CC,1);
    /*change to CC whic make exception*/
    }
    else{
        printf("we cannot make more breakpoint sir!!\n");

    }
    TODO_UNUSED(pid);
    TODO_UNUSED(addr);
}

#define CMPGET_REG(REG_TO_CMP)                   \
    if (strcmp(reg_name, #REG_TO_CMP)==0) {      \
        printf("\t");                            \
        PRINT_REG(REG_TO_CMP);                   \
        printf("\n");                            \
    }

/* HINT: No need to change this function */
void handle_get(char *reg_name, struct user_regs_struct *regs) {
    CMPGET_REG(rax); CMPGET_REG(rbx); CMPGET_REG(rcx); CMPGET_REG(rdx);
    CMPGET_REG(rbp); CMPGET_REG(rsp); CMPGET_REG(rsi); CMPGET_REG(rdi);
    CMPGET_REG(r8);  CMPGET_REG(r9);  CMPGET_REG(r10); CMPGET_REG(r11);
    CMPGET_REG(r12); CMPGET_REG(r13); CMPGET_REG(r14); CMPGET_REG(r15);
    CMPGET_REG(rip); CMPGET_REG(eflags);
    return;
}

#define CMPSET_REG(REG_TO_CMP)             \
    if(strcmp(reg_name, #REG_TO_CMP)==0){  \
    regs->REG_TO_CMP=value;                \
    }                                      \
                                                



/*
  Set the register @reg_name with the value @value.
  @regs is assumed to be holding the current register values of @pid.
*/
void handle_set(char *reg_name, unsigned long value,
                struct user_regs_struct *regs, int pid) {
    // TODO
    CMPSET_REG(rax); CMPSET_REG(rbx); CMPSET_REG(rcx); CMPSET_REG(rdx);
    CMPSET_REG(rbp); CMPSET_REG(rsp); CMPSET_REG(rsi); CMPSET_REG(rdi);
    CMPSET_REG(r8);  CMPSET_REG(r9);  CMPSET_REG(r10); CMPSET_REG(r11);
    CMPSET_REG(r12); CMPSET_REG(r13); CMPSET_REG(r14); CMPSET_REG(r15);
    CMPSET_REG(rip); CMPSET_REG(eflags);
    /*change regs value in regs and set_register*/
    /*          */
    set_registers(pid,regs);


    TODO_UNUSED(reg_name);
    TODO_UNUSED(value);
    TODO_UNUSED(regs);
    TODO_UNUSED(pid);
    return;
}

void prompt_user(int child_pid, struct user_regs_struct *regs,
                 ADDR_T baseaddr) {

    TODO_UNUSED(child_pid);
    TODO_UNUSED(baseaddr);

    const char* prompt_symbol = ">>> ";

    for(;;) {
        fprintf(stdout, "%s", prompt_symbol);
        char action[1024];
        scanf("%1024s", action);

        if(strcmp("regs", action)==0) {
            printf("%s [*] HANDLE CMD: regs\n",prompt);
            handle_regs(regs);
            continue;
        }

        if(strcmp("help", action)==0 || strcmp("h", action)==0) {
            handle_help();
            continue;
        }

        if(strcmp("get", action)==0) {
            scanf("%1024s",action);
            printf("%s [*] HANDLE CMD: get [%s]\n",prompt, action);

            handle_get(action,regs);
            continue;
            // TODO
        }

        if(strcmp("set", action)==0) {
            scanf("%1024s",action);
            unsigned long value=0;
            scanf("%lx",&value);
            printf("%s [*] HANDLE CMD: set [%s] [0x%lx]\n",prompt, action,value);
            handle_set(action,value,regs,(int)(child_pid));

            continue;
            // TODO
        }

        if(strcmp("read", action)==0 || strcmp("r", action)==0) {
            ADDR_T addr;
            size_t len;

            scanf("%llx",&addr);
            scanf("%lx",&len);
            
            printf("%s [*] HANDLE CMD: read [%llx] [%llx] [%lx]\n",prompt,addr,addr+(ADDR_T)baseaddr,len);

            unsigned char* buf=(unsigned char*)malloc(len);
            handle_read((int)(child_pid),addr+(ADDR_T)baseaddr,buf,len);
            free(buf);

            continue;
            // TODO
        }

        if(strcmp("write", action)==0 || strcmp("w", action)==0) {
             ADDR_T addr;
            unsigned long long value;
            size_t len;
            
            scanf("%llx",&addr);
            scanf("%llx",&value);
            scanf("%lx",&len);
            unsigned char* buf=(unsigned char*)malloc(len);
            unsigned char* ptr= (unsigned char*)(&value);
            for(size_t i=0;i<len;i++){
                buf[i]=*ptr;
                ptr++;
            }

            printf("%s [*] HANDLE CMD: write [%llx] [%llx] [%llx] <=0x%lx\n",prompt,addr,addr+(ADDR_T)baseaddr,value,len);
            handle_write((int)(child_pid),addr+(ADDR_T)baseaddr,buf,len);
            free(buf);
            // TODO

            continue;
        }

        if(strcmp("break", action)==0 || strcmp("b", action)==0) {

            ADDR_T addr;
            scanf("%llx",&addr);
            
            handle_break((int)(child_pid),addr+(ADDR_T)baseaddr);
            // TODO

            continue;
        }

        if(strcmp("step", action)==0 || strcmp("s", action)==0) {
            printf("%s [*] HANDLE CMD: step\n",prompt);
            set_debug_state((int)(child_pid),SINGLE_STEP);
            //get_registers((int)(child_pid), regs);
            // TODO
            break;
            continue;
        }

        if(strcmp("continue", action)==0 || strcmp("c", action)==0) {
            printf("%s [*] HANDLE CMD: continue\n",prompt);
            set_debug_state((int)(child_pid),NON_STOP);
            // TODO
            break;
            continue;
        }

        if(strcmp("quit", action)==0 || strcmp("q", action)==0) {
            LOG("HANDLE CMD: quit\n");
            exit(0);
            continue;
        }

        WARN("Not available commands\n");
    }
}


/*
  Get the current registers of @pid, and store it to @regs.
*/
void get_registers(int pid, struct user_regs_struct *regs) {
    if(ptrace(PTRACE_GETREGS, pid, NULL, regs)<0) {
        die("Error getting registers");
    }
    return;
}


/*
  Set the registers of @pid with @regs.
*/
void set_registers(int pid, struct user_regs_struct *regs) {
    // TODO
    if(ptrace(PTRACE_SETREGS,pid,NULL,regs)<0){
        die("Error setting registers");
    }
    TODO_UNUSED(pid);
    TODO_UNUSED(regs);
}


/*
  Get the base address of the main binary image, 
  loaded to the process @pid.
  This base address is the virtual address.
*/
ADDR_T get_image_baseaddr(int pid) {
    hr_procmaps** procmap = construct_procmaps(pid);
    ADDR_T baseaddr = 0;
    if(procmap[0]){
        baseaddr=procmap[0]->addr_begin;
    }
    // TODO
    TODO_UNUSED(procmap);
    return baseaddr;
}

/*
  Perform the job if the software breakpoint is fired.
  This includes to restore the original value at the breakpoint address.
*/
void handle_break_post(int pid, struct user_regs_struct *regs) {
    // TODO

    ADDR_T add=(regs->rip)-0x01;
    unsigned char tmp[1];
    
    for(int i=0;i<MAX_BPS;i++){
        if(bps[i].addr==add){
            tmp[0]=bps[i].orig_value;
            handle_write(pid,add,tmp,1);

            regs->rip=regs->rip-0x01;
            set_registers(pid,regs);
            printf("[*] 	 FOUND MATCH BP: [%d] [%llx]\n",i,bps[i].addr);
            break;
            
        }
    }
    
    //ptrace(PTRACE_CONT,pid,NULL,NULL);
    TODO_UNUSED(pid);
    TODO_UNUSED(regs);
}


/* HINT: No need to change this function */
void tracer(int child_pid) {
    int child_status;

    LOG("Tracer with pid=%d\n", getpid());

    wait(&child_status);

    ADDR_T baseaddr = get_image_baseaddr(child_pid);

    int steps_count = 0;
    struct user_regs_struct tracee_regs;
    set_debug_state(child_pid, SINGLE_STEP);

    while(1) {
        wait(&child_status);
        steps_count += 1;

        if(WIFEXITED(child_status)) {
            LOG("Exited in %d steps with status=%d\n",
                steps_count, child_status);
            break;
        }
        
        get_registers(child_pid, &tracee_regs);

        LOG("[step %d] rip=%llx child_status=%d\n", steps_count,
            tracee_regs.rip, child_status);

        handle_break_post(child_pid, &tracee_regs);
        prompt_user(child_pid, &tracee_regs, baseaddr);
    }
}

/* HINT: No need to change this function */
int main(int argc, char* argv[]) {
    char* usage = "USAGE: ./snudbg <cmd>";
    
    if (argc < 2){
        die(usage);
    }

    int pid = fork();

    switch (pid) {
    case -1:
        die("Error forking");
        break;
    case 0:
        tracee(argv+1);
        break;
    default:
        tracer(pid);
        break;
    }
    return 0;
}
