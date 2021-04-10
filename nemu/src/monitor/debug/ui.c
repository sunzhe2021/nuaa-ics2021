#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static int cmd_p(char *args);
static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Go on some steps", cmd_si },
  { "info", "Print register status", cmd_info },
  { "x", "Scan memory", cmd_x },
  { "p", "Calculate the expr", cmd_p },
  /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
	char *arg = strtok(NULL, " ");
	int n = 0;
	if(arg == NULL)
		cpu_exec(1);
	else {
		sscanf(arg, "%d", &n);
		if(n <= 1000000) {
			if(n == -1){
				cpu_exec(-1);
				return 0;
			}
			for(int i = 0; i < n; ++i) {
				cpu_exec(1);
			}
		}
		else
			printf("Input Error");
	}
	return 1;
}

void print_register() {
	int i = 0, j = 0;
	for(i = 0; i < 8; i++) {
		printf("%s  %x\n", regsl[i], cpu.gpr[i]._32);
	}
	for(i = 0; i < 8; i++) {
		printf("%s  %x\n", regsw[i], cpu.gpr[i]._16);
	}
	for(i = 0; i < 8; i++) {
		for(j = 0; j < 2; j++) {
			printf("%s %x\n", regsb[i], cpu.gpr[i]._8[j]);
		}
	}
}

static int cmd_info(char *args) {
	char *arg = strtok(NULL, " ");
	if(strcmp(arg,"r") == 0)
		print_register();
	else
		printf("Illegal Input");
	return 1;
}

static int cmd_x(char *args) {
/*
	char *arg1 = strtok(NULL, " ");
	char *arg2 = strtok(NULL, " ");
	int num, i;
	vaddr_t addr;
	sscanf(arg1, "%d", &num);
	sscanf(arg2, "%x", &addr);
	printf("Address    Dword block ... Byte Sequence\n");
	for(i = 0; i < num; i++) {
		uint32_t data = vaddr_read(addr , 4);
		printf("0x00%x ", addr);
		printf("0x%08x ", data);
		printf(" ... ");
		for( int j = 0; j < 4; ++j ) {
			printf("%02x ", data & 0xff);
			data = data >> 8;
		}
		addr += 4;
		printf("\n");
	}
	return 1;
*/
	vaddr_t point;
	int i, j, k, time;
	k = 0;
	char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");
        sscanf(arg1, "%d", &time);
	if(arg2[0] != '0') {
		bool flag = true;
		point = expr(arg2, &flag);
		if(flag == false) {
			printf("Fail!\n");
			return 0;
		}
	}
	else
		sscanf(arg2, "%x", &point);
        printf("Address    Dword block ... Byte Sequence\n");
        for(i = 0; i < time; i++) {
		uint32_t data = vaddr_read(point, 4);
                printf("0x00%x     ", point);
                printf("0x%08x	", data);
        	printf(" ... ");
                for( j = 1; j <= 4; ++j) {
			vaddr_t temp = vaddr_read(point, j);
			int mov = (k << 3);
			temp = temp >> mov;
                        printf("%02x ", temp);
                        k++;
                }
                printf("\n");
		point += 4;
        }
        return 1;
}

static int cmd_p(char *args) {
	char *arg = strtok(NULL, " ");
	bool flag = true;
	if(arg == NULL) {
		printf("Instruction requires experssion.\n");
		return 0;
	}
	uint32_t value;
	value = expr(arg, &flag);
	if(flag) {
		printf("result:%d\t(0x%x)\n", value, value);
	}
	else {
		printf("make token error!\n");
	}
	return 1;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
