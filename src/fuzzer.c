#define _GNU_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <elf.h>

#include <capstone/capstone.h>

/*
 * Defines needed for bfd "bug":
 * https://github.com/mlpack/mlpack/issues/574
 */
#define PACKAGE
#define PACKAGE_VERSION
#include <dis-asm.h>

#include "filter.h"
#include "logging.h"
#include "reg_const.h"
#include "util.h"

#define STATUS_UPDATE_RATE 0x1000

#define INSN_RANGE_MIN 0x00000000
#define INSN_RANGE_MAX 0xffffffff

#define PAGE_SIZE 4096

#ifdef __aarch64__
    #define CAPSTONE_ARCH CS_ARCH_ARM64
#else
    #define CAPSTONE_ARCH CS_ARCH_ARM
#endif

// Increment bits in x indicated by the mask m
#define MASKED_INCREMENT(x, m) ((x & ~m) | (((x | ~m) + 1) & m))

void *insn_buffer;
volatile sig_atomic_t last_insn_signum = 0;
volatile sig_atomic_t executing_insn = 0;
uint32_t insn_offset = 0;

void signal_handler(int, siginfo_t*, void*);
void init_signal_handler(void (*handler)(int, siginfo_t*, void*), int);
void execution_boilerplate(void);
uint64_t get_nano_timestamp(void);
int disas_sprintf(void*, const char*, ...);
uint32_t fill_insn_buffer(uint8_t*, size_t, uint32_t, bool);
int libopcodes_disassemble(uint32_t, bool, char*, size_t);
int capstone_disassemble(uint32_t, bool, char*, size_t, csh*);
void slave_loop(void);
void slave_loop_thumb(void);
pid_t spawn_slave(bool);
int custom_ptrace_getregs(pid_t, struct USER_REGS_TYPE*);
int custom_ptrace_setregs(pid_t, struct USER_REGS_TYPE*);
void execute_insn_slave(pid_t*, uint8_t*, size_t, bool, execution_result*);
bool is_thumb32(uint32_t);
uint64_t get_next_instruction(uint64_t, uint64_t, bool);
void print_help(char*);

extern char boilerplate_start, boilerplate_end, insn_location;

void signal_handler(int sig_num, siginfo_t *sig_info, void *uc_ptr)
{
    // Suppress unused warning
    (void)sig_info;

    ucontext_t* uc = (ucontext_t*) uc_ptr;

    last_insn_signum = sig_num;

    if (executing_insn == 0) {
        // Something other than a hidden insn execution raised the signal,
        // so quit
        fprintf(stderr, "%s\n", strsignal(sig_num));
        exit(1);
    }

    // Jump to the next instruction (i.e. skip the illegal insn)
    uintptr_t insn_skip = (uintptr_t)(insn_buffer) + (insn_offset+1)*4;

#ifdef __aarch64__
    uc->uc_mcontext.pc = insn_skip;
#else
    uc->uc_mcontext.arm_pc = insn_skip;
#endif
}

void init_signal_handler(void (*handler)(int, siginfo_t*, void*), int signum)
{
    struct sigaction s;

    s.sa_sigaction = handler;
    s.sa_flags = SA_SIGINFO;

    sigfillset(&s.sa_mask);

    sigaction(signum,  &s, NULL);
}

/*
 * State management when testing instructions.
 *
 * Used to prevent instructions with side-effects to corrupt the program
 * state, in addition to saving register values for analysis.
 */
void execution_boilerplate(void)
{
#ifdef __aarch64__
    asm volatile(
            ".global boilerplate_start  \n"
            "boilerplate_start:         \n"

            // Store all gregs
            "stp x0, x1, [sp, #-16]!    \n"
            "stp x2, x3, [sp, #-16]!    \n"
            "stp x4, x5, [sp, #-16]!    \n"
            "stp x6, x7, [sp, #-16]!    \n"
            "stp x8, x9, [sp, #-16]!    \n"
            "stp x10, x11, [sp, #-16]!  \n"
            "stp x12, x13, [sp, #-16]!  \n"
            "stp x14, x15, [sp, #-16]!  \n"
            "stp x16, x17, [sp, #-16]!  \n"
            "stp x18, x19, [sp, #-16]!  \n"
            "stp x20, x21, [sp, #-16]!  \n"
            "stp x22, x23, [sp, #-16]!  \n"
            "stp x24, x25, [sp, #-16]!  \n"
            "stp x26, x27, [sp, #-16]!  \n"
            "stp x28, x29, [sp, #-16]!  \n"
            "stp x30, xzr, [sp, #-16]!  \n"

            /*
             * Reset the regs to make insn execution deterministic
             * and avoid program corruption.
             */
            "mov x0, %[reg_init]        \n"
            "mov x1, %[reg_init]        \n"
            "mov x2, %[reg_init]        \n"
            "mov x3, %[reg_init]        \n"
            "mov x4, %[reg_init]        \n"
            "mov x5, %[reg_init]        \n"
            "mov x6, %[reg_init]        \n"
            "mov x7, %[reg_init]        \n"
            "mov x8, %[reg_init]        \n"
            "mov x9, %[reg_init]        \n"
            "mov x10, %[reg_init]       \n"
            "mov x11, %[reg_init]       \n"
            "mov x12, %[reg_init]       \n"
            "mov x13, %[reg_init]       \n"
            "mov x14, %[reg_init]       \n"
            "mov x15, %[reg_init]       \n"
            "mov x16, %[reg_init]       \n"
            "mov x17, %[reg_init]       \n"
            "mov x18, %[reg_init]       \n"
            "mov x19, %[reg_init]       \n"
            "mov x20, %[reg_init]       \n"
            "mov x21, %[reg_init]       \n"
            "mov x22, %[reg_init]       \n"
            "mov x23, %[reg_init]       \n"
            "mov x24, %[reg_init]       \n"
            "mov x25, %[reg_init]       \n"
            "mov x26, %[reg_init]       \n"
            "mov x27, %[reg_init]       \n"
            "mov x28, %[reg_init]       \n"
            "mov x29, %[reg_init]       \n"
            "mov x30, %[reg_init]       \n"

            ".global insn_location      \n"
            "insn_location:             \n"

            // This instruction will be replaced with the one to be tested
            "nop                        \n"

            // Restore all gregs
            "ldp x30, xzr, [sp], #16    \n"
            "ldp x28, x29, [sp], #16    \n"
            "ldp x26, x27, [sp], #16    \n"
            "ldp x24, x25, [sp], #16    \n"
            "ldp x22, x23, [sp], #16    \n"
            "ldp x20, x21, [sp], #16    \n"
            "ldp x18, x19, [sp], #16    \n"
            "ldp x16, x17, [sp], #16    \n"
            "ldp x14, x15, [sp], #16    \n"
            "ldp x12, x13, [sp], #16    \n"
            "ldp x10, x11, [sp], #16    \n"
            "ldp x8, x9, [sp], #16      \n"
            "ldp x6, x7, [sp], #16      \n"
            "ldp x4, x5, [sp], #16      \n"
            "ldp x2, x3, [sp], #16      \n"
            "ldp x0, x1, [sp], #16      \n"

            "ret                        \n"
            ".global boilerplate_end    \n"
            "boilerplate_end:           \n"
            :
            : [reg_init] "n" (0)
            );
#else
    asm volatile(
            ".global boilerplate_start  \n"
            "boilerplate_start:         \n"

            // Store all gregs
            "push {r0-r12, lr}          \n"

            /*
             * It's better to use ptrace in cases where the sp might
             * be corrupted, but storing the sp in a vector reg
             * mitigates the issue somewhat.
             */
            "vmov s0, sp                \n"

            // Reset the regs to make insn execution deterministic
            // and avoid program corruption
            "mov r0, %[reg_init]        \n"
            "mov r1, %[reg_init]        \n"
            "mov r2, %[reg_init]        \n"
            "mov r3, %[reg_init]        \n"
            "mov r4, %[reg_init]        \n"
            "mov r5, %[reg_init]        \n"
            "mov r6, %[reg_init]        \n"
            "mov r7, %[reg_init]        \n"
            "mov r8, %[reg_init]        \n"
            "mov r9, %[reg_init]        \n"
            "mov r10, %[reg_init]       \n"
            "mov r11, %[reg_init]       \n"
            "mov r12, %[reg_init]       \n"
            "mov lr, %[reg_init]        \n"
            // Setting the sp to 0 seems to mess up the
            // signal handling
            /* "mov sp, %[reg_init]        \n" */
            "msr cpsr_cxsf, #0x10       \n"

            ".global insn_location      \n"
            "insn_location:             \n"

            // This instruction will be replaced with the one to be tested
            "nop                        \n"

            "msr cpsr_cxsf, #0x10       \n"
            "vmov sp, s0                \n"

            // Restore all gregs
            "pop {r0-r12, lr}           \n"

            "bx lr                      \n"
            ".global boilerplate_end    \n"
            "boilerplate_end:           \n"
            :
            : [reg_init] "n" (0)
            );
#endif
}

uint64_t get_nano_timestamp(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

typedef struct {
  char *buffer;
  bool reenter;
} stream_state;

/*
 * From
 *  https://blog.yossarian.net/2019/05/18/Basic-disassembly-with-libopcodes
 * Thanks
 */
int disas_sprintf(void *stream, const char *fmt, ...) {
    stream_state *ss = (stream_state *)stream;

    size_t n;
    va_list arg;
    va_start(arg, fmt);

    if (!ss->reenter) {
        n = vasprintf(&ss->buffer, fmt, arg);
        ss->reenter = true;
    } else {
        char *tmp;
        n = vasprintf(&tmp, fmt, arg);

        char *tmp2;
        n = asprintf(&tmp2, "%s%s", ss->buffer, tmp);
        free(ss->buffer);
        free(tmp);
        ss->buffer = tmp2;
    }
    va_end(arg);

    // ugh...
    (void)n;

    return 0;
}

/*
 * Fill buf with the the bytes in insn, taking into account
 * that ARM uses little-endian, and that in Thumb (both 16-bit
 * and 32-bit), a word is 16-bit long.
 *
 * Return the buffer length
 */
uint32_t fill_insn_buffer(uint8_t *buf, size_t buf_size, uint32_t insn, bool thumb)
{
    if (buf_size < 4)
        return -1;

    if (thumb) {
        buf[0] = (insn >> 16) & 0xff;
        buf[1] = (insn >> 24) & 0xff;

        if (is_thumb32(insn)) {
            buf[2] = insn & 0xff;
            buf[3] = (insn >> 8) & 0xff;
        } else {
            return 2;
        }
    } else {
        buf[0] = insn & 0xff;
        buf[1] = (insn >> 8) & 0xff;
        buf[2] = (insn >> 16) & 0xff;
        buf[3] = (insn >> 24) & 0xff;
    }
    return 4;
}

int libopcodes_disassemble(uint32_t insn, bool thumb, char *disas_str, size_t disas_str_size) {
    stream_state ss = {};

    // Set up the disassembler
    disassemble_info disasm_info = {};
    init_disassemble_info(&disasm_info, &ss, (fprintf_ftype) disas_sprintf);

#ifdef __aarch64__
    disasm_info.arch = bfd_arch_aarch64;
    disasm_info.mach = bfd_mach_aarch64;
#else
    disasm_info.arch = bfd_arch_arm;
    disasm_info.mach = bfd_mach_arm_8;
#endif

    disasm_info.read_memory_func = buffer_read_memory;
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn, thumb);
    disasm_info.buffer = insn_bytes;
    disasm_info.buffer_length = buf_length;
    disasm_info.buffer_vma = 0;

    if (thumb)
        disasm_info.disassembler_options = "force-thumb";

    disassemble_init_for_target(&disasm_info);

    disassembler_ftype disasm;
    disasm = disassembler(disasm_info.arch, false, disasm_info.mach, NULL);

    // Actually do the disassembly
    size_t insn_size = disasm(0, &disasm_info);
    if (thumb && !is_thumb32(insn)) {
        assert(insn_size == 2);
    } else {
        assert(insn_size == 4);
    }

    // Store the resulting stsring
    snprintf(disas_str, disas_str_size, "%s", ss.buffer);

    ss.reenter = false;
    free(ss.buffer);

    return insn_size;
}

int capstone_disassemble(uint32_t insn, bool thumb, char *disas_str, size_t disas_str_size, csh *handle)
{
    cs_insn *capstone_insn;
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn, thumb);
    size_t capstone_count = cs_disasm(*handle, insn_bytes, buf_length, 0, 0, &capstone_insn);
    if (capstone_count > 0) {
        snprintf(disas_str,
                 disas_str_size,
                 "%s\t%s", capstone_insn[0].mnemonic, capstone_insn[0].op_str);
        cs_free(capstone_insn, capstone_count);
    } else {
        strcpy(disas_str, "invalid assembly code");
    }
    return capstone_count;
}

void slave_loop(void)
{
    asm volatile(
            "loop:      \n"
#ifdef __aarch64__
            "   brk #0  \n"
#else
            /*
             * UDF #16 is the same as bkpt in Linux, but without filling up the syslog
             * (https://www.jwhitham.org//2015/04/the-mystery-of-fifteen-millisecond.html)
             */
            "   udf #16 \n"
#endif
            "   nop     \n"
            "   b loop  \n"
            );

}

#ifdef __aarch64__
void slave_loop_thumb(void)
{
    return;
}
#else
__attribute__((target("thumb")))
void slave_loop_thumb(void)
{
    asm volatile(
            "loopt:     \n"
            "   udf #1  \n" // Linux-reserved bkpt
            "   nop.w   \n" // 32-bit wide nop
            "   b loopt \n"
            );
}
#endif

pid_t spawn_slave(bool thumb)
{
    pid_t slave_pid = fork();
    if (slave_pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if (thumb)
            slave_loop_thumb();
        else
            slave_loop();
    }
    int status;
    waitpid(slave_pid, &status, 0);
    return slave_pid;
}

int custom_ptrace_getregs(pid_t pid, struct USER_REGS_TYPE *regs)
{
#ifdef __aarch64__
    struct iovec iovec = { regs, sizeof(*regs) };
    return ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iovec);
#else
    return ptrace(PTRACE_GETREGS, pid, NULL, regs);
#endif
}

int custom_ptrace_setregs(pid_t pid, struct USER_REGS_TYPE *regs)
{
#ifdef __aarch64__
    struct iovec iovec = { regs, sizeof(*regs) };
    return ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iovec);
#else
    return ptrace(PTRACE_SETREGS, pid, NULL, regs);
#endif
}

void execute_insn_slave(pid_t *slave_pid_ptr, uint8_t *insn_bytes, size_t insn_length, bool thumb, execution_result *result)
{
    int status;

    pid_t slave_pid = *slave_pid_ptr;

    // Set regs etc.
    struct USER_REGS_TYPE regs;

    if (custom_ptrace_getregs(slave_pid, &regs) == -1) {
        perror("getregs failed");
    }

#ifdef __aarch64__
    static unsigned long long insn_loc = 0;
    unsigned long long *pc_reg = &regs.pc;
    unsigned long long *regs_ptr = regs.regs;
#else
    static unsigned long insn_loc = 0;
    unsigned long *pc_reg = &regs.uregs[ARM_pc];
    unsigned long *regs_ptr = regs.uregs;
#endif

    if (insn_loc == 0) {
        if (thumb) {
            // The bkpt / udf #1 instruction is two bytes
            insn_loc = *pc_reg + 2;
        } else {
            insn_loc = *pc_reg + 4;
        }
    }

    uint32_t insn;
    if (thumb && insn_length == 2) {
        /*
         * If the insn length is two, we have a 16-bit thumb instruction
         * In those cases, add a nop (bf00) as the second half-word
         */
        insn = insn_bytes[0]
                | (insn_bytes[1] << 8)
                | (0x00 << 16)
                | (0xbf << 24);
    } else {
        insn = insn_bytes[0]
                | (insn_bytes[1] << 8)
                | (insn_bytes[2] << 16)
                | (insn_bytes[3] << 24);
    }

#ifdef __aarch64__
    /*
     * PTRACE_POKETEXT can only write a word at a time, which is 8 bytesin AArch64.
     * Since every instruction is 4 bytes, this will result in the instruction
     * after being overwritten. We therefore need to combine the two into a
     * single word before writing.
     */
    uint32_t next_insn = (uint32_t)ptrace(PTRACE_PEEKTEXT, slave_pid, insn_loc+4, 0);
    uint64_t insn_word = insn | ((uint64_t)next_insn << 32);
#else
    uint32_t insn_word = insn;
#endif

    if (ptrace(PTRACE_POKETEXT, slave_pid, insn_loc, insn_word) == -1) {
        perror("poketext failed");
    }

    // Reset all regs
    memset(regs_ptr, 0, UREG_COUNT * sizeof(regs_ptr[0]));
    *pc_reg = insn_loc;
#ifdef __aarch64__
    regs.sp = 0;
    regs.pstate = 0;
#else
    regs.uregs[ARM_cpsr] = 0x10;  // user mode
    if (thumb)
        regs.uregs[ARM_cpsr] |= 0x20;   // Thumb execution
#endif
    if (custom_ptrace_setregs(slave_pid, &regs) == -1) {
        perror("setregs failed");
    }

    memcpy(&result->regs_before, &regs, sizeof(regs));

    // Execute the instruction
    ptrace(PTRACE_CONT, slave_pid, NULL, NULL);
    waitpid(slave_pid, &status, 0);

    if (WIFEXITED(status)) {
        // TODO: Refork slave if it died
        result->died = true;
        return;
    }

    result->died = false;

    // Store results
    if (custom_ptrace_getregs(slave_pid, &regs) == -1) {
        perror("getregs failed");
    }
    memcpy(&result->regs_after, &regs, sizeof(regs));

    siginfo_t siginfo;
    if (ptrace(PTRACE_GETSIGINFO, slave_pid, NULL, &siginfo) == -1) {
        perror("getsiginfo failed");
    }

    int signo = siginfo.si_signo;
    result->signal = (signo == SIGTRAP) ? 0 : signo;


    // Fix the pc if the exception prevented the pc from advancing
    if (*pc_reg == insn_loc) {
        // Check whether a trap signal was caused by the executed instruction
        // (as opposed to the bkpt)
        if (signo == SIGTRAP) {
            result->signal = signo;
        }

        if (thumb) {
            *pc_reg = insn_loc - 2;
        } else {
            *pc_reg = insn_loc - 4;
        }

        if (custom_ptrace_setregs(slave_pid, &regs) == -1) {
            perror("setregs failed");
        }
        ptrace(PTRACE_CONT, slave_pid, NULL, NULL);
        waitpid(slave_pid, &status, 0);
    }
}

/*
 * Returns the next instruction based on the current one, taking
 * the instruction mask and thumb execution into account.
 *
 * The insn and mask are 64-bit to prevent overflow, such that the
 * caller can check whether the instruction is out of bounds.
 */
uint64_t get_next_instruction(uint64_t insn, uint64_t mask, bool thumb)
{
    if (thumb && !is_thumb32(insn & 0xffffffff)) {
        /*
         * Increment upper half, including the "extended" bits
         * and taking the supplied mask into account
         */
        mask &= 0xffffffffffff0000;
    }
    return MASKED_INCREMENT(insn, mask);
}

struct option long_options[] = {
    {"help",            no_argument,        NULL, 'h'},
    {"start",           required_argument,  NULL, 's'},
    {"end",             required_argument,  NULL, 'e'},
    {"no-exec",         no_argument,        NULL, 'n'},
    {"log-suffix",      required_argument,  NULL, 'l'},
    {"quiet",           no_argument,        NULL, 'q'},
    {"discreps",        no_argument,        NULL, 'c'},
    {"ptrace",          no_argument,        NULL, 'p'},
    {"exec-all",        no_argument,        NULL, 'x'},
    {"print-regs",      no_argument,        NULL, 'r'},
    {"single-exec",     no_argument,        NULL, 'i'},
    {"filter",          no_argument,        NULL, 'f'},
    {"mask",            required_argument,  NULL, 'm'},
    {"thumb",           no_argument,        NULL, 't'}
};

void print_help(char *cmd_name)
{
    printf("Usage: %s [option(s)]\n", cmd_name);
    printf("\n\
Options:\n\
        -h, --help              Print help information.\n\
        -s, --start <insn>      Start of instruction search range (in hex).\n\
                                [default: 0x00000000]\n\
        -e, --end <insn>        End of instruction search range, inclusive (in hex).\n\
                                [default: 0xffffffff]\n\
        -n, --no-exec           Calculate the total amount of undefined instructions,\n\
                                without executing them.\n\
        -l, --log-suffix        Add a suffix to the log and status file.\n\
        -q, --quiet             Don't print the status line.\n\
        -c, --discreps          Log disassembler discrepancies.\n\
        -p, --ptrace            Execute instructions on a separate process using ptrace.\n\
                                This will generally make execution slower, but lowers the\n\
                                chance of the fuzzer crashing in case hidden instructions\n\
                                with certain side-effects are found. It also enables\n\
                                logging register content changes on hidden instructions.\n\
        -x, --exec-all          Execute all instructions (regardless of the disassembly result).\n\
        -r, --print-regs        Print register values before/after instruction execution.\n\
                                (Only available together with -p)\n\
        -i, --single-exec       Execute a single instruction (i.e., set end=start).\n\
        -f, --filter            Filter away (skip) certain instructions that might generate\n\
                                false positives.\n\
                                (Mainly instructions with incorrect SBO/SBZ bits.)\n\
        -m, --mask <mask>       Only update instruction bits marked in the supplied mask.\n\
                                Useful for testing different operands on a single instruction.\n\
                                Example: 0xf0000000 -> only increment most significant nibble\n\
        -t, --thumb             Use the thumb instruction set (only available on AArch32).\n"
    );
}

int main(int argc, char **argv)
{
    uint32_t insn_range_start = INSN_RANGE_MIN;
    uint32_t insn_range_end = INSN_RANGE_MAX; // 2^32 - 1
    uint64_t insn_mask = ~0;
    bool no_exec = false;
    bool quiet = false;
    bool log_discreps = false;
    bool use_ptrace = false;
    bool exec_all = false;
    bool print_regs = false;
    bool single_insn = false;
    bool do_filter = false;
    bool thumb = false;

    char *file_suffix = NULL;
    char *endptr;
    int c;
    while ((c = getopt_long(argc, argv, "hs:e:nl:qcpxrifm:t", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                print_help(argv[0]);
                return 1;
            case 's':
                insn_range_start = strtoull(optarg, &endptr, 16);
                if (*endptr != '\0') {
                    fprintf(stderr, "ERROR: Unable to read instruction range start\n");
                    return 1;
                }
                break;
            case 'e':
                insn_range_end = strtoull(optarg, &endptr, 16);
                if (*endptr != '\0') {
                    fprintf(stderr, "ERROR: Unable to read instruction range end\n");
                    return 1;
                }
                break;
            case 'n':
                no_exec = true;
                break;
            case 'l':
                if (asprintf(&file_suffix, "%s", optarg) == -1) {
                    fprintf(stderr, "ERROR: asprintf with file_suffix failed\n");
                    return 1;
                }
                break;
            case 'q':
                quiet = true;
                break;
            case 'c':
                log_discreps = true;
                break;
            case 'p':
                use_ptrace = true;
                break;
            case 'x':
                exec_all = true;
                break;
            case 'r':
                print_regs = true;
                break;
            case 'i':
                single_insn = true;
                break;
            case 'f':
                do_filter = true;
                break;
            case 'm':
                insn_mask = strtoull(optarg, &endptr, 16);
                if (*endptr != '\0') {
                    fprintf(stderr, "ERROR: Unable to read instruction mask\n");
                    return 1;
                }
                /*
                 * Set all the bits in the upper half of the (64-bit) mask.
                 * This way we don't need to check for wrap-around
                 * after MASKED_INCREMENT.
                 */
                insn_mask = 0xffffffff00000000 | (insn_mask & 0xffffffff);
                break;
            case 't':
#ifdef __aarch64__
                fprintf(stderr, "Thumb execution is only available on AArch32.\n");
                return 1;
#else
                thumb = true;
#endif
                break;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    if (thumb && !use_ptrace) {
        /*
         * Only ptrace execution supported for thumb as of now, as page exec
         * requires quite a few changes with little to be gained, seeing
         * as the thumb(2) instruction space is an order of magnitude smaller
         * than A32.
         */
        fprintf(stderr, "Thumb execution requires ptrace. Run with -p option.\n");
        return 1;
    }

    if (single_insn)
        insn_range_end = insn_range_start;

    pid_t slave_pid = 0;
    if (use_ptrace)
        slave_pid = spawn_slave(thumb);

    char *log_path;
    if (asprintf(&log_path, "%s%s", "data/log", file_suffix == NULL ? "" : file_suffix) == -1) {
        fprintf(stderr, "ERROR: asprintf with log_path failed\n");
        return 1;
    }

    char *statusfile_path;
    if (asprintf(&statusfile_path, "%s%s", "data/status", file_suffix == NULL ? "" : file_suffix) == -1) {
        fprintf(stderr, "ERROR: asprintf with statusfile_path failed\n");
        return 1;
    }
    if (file_suffix != NULL)
        free(file_suffix);

    if (insn_range_end < insn_range_start) {
        fprintf(stderr, "ERROR: Instruction range start > instruction range end\n");
        return 1;
    }

	csh cs_handle;

	if (cs_open(CAPSTONE_ARCH,
                CS_MODE_ARM + CS_MODE_LITTLE_ENDIAN + (thumb ? CS_MODE_THUMB : 0),
                &cs_handle) != CS_ERR_OK) {
        fprintf(stderr, "ERROR: Unable to load capstone\n");
		return 1;
    }

    init_signal_handler(signal_handler, SIGILL);
    init_signal_handler(signal_handler, SIGSEGV);
    init_signal_handler(signal_handler, SIGTRAP);

    // Allocate an executable page / memory region
    insn_buffer = mmap(NULL,
                       PAGE_SIZE,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);

    if (insn_buffer == MAP_FAILED) {
        perror("insn_buffer mmap failed");
        return 1;
    }

    uint32_t boilerplate_length = (&boilerplate_end - &boilerplate_start) / 4;

    // Load the boilerplate assembly
    for (uint32_t i = 0; i < boilerplate_length; ++i)
        ((uint32_t*)insn_buffer)[i] = ((uint32_t*)&boilerplate_start)[i];

    insn_offset = (&insn_location - &boilerplate_start) / 4;

    // Jumps to the instruction buffer
    void (*execute_insn_buffer)() = (void(*)()) insn_buffer;

    struct stat st = {0};

    // Create data directory
    if (stat("data", &st) == -1) {
        if (mkdir("data", 0755) == -1) {
            perror("Unable to make data directory");
            return 1;
        }
    }

    // Clear/create log file
    FILE *log_fp = fopen(log_path, "w");
    if (log_fp == NULL) {
        fprintf(stderr,
                "Error opening logfile (%s). Logging will be disabled.\n",
                log_path);
    } else {
        fclose(log_fp);
    }

    uint64_t instructions_checked = 0;
    uint64_t instructions_skipped = 0;
    uint64_t instructions_filtered = 0;
    uint64_t hidden_instructions_found = 0;
    uint64_t disas_discreps_found = 0;
    uint64_t last_timestamp = get_nano_timestamp();

    char cs_str[256] = {0};
    char libopcodes_str[256] = {0};

    uint32_t curr_insn = insn_range_start & 0xffffffff;
    search_status curr_status = {0};

    for (uint64_t i = insn_range_start;
            i <= insn_range_end;
            i = get_next_instruction(i, insn_mask, thumb)) {
        curr_insn = i & 0xffffffff;

        // Check if capstone thinks the instruction is undefined
        int capstone_ret = capstone_disassemble(curr_insn,
                                                thumb,
                                                cs_str,
                                                sizeof(cs_str),
                                                &cs_handle);

        bool capstone_undefined = (capstone_ret == 0);

        // Now check what libopcodes thinks
        int libopcodes_ret = libopcodes_disassemble(curr_insn,
                                                    thumb,
                                                    libopcodes_str,
                                                    sizeof(libopcodes_str));
        if (libopcodes_ret == 0) {
            fprintf(stderr, "libopcodes disassembly failed on insn 0x%08" PRIx32 "\n", curr_insn);
            return 1;
        }

        uint64_t total_insns = (instructions_checked
                                + instructions_skipped
                                + instructions_filtered);
        // Write the current search status to the statusfile now and then
        if (total_insns % STATUS_UPDATE_RATE == 0 || curr_insn == insn_range_end) {
            curr_status.curr_insn = curr_insn;
            strncpy(curr_status.cs_disas, cs_str, sizeof(curr_status.cs_disas));
            strncpy(curr_status.libopcodes_disas,
                    libopcodes_str,
                    sizeof(curr_status.libopcodes_disas));
            curr_status.instructions_checked = instructions_checked;
            curr_status.instructions_skipped = instructions_skipped;
            curr_status.instructions_filtered = instructions_filtered;
            curr_status.disas_discrepancies = disas_discreps_found;
            curr_status.hidden_instructions_found = hidden_instructions_found;

            uint64_t curr_timestamp = get_nano_timestamp();
            curr_status.instructions_per_sec =
                STATUS_UPDATE_RATE / (double)((curr_timestamp - last_timestamp) / 1e9);
            last_timestamp = curr_timestamp;

            if (write_statusfile(statusfile_path, &curr_status) == -1) {
                fprintf(stderr, "ERROR: Failed to write to statusfile\n");
            }

            if (!quiet)
                print_statusline(&curr_status);
        }

        bool libopcodes_undefined = (strstr(libopcodes_str, "undefined") != NULL
                                  || strstr(libopcodes_str, "NYI") != NULL
                                  || strstr(libopcodes_str, "UNDEFINED") != NULL);

        /* Only test instructions that both capstone and libopcodes think are
         * undefined, but report inconsistencies, as they might indicate
         * bugs in either of the disassemblers.
         *
         * The primary reason for this double check is that capstone apparently
         * generates a lot of false positives.
         *
         * libopcodes does not appear to make the same mistake, but might have
         * other issues, so better use both. libopcodes is a bit slower, but
         * actually executing the insns takes so long anyway.
         */
        if ((!capstone_undefined || !libopcodes_undefined) && !exec_all) {
            // Write to log if one of the disassemblers thinks the instruction
            // is undefined, but not the other one
            if (capstone_undefined || libopcodes_undefined) {
                if (log_discreps) {
                        log_fp = fopen(log_path, "a");

                        if (log_fp != NULL) {
                            fprintf(log_fp,
                                    "%08" PRIx32 ",discrepancy,\"%s\",\"%s\"\n",
                                    curr_insn, cs_str, libopcodes_str);
                            fclose(log_fp);
                        }
                }
                ++disas_discreps_found;
            }

            ++instructions_skipped;
            continue;
        } else if (no_exec) {
            // Just count the undefined instruction and continue if we're not
            // going to execute it anyway (because of the no_exec flag)
            ++instructions_checked;
            continue;
        }

        if (do_filter && filter_instruction(curr_insn, thumb) && !exec_all) {
            ++instructions_filtered;
            continue;
        }

        uint8_t insn_bytes[4];
        size_t buf_length = fill_insn_buffer(insn_bytes,
                                             sizeof(insn_bytes),
                                             curr_insn,
                                             thumb);

        if (thumb && !is_thumb32(curr_insn)) {
            insn_bytes[2] = 0;
            insn_bytes[3] = 0;
        }

        execution_result exec_result = {0};
        if (use_ptrace) {
            execute_insn_slave(&slave_pid, insn_bytes, buf_length, thumb, &exec_result);

            if (exec_result.died) {
                fprintf(stderr, "slave died. quitting...\n");
                break;
            }

            last_insn_signum = exec_result.signal;
            if (print_regs)
                print_execution_result(&exec_result);
        } else {
            // Update the first instruction in the instruction buffer
            memcpy(insn_buffer + insn_offset * 4, insn_bytes, buf_length);

            last_insn_signum = 0;

            /*
             * Clear insn_buffer (at the insn to be tested)
             * in the d- and icache
             * (some instructions might be skipped otherwise.)
             */
            __clear_cache(insn_buffer + insn_offset * 4,
                          insn_buffer + insn_offset * 4 + sizeof(curr_insn));

            executing_insn = 1;

            // Jump to the instruction to be tested (and execute it)
            execute_insn_buffer();

            executing_insn = 0;

            exec_result.signal = last_insn_signum;
        }

        exec_result.insn = curr_insn;

        if (last_insn_signum != SIGILL) {
            if (write_logfile(log_path, &exec_result, use_ptrace) == -1) {
                fprintf(stderr, "ERROR: Failed to write to logfile\n");
            }
            ++hidden_instructions_found;
        }

        ++instructions_checked;
    }

    // Print the statusline one last time to capture the result of the last insn
    curr_status.curr_insn = curr_insn;
    strncpy(curr_status.cs_disas, cs_str, sizeof(curr_status.cs_disas));
    strncpy(curr_status.libopcodes_disas,
            libopcodes_str,
            sizeof(curr_status.libopcodes_disas));
    curr_status.instructions_checked = instructions_checked;
    curr_status.instructions_skipped = instructions_skipped;
    curr_status.instructions_filtered = instructions_filtered;
    curr_status.hidden_instructions_found = hidden_instructions_found;
    curr_status.disas_discrepancies = disas_discreps_found;

    print_statusline(&curr_status);
    write_statusfile(statusfile_path, &curr_status);

    // Compensate for the statusline not having a linebreak
    printf("\n");

    if (no_exec)
        printf("Total undefined: %" PRIu64 "\n", instructions_checked);

    munmap(insn_buffer, PAGE_SIZE);
    cs_close(&cs_handle);
    free(log_path);

    return 0;
}
