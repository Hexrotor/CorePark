#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define PATH_CPU_DIR_HEAD "/sys/devices/system/cpu/"
#define PATH_STAT "/proc/stat"
#define READ_NEXT_LINE while((c = fgetc(stat)) != '\n' && c != EOF)
//Intel CPU has E Core and P Core
#define E_CORE_START 12

struct CPU_STAT
{
    char name[10];
    size_t user;
    size_t nice;
    size_t system;
    size_t idle;
    size_t iowait;
    size_t irq;
    size_t softirq;
} TOTAL_CPU_STAT;

struct CPU
{
    int cpuid;
    float usage;
    int online;
    int freq;
    int maxFreq;
    int minFreq;
    struct CPU_STAT stat;
};

enum CONTROL_CORE_REQUEST {
    OFFLINE = 0,
    ONLINE = 1,
    QUERY_ONLINE_STATUS = 2,
    QUERY_FREQ = 3
};

int stop = 0;

int getCoreCount() {
    char path[100];
    int count = 0;;
    snprintf(path, 99, "%spresent", PATH_CPU_DIR_HEAD);
    FILE *presentCPUs = fopen(path, "r");
    fscanf(presentCPUs, "0-%d", &count);
    fclose(presentCPUs);
    return count + 1;
}

struct CPU** initCPU(size_t coreCount) {
    struct CPU** cpuList = (struct CPU**)malloc(sizeof(struct CPU*) * coreCount);
    for(int i = 0; i < coreCount; i++) {
        struct CPU* cpu = (struct CPU*)malloc(sizeof(struct CPU));
        cpu->cpuid = i;
        cpu->usage = 0;
        cpuList[i] = cpu;
    }
    return cpuList;
}

// void showCPUStat(struct CPU_STAT cpuStat) {
//     printf("name=%s, user=%lu, nice=%lu, system=%lu, idle=%lu, iowait=%lu, irq=%lu, softirq=%lu\n", cpuStat.name, cpuStat.user, cpuStat.nice, cpuStat.system, cpuStat.idle, cpuStat.iowait, cpuStat.irq, cpuStat.softirq);
// }

//function caculate total CPU usage, deprecated because it's not accurate when core count is changed
// float getRealTimeTotalCPUUsage() {
//     // The first line of /proc/stat is the total CPU usage
//     FILE *stat = fopen(PATH_STAT, "r");
//     if(stat == NULL) {
//         perror("Error opening /proc/stat");
//         exit(1);
//     }
//     struct CPU_STAT cpuStat;
//     fscanf(stat, "%s %lu %lu %lu %lu %lu %lu %lu", cpuStat.name, &cpuStat.user, &cpuStat.nice, &cpuStat.system, &cpuStat.idle, &cpuStat.iowait, &cpuStat.irq, &cpuStat.softirq);
//     fclose(stat);
//     size_t totaldiff = (cpuStat.user + cpuStat.nice + cpuStat.system + cpuStat.idle + cpuStat.iowait + cpuStat.irq + cpuStat.softirq) - (TOTAL_CPU_STAT.user + TOTAL_CPU_STAT.nice + TOTAL_CPU_STAT.system + TOTAL_CPU_STAT.idle + TOTAL_CPU_STAT.iowait + TOTAL_CPU_STAT.irq + TOTAL_CPU_STAT.softirq);
//     size_t idlediff = cpuStat.idle - TOTAL_CPU_STAT.idle;
//     //showCPUStat(cpuStat);
//     //showCPUStat(TOTAL_CPU_STAT);
//     float totalCPUUsage = (totaldiff - idlediff) / (float)totaldiff;
//     strncpy(TOTAL_CPU_STAT.name, cpuStat.name, 10);
//     TOTAL_CPU_STAT.user = cpuStat.user;
//     TOTAL_CPU_STAT.nice = cpuStat.nice;
//     TOTAL_CPU_STAT.system = cpuStat.system;
//     TOTAL_CPU_STAT.idle = cpuStat.idle;
//     TOTAL_CPU_STAT.iowait = cpuStat.iowait;
//     TOTAL_CPU_STAT.irq = cpuStat.irq;
//     TOTAL_CPU_STAT.softirq = cpuStat.softirq;
//     if(totaldiff<=idlediff) {
//         return -1;
//     }
//     return totalCPUUsage;
// }


// request: 0: offline, 1: online, 2: query online status, 3: query freqs
// /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
void controlCore(struct CPU *cpu, int request) {
    if(cpu->cpuid != 0 || request == QUERY_FREQ) {
        char path[100];
        if(request == QUERY_FREQ) {
            snprintf(path, 99, "%scpu%d/cpufreq/scaling_cur_freq", PATH_CPU_DIR_HEAD, cpu->cpuid);
            FILE *curFreq = fopen(path, "r");
            snprintf(path, 99, "%scpu%d/cpufreq/scaling_max_freq", PATH_CPU_DIR_HEAD, cpu->cpuid);
            FILE *maxFreq = fopen(path, "r");
            snprintf(path, 99, "%scpu%d/cpufreq/scaling_min_freq", PATH_CPU_DIR_HEAD, cpu->cpuid);
            FILE *minFreq = fopen(path, "r");
            if(curFreq == NULL || maxFreq == NULL || minFreq == NULL) {
                perror("Error opening freq file, maybe you need to run as root");
                perror(path);
                exit(1);
            }
        
            fscanf(curFreq, "%d", &cpu->freq);
            fscanf(maxFreq, "%d", &cpu->maxFreq);
            fscanf(minFreq, "%d", &cpu->minFreq);
            fclose(curFreq);
            fclose(maxFreq);
            fclose(minFreq);
            return;
        } else {
            snprintf(path, 99, "%scpu%d/online", PATH_CPU_DIR_HEAD, cpu->cpuid);
            FILE *f = fopen(path, "w+");
            if(f == NULL) {
                perror("Error opening online file, maybe you need to run as root");
                perror(path);
                exit(1);
            }
            switch (request)
            {
            case OFFLINE:
                fprintf(f, "0");
                break;
            case ONLINE:
                fprintf(f, "1");
                break;
            case QUERY_ONLINE_STATUS:
                fscanf(f, "%d", &cpu->online);
                break;
            }
            fclose(f);
        }
    } else {
        //cpu0 is always online
        cpu->online = ONLINE;
    }
    
}

//Update all core CPU usage, return the greatest usage
float updateAllCoreCPUUsage(struct CPU** cpuList, size_t coreCount) {
    char c;
    float SumUsage = 0;
    int updateCount = 0;
    FILE *stat = fopen(PATH_STAT, "r");
    if(stat == NULL) {
        perror("Error opening /proc/stat");
        exit(1);
    }
    READ_NEXT_LINE;
    //start update all core usage
    for(int i = 0; i < coreCount; i++) {
        
        //first should check if the line is start with "cpu%d"
        int cpuid;
        char name[3];
        //check if the line is start with "cpu"
        fread(name, 3, 1, stat);
        //printf("name=%s\n", name);
        if(name[0] != 'c' || name[1] != 'p' || name[2] != 'u') {
            //if not, break
            break;
        }
        
        //if true we need extract the '%d' and update its usage
        fscanf(stat, "%d", &cpuid);
        //if cpuid does not match i, the cpuList[i] is offline

        struct CPU_STAT cpuStat;
        fscanf(stat, "%lu %lu %lu %lu %lu %lu %lu", &cpuStat.user, &cpuStat.nice, &cpuStat.system, &cpuStat.idle, &cpuStat.iowait, &cpuStat.irq, &cpuStat.softirq);
        READ_NEXT_LINE;
        size_t totaldiff = (cpuStat.user + cpuStat.nice + cpuStat.system + cpuStat.idle + cpuStat.iowait + cpuStat.irq + cpuStat.softirq) - (cpuList[cpuid]->stat.user + cpuList[cpuid]->stat.nice + cpuList[cpuid]->stat.system + cpuList[cpuid]->stat.idle + cpuList[cpuid]->stat.iowait + cpuList[cpuid]->stat.irq + cpuList[cpuid]->stat.softirq);
        size_t idlediff = cpuStat.idle - cpuList[cpuid]->stat.idle;
        cpuList[cpuid]->usage = (totaldiff - idlediff) / (float)totaldiff;
        cpuList[cpuid]->stat.user = cpuStat.user;
        cpuList[cpuid]->stat.nice = cpuStat.nice;
        cpuList[cpuid]->stat.system = cpuStat.system;
        cpuList[cpuid]->stat.idle = cpuStat.idle;
        cpuList[cpuid]->stat.iowait = cpuStat.iowait;
        cpuList[cpuid]->stat.irq = cpuStat.irq;
        cpuList[cpuid]->stat.softirq = cpuStat.softirq;

        SumUsage += cpuList[cpuid]->usage;
        updateCount++;
    }
    fclose(stat);
    //start update all core online status and freq
    for(int i = 0; i < coreCount; i++) {
        controlCore(cpuList[i], QUERY_ONLINE_STATUS);
        controlCore(cpuList[i], QUERY_FREQ);
    }
    return SumUsage / updateCount;
    
}


void dynamicCoreControl(struct CPU** cpuList, size_t coreCount) {
    // float totalCPUUsage = getRealTimeTotalCPUUsage();
    // printf("totalCPUUsage=%.2f%%\n", totalCPUUsage * 100);
    float AvgUsage = updateAllCoreCPUUsage(cpuList, coreCount);
    printf("AvgUsage=%.2f%%\n", AvgUsage*100);

    int pCoreAllOnline = 1;
    int maxCurFreq = 0;
    float maxCPUFreqPercentage = 0;
    for(int i = 0; i < E_CORE_START; i++) {
        // Find the greatest freq and calculate the freq percentage
        if(cpuList[i]->freq > maxCurFreq) {
            maxCPUFreqPercentage = (float)(cpuList[i]->freq - cpuList[i]->minFreq) / (cpuList[i]->maxFreq - cpuList[i]->minFreq);
            maxCurFreq = cpuList[i]->freq;
        }
        if(cpuList[i]->online == 0) {
            pCoreAllOnline = 0;
        }
    }
    //For test
    //pCoreAllOnline = 1;

    //Control P Core
    for(int i = E_CORE_START - 1; i >= 1; i--) {
        if(cpuList[i]->online == 1 && cpuList[i]->usage < 0.10 && AvgUsage < 0.3) {
            printf("Disabling P core cpu%d \n", i);
            controlCore(cpuList[i], OFFLINE);
            cpuList[i]->usage = 0;
            cpuList[i]->freq = 0;
            break;
        }
    }
    
    //Control E Core
    for(int i = coreCount - 1; i >= E_CORE_START; i--) {
        if(!pCoreAllOnline && cpuList[i]->online == 1) {
            printf("Disabling E core cpu%d \n", i);
            controlCore(cpuList[i], OFFLINE);
            cpuList[i]->usage = 0;
            cpuList[i]->freq = 0;
        } else if(cpuList[i]->online == 1 && cpuList[i]->usage < 0.07) {
            printf("Disabling E core cpu%d \n", i);
            controlCore(cpuList[i], OFFLINE);
            cpuList[i]->usage = 0;
            cpuList[i]->freq = 0;
        }
    }

    //P Core is start from cpu0 to cpu[E_CORE_START - 1], E Core is start from cpu[E_CORE_START] to cpu[coreCount - 1]
    //If P core is all online, considering use E core
    
    if(AvgUsage > 0.8 && pCoreAllOnline) {
        for(int i = E_CORE_START; i < coreCount; i++) {
            if(cpuList[i]->online == 0) {
                printf("Enabling E core cpu%d \n", i);
                controlCore(cpuList[i], ONLINE);
                break;
            }
        } 
    }
    // I don't know why I considered the maxCPUFreqPercentage>0.8, maybe it's wrong
    if(AvgUsage > 0.3 && maxCPUFreqPercentage > 0.8) {
        for(int i = 1; i < E_CORE_START; i++) {
            if(cpuList[i]->online == 0) {
                printf("Enabling P core cpu%d \n", i);
                controlCore(cpuList[i], ONLINE);
                break;
            }
        }
    }

}

void signalHandler(int sig) {
    if(sig == SIGINT) {
        printf(" signal received, enabling all cores\n");
        stop = 1;
    }
}

int main()
{
    //catch Ctrl+C signal and enable all cores
    signal(SIGINT, signalHandler);
    size_t coreCount = getCoreCount();
    struct CPU **cpuList = initCPU(coreCount);
    memset(&TOTAL_CPU_STAT, 0, sizeof(struct CPU_STAT));
    while(1) {
        for(int i = 0; i < coreCount; i++) {
            printf("cpu%d: freq=%dMhz, usage=%.2f%%, online=%d\n", cpuList[i]->cpuid,(int)(cpuList[i]->freq/1000), cpuList[i]->usage * 100, cpuList[i]->online);
        }
        dynamicCoreControl(cpuList, coreCount);
        usleep(500000);
        if(stop) {
            break;
        }
    }
    for(int i = 1; i < coreCount; i++) {
        controlCore(cpuList[i], ONLINE);
    }
    return 0;
}