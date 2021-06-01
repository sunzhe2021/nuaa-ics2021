#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp() {
	if(free_ == NULL) {
		printf("no places!\n");
		return NULL;
	}
	WP *p = free_;
	free_ = free_->next;
	p->next = NULL;
	return p;
}

void free_wp(WP *wp) {
	WP *p = head;
	WP *pre = p;
	if(head == NULL || wp == NULL || wp->NO >= NR_WP) {
		printf("no watchpoint!\n");
	}
	if(head->NO == wp->NO) {
		head = head->next;
		p->next = free_;
		free_ = p;
	}
	else {
		while(p != NULL && p->NO != wp->NO) {
			pre = p;
			p = p->next;
		}
		if(p->NO == wp->NO) {
			pre->next = p->next;
			p->next = free_;
			free_ = p;
		}
		if(p == NULL) {
			printf("Can't find!\n");
		}
	}
}

int set_watchpoint(char *args) {
	bool success = true;
	uint32_t val = expr(args, &success);
	if( !success ) {
		printf("Failed to create watchpoint!\n");
		return -1;
	}
	WP *wp = new_wp();
	if(wp == NULL) {
		printf("Failed to create watchpoint!\n");
		return 0;
	}
	wp->old_val = val;
	strcpy(wp->expr, args);
	if(head == NULL) {
		wp->NO = 0;
		head = wp;
	}
	else {
		WP *newwp;
		newwp = head;
		while(newwp->next != NULL) {
			newwp = newwp->next;
		}
		wp->NO = newwp->NO +1;
		newwp->next = wp;
	}
	printf("Set watchpoint #%d\n", wp->NO);
	printf("expr           =%s\n", wp->expr);
	printf("old value  =  0x%x\n", wp->old_val);
	return wp->NO;
}

bool delete_watchpoint(int NO) {
	if( head == NULL) {
		printf("No watchpoint!\n");
		return false;
	}
	WP *wp = head;
	while(wp->next != NULL && wp->next->NO != NO) {
		wp = wp->next;
	}
	if(wp == NULL) {
		printf("Failed to find the watchpoint!\n");
	}
	else {
		free_wp(wp);
	}
	return true;
}

void list_watchpoint() {
	if(head == NULL) {
		//printf("There is no watchpoint!\n");
	}
	WP *wp;
	wp = head;
	printf("NO	Expr	Old Value \n");
	while(wp != NULL) {
		printf("%2d	%10s	0x%08x\n",wp->NO, wp->expr, wp->old_val);
		wp = wp->next;
	}
}

WP *scan_watchpoint() {
	WP *wp = head;
	if(wp == NULL) {
		//printf("There is no watchpoint!\n");
	}
	while(wp != NULL) {
		bool success = true;
		int val = expr(wp->expr, &success);
		if(val != wp->old_val) {
			printf("Hit watchpoint %d at address 0x%8s\n", wp->NO, wp->expr);
			printf("expr          = %s\n", wp->expr);
			printf("The old value = 0x%08x\n", wp->old_val);
			printf("The new value = 0x%08x\n", val);
			wp->new_val = val;
			return wp;
		}
		wp = wp->next;
	}
	//printf("program paused\n");
	return NULL;
}


