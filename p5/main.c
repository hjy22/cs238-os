/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include <signal.h>
#include "system.h"

typedef struct
{
	unsigned long mem_free;
	double memory_usage;
} MemoryInfo;

typedef struct
{
	unsigned long packets_sent;
	unsigned long packets_receive;
} PacketsInfo;

static volatile int done;

static void
_signal_(int signum)
{
	assert(SIGINT == signum);

	done = 1;
}

double
cpu_util(const char *s)
{
	static unsigned sum_, vector_[7];
	unsigned sum, vector[7];
	const char *p;
	double util;
	uint64_t i;

	if (!(p = strstr(s, " ")) ||
		(7 != sscanf(p,
					 "%u %u %u %u %u %u %u",
					 &vector[0],
					 &vector[1],
					 &vector[2],
					 &vector[3],
					 &vector[4],
					 &vector[5],
					 &vector[6])))
	{
		return 0;
	}
	sum = 0.0;
	for (i = 0; i < ARRAY_SIZE(vector); ++i)
	{
		sum += vector[i];
	}
	util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
	sum_ = sum;
	for (i = 0; i < ARRAY_SIZE(vector); ++i)
	{
		vector_[i] = vector[i];
	}
	return util;
}

MemoryInfo
memory_util()
{
	const char *const PROC_MEMINFO = "/proc/meminfo";
	char line[1024];
	FILE *file;
	unsigned long mem_total = 0.0;
	unsigned long mem_free = 0.0;
	MemoryInfo memory_info = {0, 0};

	if (!(file = fopen(PROC_MEMINFO, "r")))
	{
		TRACE("fopen()");
		return memory_info;
	}

	while (fgets(line, sizeof(line), file))
	{
		if (strstr(line, "MemTotal") != NULL)
		{
			sscanf(line, "MemTotal: %lu kB\n", &mem_total);
		}
		if (strstr(line, "MemFree") != NULL)
		{
			if (sscanf(line, "MemFree: %lu kB", &mem_free))
			{
				break;
			}
		}
	}
	fclose(file);
	memory_info.mem_free = mem_free / 1024;
	memory_info.memory_usage = 100.0 - (((double)mem_free / mem_total) * 100.0);
	return memory_info;
}

int load_util()
{
	const char *const PROC_LOADAVG = "/proc/loadavg";
	char line[1024];
	FILE *file;
	int running_processes = 0;

	if (!(file = fopen(PROC_LOADAVG, "r")))
	{
		TRACE("fopen()");
		return -1;
	}

	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, "%*s %*s %*s %d/%*d", &running_processes))
		{
			break;
		}
	}
	fclose(file);
	return running_processes;
}
PacketsInfo packets_util()
{
	const char *const PROC_PACKET = "/proc/net/dev";
	FILE *file;
	PacketsInfo packet_info = {0, 0};
	char line[1024];

	if (!(file = fopen(PROC_PACKET, "r")))
	{
		perror("Error opening /proc/net/dev");
		return packet_info;
	}
	while (fgets(line, sizeof(line), file))
	{
		if (strstr(line, "ens4") != NULL)
		{
			if (sscanf(line, "  ens4: %*d %lu %*d %*d %*d %*d %*d %*d %*d %lu",
					   &packet_info.packets_receive, &packet_info.packets_sent) == 2)
			{
				break;
			}
		}
	}
	fclose(file);
	return packet_info;
}

int main(int argc, char *argv[])
{
	const char *const PROC_STAT = "/proc/stat";
	char line[1024];
	FILE *file;
	PacketsInfo packet_info;
	MemoryInfo memory_info;
	int running_proccess;

	UNUSED(argc);
	UNUSED(argv);

	if (SIG_ERR == signal(SIGINT, _signal_))
	{
		TRACE("signal()");
		return -1;
	}
	printf("CPU		| Mem Free	| Mem Usage	| Run PROC	| PKG Receive	| PKG Send\n");
	while (!done)
	{
		if (!(file = fopen(PROC_STAT, "r")))
		{
			TRACE("fopen()");
			return -1;
		}
		if (fgets(line, sizeof(line), file))
		{
			printf("%5.1f%%	| ", cpu_util(line));
			fflush(stdout);
		}
		memory_info = memory_util();
		packet_info = packets_util();
		running_proccess = load_util();
		printf("%ldMB			| 	", memory_info.mem_free);
		printf("%.2f%%	| ", memory_info.memory_usage);
		printf("%d		|", running_proccess);
		printf(" %ld	| ", packet_info.packets_receive);
		printf("%ld\r", packet_info.packets_sent);
		fflush(stdout);
		us_sleep(500000);
		fclose(file);
	}
	printf("\nDone!   \n");
	return 0;
}
