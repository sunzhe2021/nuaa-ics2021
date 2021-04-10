#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_TEN, TK_SIXTEEN, TK_REG, GE, LE, TK_EQ, TK_UEQ, POINTER, MINUS
  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
  int high_status;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE, 0},              // spaces
  {"\\t+", TK_NOTYPE, 0},            // tabs
  {"\\+", '+', 4},                   // plus
  {"==", TK_EQ, 3},                  // equal
  {"[0-9]+", TK_TEN, 0},             //decimalism
  {"0x[0-9a-f]+", TK_SIXTEEN, 0},    //hexadecimal
  {"\\$[a-ehilpx]{2,3}", TK_REG, 0}, //register
  {"\\(", '(', 7},                   //left_bracket
  {"\\)", ')', 7},                   //right_bracket
  {"\\-", '-', 4},                   //sub
  {"\\*", '*', 5},                   //mul
  {"\\/", '/', 5},                   //devide
  {"!=", TK_UEQ, 3},                 //not equal
  {"&&", '&', 2},                    //and
  {"\\|\\|", '|', 1},                //or
  {"!", '!', 6},                     //not
  {">", '>', 3},                     //greater
  {"<", '<', 3},                     //lower
  {">=", GE, 3},                     //greater or equal
  {"<=", LE, 3},                     //lower or equal  
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
  int high_status;
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
	char *temp = e + position + 1;
        int substr_len = pmatch.rm_eo;
	position += substr_len;
        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
		case TK_NOTYPE: break;
		case TK_REG: 
			tokens[nr_token].type = rules[i].token_type;
			tokens[nr_token].high_status = rules[i].high_status;
			strncpy(tokens[nr_token].str,temp,substr_len-1);
			tokens[nr_token].str[substr_len-1]='\0';
			nr_token ++;
			break;
	        default:
			tokens[nr_token].type = rules[i].token_type;
			tokens[nr_token].high_status = rules[i].high_status;
			strncpy(tokens[nr_token].str,substr_start,substr_len);
			tokens[nr_token].str[substr_len]='\0';
			nr_token ++;
			break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool parentheses_check(int m, int n) {
	int i;
	if(tokens[m].type == '(' && tokens[n].type == ')') {
		int lp = 0, rp = 0;
		for(i = m + 1; i < n; i++) {
			if(tokens[i].type == '(')
				lp++;
			else if(tokens[i].type == ')')
				rp++;
			else if(rp > lp)
				return false;
		}
		if(lp == rp)
			return true;
	}
	return false;
}

int find_central_op(int m, int n) {
	int i;
	int min_status = 10;
	int oper = m;
	int count = 0;
	for(i = m; i <= n; i++) {
		if(tokens[i].type == TK_TEN ||tokens[i].type == TK_SIXTEEN ||tokens[i].type == TK_REG )
			continue;
		if(tokens[i].type == '(')
			count++;
		if(tokens[i].type == ')')
			count--;
		if(count != 0)
			continue;
		if(tokens[i].high_status <= min_status) {
			min_status = tokens[i].high_status;
			oper = i;
		}
	}
	return oper;
}

uint32_t eval(int m, int n) {
	if(m > n) {
		return 0;
	}
	if(m == n) {
		uint32_t num = 0;
		if(tokens[m].type == TK_TEN)
			sscanf(tokens[m].str, "%d", &num);
		else if(tokens[m].type == TK_SIXTEEN)
			sscanf(tokens[m].str, "%x", &num);
		else if(tokens[m].type == TK_REG) {
			int i;
			for(i = 0; i < 4; i++) {
				tokens[m].str[i] = tokens[m].str[i + 1];
			}
			if(strcmp(tokens[m].str, "eip") == 0)
				num = cpu.eip;
			else {
				for(i = 0; i < 8; i++) {
					if(strcmp(tokens[m].str,regsl[i]) == 0) {
						num = cpu.gpr[i]._32;
						break;
					}
				}
			}
		}
		else if(parentheses_check(m, n) == true) {
			return eval(m+1, n-1);
		}
		else {
			int op = find_central_op(m, n);
			if(m == op) {
				uint32_t val = eval(m+1, n);
				switch(tokens[m].type) {
					case '!': return !val;
					default: return -1;
				}
			}
			uint32_t value1 = eval(m, op - 1);
			uint32_t value2 = eval(op + 1, n);
			switch(tokens[op].type) {
				case '+': return value1 + value2;
				case '-': return value1 - value2;
				case '*': return value1 * value2;
				case '/': return value1 / value2;
				case TK_EQ: return value1 == value2;
				case TK_UEQ: return value1 != value2;
				case '>': return value1 > value2;
				case '<': return value1 < value2;
				case GE: return value1 >= value2;
				case LE: return value1 <= value2;
				case '&': return value1 && value2;
				case '|': return value1 || value2;
				default: return -1;
			}
		}
	}
	return 0;
}


uint32_t expr(char *e, bool *success) {
	if (!make_token(e)) {
		*success = false;
		return 0;
	}
	/* TODO: Insert codes to evaluate the expression. */
	//TODO();
	int i;
	for(i = 0; i < nr_token; i++ ) {
		if(tokens[i].type == '*' &&( i == 0 || (tokens[i - 1].type != TK_TEN && tokens[i - 1].type != TK_SIXTEEN && tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
			tokens[i].type = POINTER;	//定义指针
			tokens[i].high_status = 6;
		}
		if(tokens[i].type == '-' &&( i == 0 || (tokens[i - 1].type != TK_TEN && tokens[i - 1].type != TK_SIXTEEN && tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
			tokens[i].type = MINUS;		//定义负数
			tokens[i].high_status = 6;
		}
	}
	int result;
	result = eval(0, nr_token - 1);
	return result;
}
