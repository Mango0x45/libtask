#ifndef TASK_H
#define TASK_H

#include <stdio.h>
#include <time.h>

struct task {
	char *title, *body;
	char **authors;
	int author_cnt, _author_cap;
	int body_len, _body_cap;
	struct tm start, end;
};

int readtask(FILE *fp, struct task *tsk);

#endif /* !TASK_H */
