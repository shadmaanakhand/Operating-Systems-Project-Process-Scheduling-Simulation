#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Array size limits */
#define MAX_PROCESSES 100
#define MAX_EVENTS    2000
#define TIME_QUANTUM  2
#define MAX_BLOCKS    100
#define MAX_PAGES     1000
#define MAX_FRAMES    100

/* Holds all input and computed data for a single process */
typedef struct { 
    int pid, arrival_time, burst_time, priority, waiting_time, turnaround_time, mem_size; 
} Process;

/* Holds the final scheduling output statistics for a process */
typedef struct { 
    int pid, waiting_time, turnaround_time, completion_time, response_time; 
} ProcessResult;

/* Represents one time slot on the Gantt chart; pid == 0 means CPU was idle */
typedef struct { 
    int pid, start_time, end_time; 
} GanttEvent;

/* Represents one contiguous block of memory; free == 1 means unallocated */
typedef struct { 
    int start, size, free; 
} MemBlock;

/* Selection sort: orders the process array by arrival_time (ascending).
 * Called by both fifo() and round_robin() before scheduling begins. */
static void sort_by_arrival(Process p[], int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (p[i].arrival_time > p[j].arrival_time) {
                Process t = p[i]; 
                p[i] = p[j]; 
                p[j] = t;
            }
}

/* Appends one time slot to the Gantt event array.
 * Skips zero-duration events (s >= e) and silently drops events if the
 * array is full (prevents overflow). ec is incremented on each valid append. */
static void record_event(GanttEvent ev[], int *ec, int pid, int s, int e) {
    if (s >= e || *ec >= MAX_EVENTS) return;
    ev[*ec].pid = pid; ev[*ec].start_time = s; ev[*ec].end_time = e;
    (*ec)++;
}

/* When the CPU is idle (queue empty), scans all processes to find the
 * soonest future arrival time. Returns -1 if no unfinished process exists
 * beyond the current time ct. Used by round_robin() to jump the clock forward. */
static int find_next_arrival(Process p[], int n, int rb[], int ct) {
    int next = -1;
    for (int i = 0; i < n; i++)
        if (rb[i] > 0 && p[i].arrival_time > ct && (next == -1 || p[i].arrival_time < next))
            next = p[i].arrival_time;
    return next;
}

/* ── Circular Queue (used by Round Robin) ──
 * Stores process indices (not the processes themselves).
 * The % MAX_PROCESSES wrap on front/rear makes the fixed array behave as a ring. */
/* Returns 1 if the queue is empty, 0 otherwise */
static int  q_is_empty(int qs){ 
    return qs == 0; 
}
/* Adds process index idx to the back of the queue; silently ignores if full */
static void q_enqueue(int q[], int *f, int *r, int *qs, int idx){ 
    if (*qs >= MAX_PROCESSES) return; q[*r] = idx; *r = (*r + 1) % MAX_PROCESSES; (*qs)++; 
}
/* Removes and returns the process index at the front; returns -1 if empty.
 * (void)r suppresses an unused-parameter compiler warning. */
static int  q_dequeue(int q[], int *f, int *r, int *qs){ 
    (void)r; 
    if (*qs <= 0) return -1; 
    int i = q[*f]; 
    *f = (*f + 1) % MAX_PROCESSES; 
    (*qs)--; 
    return i; 
}

/* File I/O */
int read_processes(const char *filename, Process p[]) {
    FILE *file = fopen(filename, "r");
    if (!file) { 
        perror("Error opening file"); 
        return -1; }

    char header[256];
    if (!fgets(header, sizeof(header), file)) {
        printf("Error: Input file is empty.\n"); fclose(file); return -1;
    }

    int count = 0;
    while (count < MAX_PROCESSES) {
        int pid, at, bt, pr;
        if (fscanf(file, "%d %d %d %d", &pid, &at, &bt, &pr) != 4) break;
        p[count].pid = pid; p[count].arrival_time = at;
        p[count].burst_time = bt; p[count].priority = pr;
        p[count].waiting_time = p[count].turnaround_time = p[count].mem_size = 0;
        count++;
    }
    fclose(file);
    return count;
}

/* Prints the Gantt chart, per-process stats, averages, and CPU utilization.*/
void print_gantt_and_stats(GanttEvent ev[], int ec, ProcessResult res[], int n) {
    if (ec <= 0) { printf("\nNo events to display.\n"); return; }

    printf("\n--- GANTT CHART ---\n|");
    for (int i = 0; i < ec; i++)
        printf(ev[i].pid == 0 ? " IDLE |" : " P%d |", ev[i].pid);
    printf("\n%d", ev[0].start_time);
    for (int i = 0; i < ec; i++) {
        int d = snprintf(NULL, 0, "%d", ev[i].end_time);
        int sp = (5 - d < 1) ? 1 : 5 - d;
        for (int j = 0; j < sp; j++) printf(" ");
        printf("%d", ev[i].end_time);
    }

    printf("\n\n--- PROCESS STATISTICS ---\nPID\tWaiting Time\tTurnaround Time\n");
    float tw = 0.0f, tt = 0.0f;
    for (int i = 0; i < n; i++) {
        printf("%d\t%d\t\t%d\n", res[i].pid, res[i].waiting_time, res[i].turnaround_time);
        tw += res[i].waiting_time; 
        tt += res[i].turnaround_time;
    }

    printf("\n--- AVERAGE TIMES ---\n");
    printf("Average Waiting Time: %.2f\n", n > 0 ? tw / n : 0.0f);
    printf("Average Turnaround Time: %.2f\n", n > 0 ? tt / n : 0.0f);

    int busy = 0;
    for (int i = 0; i < ec; i++)
        if (ev[i].pid != 0) busy += ev[i].end_time - ev[i].start_time;
    int total = ev[ec - 1].end_time - ev[0].start_time;
    printf("CPU Utilization: %.2f%%\n", total > 0 ? 100.0f * busy / total : 0.0f);
}

/* First-Come First-Served Scheduling Algorithm */
void fcfs(Process p[], int n, GanttEvent ev[], int *ec, ProcessResult res[]) {
    sort_by_arrival(p, n);
    int ct = 0; *ec = 0; /* ct = current clock time */
    printf("\n--- FCFS Scheduling ---\nExecution Order:\n");
    for (int i = 0; i < n; i++) {
        /* If CPU would be idle before next process arrives, record an idle slot */
        if (ct < p[i].arrival_time) { 
            record_event(ev, ec, 0, ct, p[i].arrival_time); ct = p[i].arrival_time; }
        int st = ct, et = ct + p[i].burst_time; /* start and end time for this process */
        record_event(ev, ec, p[i].pid, st, et);
        printf("P%d: %d -> %d\n", p[i].pid, st, et);
        p[i].waiting_time = st - p[i].arrival_time;
        p[i].turnaround_time = et - p[i].arrival_time;
        res[i].pid = p[i].pid; res[i].waiting_time = p[i].waiting_time;
        res[i].turnaround_time = p[i].turnaround_time;
        res[i].completion_time = et; res[i].response_time = p[i].waiting_time;
        ct = et; /* advance clock to end of this process */
    }
}

/* Round Robin Scheduling Algorithm */
void round_robin(Process p[], int n, int tq, GanttEvent ev[], int *ec, ProcessResult res[]) {
    sort_by_arrival(p, n);
    int rb[MAX_PROCESSES], inq[MAX_PROCESSES], frt[MAX_PROCESSES]; /* remaining burst time per process. 1 if process is currently in the queue. Clock time of first run (-1 = not yet run) */
    for (int i = 0; i < n; i++) {
        rb[i] = p[i].burst_time; 
        inq[i] = 0; frt[i] = -1;
        res[i].pid = p[i].pid; 
        res[i].waiting_time = res[i].turnaround_time = 0;
        res[i].completion_time = 0; 
        res[i].response_time = -1;
    }
    *ec = 0;
    int done = 0, q[MAX_PROCESSES], front = 0, rear = 0, qs = 0; /* Clock starts at first process arrival (may leave an initial idle gap) */
    int ct = n > 0 ? p[0].arrival_time : 0;
    if (ct > 0) record_event(ev, ec, 0, 0, ct); /* record initial idle period */
    for (int i = 0; i < n; i++) /* Enqueue any processes that have already arrived at the start time */
        if (rb[i] > 0 && p[i].arrival_time <= ct && !inq[i]) { 
            q_enqueue(q, &front, &rear, &qs, i); inq[i] = 1; 
        }

    printf("\n--- Round Robin Scheduling (Time Quantum = %d) ---\nExecution Order:\n", tq);
    while (done < n) {
        if (q_is_empty(qs)) { 
            /* Queue is empty: jump clock to next process arrival */
            int nt = find_next_arrival(p, n, rb, ct);
            if (nt == -1) break;
            record_event(ev, ec, 0, ct, nt); 
            ct = nt;
            for (int i = 0; i < n; i++) /* Enqueue all processes that have now arrived */
                if (rb[i] > 0 && p[i].arrival_time <= ct && !inq[i]) { 
                    q_enqueue(q, &front, &rear, &qs, i); inq[i] = 1; 
                }
            continue;
        }
        int idx = q_dequeue(q, &front, &rear, &qs);
        if (idx < 0) continue; /* Record response time on the very first run of this process */
        if (frt[idx] == -1) { 
            frt[idx] = ct; res[idx].response_time = ct - p[idx].arrival_time; 
        }
        int exec = rb[idx] < tq ? rb[idx] : tq; /* Run for min(remaining burst, quantum) */
        record_event(ev, ec, p[idx].pid, ct, ct + exec);
        printf("P%d: %d -> %d\n", p[idx].pid, ct, ct + exec);
        ct += exec; rb[idx] -= exec;
        for (int i = 0; i < n; i++) /* Enqueue newly arrived processes BEFORE re-queuing the current one */
            if (rb[i] > 0 && p[i].arrival_time <= ct && !inq[i]) {
                /* Process not finished — send it to the back of the queue */ 
                q_enqueue(q, &front, &rear, &qs, i); inq[i] = 1; 
            }
        if (rb[idx] > 0) {
            q_enqueue(q, &front, &rear, &qs, idx);
        } else {
            /* Process finished, compute final stats */
            done++; res[idx].completion_time = ct;
            res[idx].turnaround_time = ct - p[idx].arrival_time;
            res[idx].waiting_time    = res[idx].turnaround_time - p[idx].burst_time;
            inq[idx] = 0;
        }
    }
}

/* Memory Allocation */
static void print_blocks(MemBlock b[], int m) {
    /* Prints a table of all current memory blocks with their status (FREE/USED) */
    printf("\nMemory Blocks:\nBlock\tStart\tSize\tStatus\n");
    for (int i = 0; i < m; i++)
        printf("%d\t%d\t%d\t%s\n", i, b[i].start, b[i].size, b[i].free ? "FREE" : "USED");
}

/* Marks block bi as used (size = req) and inserts a new free block for
 * any leftover space by shifting the array right to make room. */
static void split_and_allocate(MemBlock b[], int *m, int bi, int req) {
    int os = b[bi].start, osz = b[bi].size; /* save original start and size */
    b[bi].size = req; b[bi].free = 0; /* allocate the requested portion */
    int left = osz - req; /* remaining free space after allocation */
    if (left > 0 && *m < MAX_BLOCKS) {
        /* Shift blocks right to insert the new free block after bi */
        for (int i = *m; i > bi + 1; i--) b[i] = b[i - 1];
        b[bi + 1].start = os + req; b[bi + 1].size = left; b[bi + 1].free = 1;
        (*m)++;
    }
}

/* First-Fit: returns the index of the first free block large enough for req. */
static int select_block_first_fit(MemBlock b[], int m, int req) {
    for (int i = 0; i < m; i++) if (b[i].free && b[i].size >= req) return i;
    return -1;
}

/* Best-Fit: returns the index of the smallest free block that still fits req. */
static int select_block_best_fit(MemBlock b[], int m, int req) {
    int bi = -1, bs = 0; /* best index and best (smallest fitting) size */
    for (int i = 0; i < m; i++)
        if (b[i].free && b[i].size >= req && (bi == -1 || b[i].size < bs)) { 
            bi = i; bs = b[i].size; }
    return bi;
}

/* Worst-Fit: returns the index of the largest free block. */
static int select_block_worst_fit(MemBlock b[], int m, int req) {
    int wi = -1, ws = -1; /* worst index and worst (largest) size seen */
    for (int i = 0; i < m; i++)
        if (b[i].free && b[i].size >= req && b[i].size > ws) {
            wi = i; ws = b[i].size; }
    return wi;
}

/* Starts with one large free block, prompts for a fit strategy and each
 * process's memory requirement, then allocates and splits blocks accordingly. */
void simulate_memory_allocation(Process p[], int n) {
    int total_mem = 0, m = 1, choice = 0;
    MemBlock blocks[MAX_BLOCKS];
    printf("\n--- Memory Allocation (First/Best/Worst Fit) ---\n");
    printf("Enter total memory size (e.g., 1024): ");
    if (scanf("%d", &total_mem) != 1 || total_mem <= 0) { 
        printf("Invalid total memory size.\n"); return; 
    }
    /* Initialise memory as a single large free block */
    blocks[0].start = 0; blocks[0].size = total_mem; blocks[0].free = 1;
    printf("Choose allocation strategy: 1) First-Fit  2) Best-Fit  3) Worst-Fit : ");
    if (scanf("%d", &choice) != 1) {
        printf("Invalid choice.\n"); 
        return; 
    }
    /* Collect memory requirements for each process */
    for (int i = 0; i < n; i++) {
        printf("Enter memory required for P%d: ", p[i].pid);
        scanf("%d", &p[i].mem_size);
        if (p[i].mem_size < 0) p[i].mem_size = 0;
    }
    /* Attempt allocation for each process */
    for (int i = 0; i < n; i++) {
        int req = p[i].mem_size;
        if (req == 0) {
            printf("P%d requested 0 memory -> skipped\n", p[i].pid);
            continue;
        }
        /* Chained ternary dispatches to the chosen fit strategy */
        int idx = (choice == 1) ? select_block_first_fit(blocks, m, req)
                : (choice == 2) ? select_block_best_fit(blocks, m, req)
                : (choice == 3) ? select_block_worst_fit(blocks, m, req) : -2;
        if (idx == -2) { 
            printf("Invalid strategy choice.\n");
            return;
        }
        if (idx == -1) printf("P%d allocation FAILED (req=%d)\n", p[i].pid, req);
        else { printf("P%d allocated %d at block %d (start=%d)\n", p[i].pid, req, idx, blocks[idx].start); split_and_allocate(blocks, &m, idx, req); }
    }
    print_blocks(blocks, m);
}

/* Paging / Page Replacement */
static int find_in_frames(int frames[], int fc, int page) {
    /* Scans frames[] for page. Returns its frame index (a hit) or -1 (a fault). */
    for (int i = 0; i < fc; i++) if (frames[i] == page) return i;
    return -1;
}

/* Shared input routine for both paging simulations.
 * Reads frame count into *fc, reference string length into *rl,
 * and fills refs[] with the page reference sequence.
 * Returns 1 on success, 0 if any input is out of range. */
static int read_paging_input(int *fc, int *rl, int refs[]) {
    scanf("%d", fc);
    if (*fc <= 0 || *fc > MAX_FRAMES) {
        printf("Invalid frame count.\n"); 
        return 0; }
    printf("Enter reference string length (<= %d): ", MAX_PAGES);
    scanf("%d", rl);
    if (*rl <= 0 || *rl > MAX_PAGES) {
        printf("Invalid reference length.\n"); 
        return 0; }
    printf("Enter %d page numbers (space-separated):\n", *rl);
    for (int i = 0; i < *rl; i++) scanf("%d", &refs[i]);
    return 1;
}

/* Prints the current state of all frames after each reference. */
static void print_frames(int frames[], int fc) {
    for (int f = 0; f < fc; f++) frames[f] == -1 ? printf("[ ] ") : printf("[%d] ", frames[f]);
    printf("\n");
}

/* FIFO Page Replacement simulation */
void simulate_paging_fifo(void) {
    int fc = 0, rl = 0, refs[MAX_PAGES], frames[MAX_FRAMES];
    printf("\n--- Paging (FIFO Replacement) ---\nEnter number of frames (<= %d): ", MAX_FRAMES);
    if (!read_paging_input(&fc, &rl, refs)) return;
    for (int i = 0; i < fc; i++) frames[i] = -1; /* -1 = empty frame */
    int faults = 0, ptr = 0; /* ptr = next eviction target */
    for (int i = 0; i < rl; i++) {
        int page = refs[i], hit = find_in_frames(frames, fc, page) != -1;
        if (!hit) { 
            frames[ptr] = page; ptr = (ptr + 1) % fc; faults++; /* Fault: overwrite the oldest frame and advance the pointer */
        }
        printf("Ref %d -> %s | Frames: ", page, hit ? "HIT" : "FAULT");
        print_frames(frames, fc);
    }
    printf("Total Page Faults (FIFO): %d\n", faults);
}

/* LRU Page Replacement simulation */
void simulate_paging_lru(void) {
    int fc = 0, rl = 0, refs[MAX_PAGES], frames[MAX_FRAMES], last[MAX_FRAMES];
    printf("\n--- Paging (LRU Replacement) ---\nEnter number of frames (<= %d): ", MAX_FRAMES);
    if (!read_paging_input(&fc, &rl, refs)) return;
    for (int i = 0; i < fc; i++) { 
        frames[i] = -1; last[i] = -1; /* -1 = empty / never used */
    }
    int faults = 0;
    for (int t = 0; t < rl; t++) {
        int page = refs[t], pos = find_in_frames(frames, fc, page);
        if (pos != -1) {
            last[pos] = t; /* HIT: update the access timestamp for this frame */
        } else {
            faults++;
            int empty = -1; /* Look for an empty frame first */
            for (int f = 0; f < fc; f++) if (frames[f] == -1) { empty = f; break; }
            if (empty != -1) { frames[empty] = page; last[empty] = t; }
            else {
                int li = 0; /* No empty frame — evict the least recently used */
                for (int f = 1; f < fc; f++) if (last[f] < last[li]) li = f;
                frames[li] = page; last[li] = t;
            }
        }
        printf("Ref %d -> %s | Frames: ", page, pos != -1 ? "HIT" : "FAULT");
        print_frames(frames, fc);
    }
    printf("Total Page Faults (LRU): %d\n", faults);
}

/* Prompts user to choose FIFO or LRU */
void simulate_paging(void) {
    int choice = 0;
    printf("\nChoose replacement algorithm: 1) FIFO  2) LRU : ");
    scanf("%d", &choice);
    if (choice == 1) simulate_paging_fifo();
    else if (choice == 2) simulate_paging_lru();
    else printf("Invalid paging choice.\n");
}

/* Prints command-line usage instructions */
void print_usage(const char *prog) {
    printf("Usage: %s <filename> <mode>\n", prog);
    printf("  filename:  Path to the processes file\n");
    printf("  mode:\n");
    printf("    fcfs       - FCFS scheduling\n");
    printf("    rr         - Round Robin scheduling\n");
    printf("    mem-alloc  - Contiguous allocation (First/Best/Worst)\n");
    printf("    paging     - Paging + replacement (FIFO/LRU)\n");
    printf("    -h:          Display this help message\n");
    printf("\nExample: %s processes.txt fcfs\n", prog);
}

/* Validates arguments, loads processes from file, prints the process table,
 * then dispatches to the correct simulation based on the mode argument. */
int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-h") == 0) { 
        print_usage(argv[0]); 
        return EXIT_SUCCESS; 
    }
    if (argc != 3) { 
        printf("Error: Invalid number of arguments.\n\n"); 
        print_usage(argv[0]); 
        return EXIT_FAILURE; 
    }

    const char *filename = argv[1], *mode = argv[2];
    Process processes[MAX_PROCESSES];
    int n = read_processes(filename, processes);
    if (n <= 0) { 
        printf("Error: No processes loaded.\n"); 
        return EXIT_FAILURE; 
    }
    /* Print loaded process table */
    printf("Loaded %d processes from '%s'\nMode: %s\n", n, filename, mode);
    printf("\nPID\tArrival\tBurst\tPriority\n");
    for (int i = 0; i < n; i++)
        printf("%d\t%d\t%d\t%d\n", processes[i].pid, processes[i].arrival_time, processes[i].burst_time, processes[i].priority);

    ProcessResult results[MAX_PROCESSES];
    GanttEvent events[MAX_EVENTS];
    int ec = 0; /* event count for Gantt chart */

    /* Modes to select from */
    if (strcmp(mode, "fcfs") == 0) { 
        fcfs(processes, n, events, &ec, results);                   
        print_gantt_and_stats(events, ec, results, n); 
    } else if (strcmp(mode, "rr") == 0) { 
        round_robin(processes, n, TIME_QUANTUM, events, &ec, results); 
        print_gantt_and_stats(events, ec, results, n); 
    } else if (strcmp(mode, "mem-alloc") == 0) { 
        simulate_memory_allocation(processes, n); 
    } else if (strcmp(mode, "paging") == 0) { 
        simulate_paging(); 
    } else { 
        printf("Error: Unknown mode '%s'\n\n", mode); 
        print_usage(argv[0]); 
        return EXIT_FAILURE; 
    }
    return EXIT_SUCCESS;
}
