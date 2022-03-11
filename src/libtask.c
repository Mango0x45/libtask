/*
 * BSD Zero Clause License
 *
 * Copyright (c) 2022 Thomas Voss
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "task.h"

#define EOK 0
#define SKPSPC(s)                                                                                  \
	do {                                                                                       \
		while (isspace(*s))                                                                \
			s++;                                                                       \
	} while (0)

static bool header(char *s);
static int parsehead(char *s, struct task *tsk);
static int parsetitle(char *s, struct task *tsk);
static int parseauthor(char *s, struct task *tsk);
static int parsetframe(char *s, struct task *tsk);
static int appendbody(char *s, ssize_t len, struct task *tsk);
static int timecmp(struct tm t1, struct tm t2);
static bool timenull(struct tm t1);
static void timewrite(FILE *fp, struct tm t);
static size_t max(size_t a, size_t b);

void
taskfree(struct task task)
{
	int i;

	free(task.title);
	free(task.body);
	for (i = 0; i < task.author_cnt; i++)
		free(task.authors[i]);
	free(task.authors);
}

int
taskwrite(FILE *stream, struct task task)
{
	int i, cmp;
	size_t j, len, mx = 0;
	const int tgpad = 13; /* strlen("Time Frame:  ")              */
	const int ftpad = 9;  /* strlen("From  to ")                  */
	const int aupad = 6;  /* strlen("After ") || strlen("Until ") */
	const int onpad = 3;  /* strlen("On ")                        */
	const int tslen = 16; /* strlen("HH:MM YYYY-MM-DD")           */
	enum timetype {
		AFTER,
		FROM_TO,
		ON,
		UNTIL
	} tt;

	mx = strlen(task.title);
	for (i = 0; i < task.author_cnt; i++)
		mx = max(mx, strlen(task.authors[i]));

	if ((cmp = timecmp(task.start, task.end)) == 0) {
		tt = ON;
		mx = max(mx, tslen + onpad);
	} else if (timenull(task.end)) {
		tt = AFTER;
		mx = max(mx, tslen + aupad);
	} else if (timenull(task.start)) {
		tt = UNTIL;
		mx = max(mx, tslen + aupad);
	} else if (cmp < 0) {
		tt = FROM_TO;
		mx = max(mx, 2 * tslen + ftpad);
	} else
		return EINVAL;

	len = tgpad + mx;
	for (j = 0; j < len; j++)
		fputc('-', stream);
	fputc('\n', stream);

	fprintf(stream, "Title:       %s\n", task.title);
	for (i = 0; i < task.author_cnt; i++)
		fprintf(stream, "Author:      %s\n", task.authors[i]);

	switch (tt) {
	case AFTER:
		fputs("Time Frame:  After ", stream);
		timewrite(stream, task.start);
		break;
	case FROM_TO:
		fputs("Time Frame:  From ", stream);
		timewrite(stream, task.start);
		fputs(" to ", stream);
		timewrite(stream, task.end);
		break;
	case ON:
		fputs("Time Frame:  On ", stream);
		timewrite(stream, task.start);
		break;
	case UNTIL:
		fputs("Time Frame:  Until ", stream);
		timewrite(stream, task.end);
	}

	fputc('\n', stream);
	for (j = 0; j < len; j++)
		fputc('-', stream);
	if (task.body != NULL)
		fprintf(stream, "\n\n%s", task.body);
	else
		fputc('\n', stream);

	return EOK;
}

int
taskread(FILE *stream, struct task *task)
{
	int rval = EOK, lineno = 0;
	bool hasbody = false, inhead = true;
	char *line = NULL;
	char bodybuf[BUFSIZ + 1] = {0};
	size_t len = 0;
	ssize_t nr;

	memset(task, 0, sizeof(struct task));

	while ((nr = getline(&line, &len, stream)) > 0) {
		line[nr - 1] = '\0';
		if (lineno == 0 && !header(line)) {
			rval = EBADMSG;
			goto out;
		} else if (lineno > 0) {
			if (header(line)) {
				if (task->title == NULL
						|| (timenull(task->start) && timenull(task->end))) {
					rval = EBADMSG;
					goto out;
				}
				inhead = false;
			} else if (!inhead) {
				if (*line != '\0') {
					rval = EBADMSG;
					goto out;
				}
				hasbody = true;
				break;
			} else if (inhead && (rval = parsehead(line, task)) != EOK)
				goto out;
		}

		lineno++;
	}
	if (nr == -1) {
		rval = errno;
		goto out;
	}

	while ((len = fread(bodybuf, sizeof(char), BUFSIZ, stream)) > 0) {
		bodybuf[len] = '\0';
		if ((rval = appendbody(bodybuf, len, task)) != EOK)
			goto out;

		if (len < BUFSIZ) {
			if (ferror(stream)) {
				rval = errno;
				goto out;
			}
			break;
		}
	}

	if (hasbody && task->body == NULL)
		rval = EBADMSG;
out:
	free(line);
	return rval;
}

bool
header(char *s)
{
	char *p = s;
	while (*p != '\0' && *p == '-')
		p++;
	return *p == '\0' && s != p;
}

int
parsehead(char *s, struct task *tsk)
{
	return strncmp(s, "Title:", 6)       == 0 ? parsetitle(s + 6, tsk)   :
	       strncmp(s, "Author:", 7)      == 0 ? parseauthor(s + 7, tsk)  :
	       strncmp(s, "Time Frame:", 11) == 0 ? parsetframe(s + 11, tsk) :
						    EBADMSG;
}

int
parsetitle(char *s, struct task *tsk)
{
	size_t len;

	SKPSPC(s);
	if ((len = strlen(s)) == 0)
		return EBADMSG;
	if ((tsk->title = malloc(sizeof(char) * len + 1)) == NULL)
		return errno;
	strcpy(tsk->title, s);

	return EOK;
}

int
parseauthor(char *s, struct task *tsk)
{
	bool inspc = false;
	size_t len;
	char *p1, *p2;

	SKPSPC(s);
	p1 = p2 = s;
	while (*p1) {
		if (isspace(*p1)) {
			if (inspc)
				SKPSPC(p1);
			else {
				inspc = true;
				*p1 = ' ';
			}
		} else
			inspc = false;
		*p2++ = *p1++;
	}
	*p2 = '\0';

	if ((len = strlen(s)) == 0)
		return EBADMSG;

	if (tsk->authors == NULL) {
		if ((tsk->authors = malloc(sizeof(char *) * 2)) == NULL)
			return errno;
		tsk->_author_cap = 1;
		tsk->author_cnt = 0;
		tsk->authors[1] = NULL;
	} else if (tsk->_author_cap == tsk->author_cnt) {
		int oaucap = tsk->_author_cap;

		tsk->_author_cap *= 2;
		if ((tsk->authors = realloc(tsk->authors, sizeof(char *) * tsk->_author_cap + 1))
				== NULL)
			return errno;
		memset(tsk->authors + oaucap, 0, tsk->_author_cap - oaucap);
	}

	if ((tsk->authors[tsk->author_cnt] = malloc(sizeof(char) * len + 1)) == NULL)
		return errno;
	strcpy(tsk->authors[tsk->author_cnt++], s);

	return EOK;
}

int
parsetframe(char *s, struct task *tsk)
{
	SKPSPC(s);

	if (strncmp(s, "Until", 5) == 0) {
		if ((s = strptime(s, "Until %H:%M %Y-%m-%d", &tsk->end)) == NULL)
			return EBADMSG;
	} else if (strncmp(s, "After", 5) == 0) {
		if ((s = strptime(s, "After %H:%M %Y-%m-%d", &tsk->start)) == NULL)
			return EBADMSG;
	} else if (strncmp(s, "On", 2) == 0) {
		if ((s = strptime(s, "On %H:%M %Y-%m-%d", &tsk->start)) == NULL)
			return EBADMSG;
		tsk->end = tsk->start;
	} else if (strncmp(s, "From", 4) == 0) {
		if ((s = strptime(s, "From %H:%M %Y-%m-%d to ", &tsk->start)) == NULL)
			return EBADMSG;
		if ((s = strptime(s, "%H:%M %Y-%m-%d", &tsk->end)) == NULL)
			return EBADMSG;
		if (timecmp(tsk->start, tsk->end) >= 0)
			return EBADMSG;
	} else
		return EBADMSG;

	SKPSPC(s);
	return *s == '\0' ? EOK : EBADMSG;
}

int
appendbody(char *s, ssize_t len, struct task *tsk)
{
	if (tsk->body == NULL) {
		tsk->_body_cap = max(256, len);
		if ((tsk->body = calloc(tsk->_body_cap + 1, sizeof(char))) == NULL)
			return errno;
	}
	while (tsk->body_len + len > tsk->_body_cap) {
		tsk->_body_cap *= 2;
		if ((tsk->body = realloc(tsk->body, sizeof(char) * tsk->_body_cap + 1)) == NULL)
			return errno;
	}

	strcat(tsk->body, s);
	tsk->body_len += len;
	return EOK;
}

int
timecmp(struct tm t1, struct tm t2)
{
	return t1.tm_year < t2.tm_year ? -1 :
	       t1.tm_year > t2.tm_year ? +1 :
	       t1.tm_mon  < t2.tm_mon  ? -1 :
	       t1.tm_mon  > t2.tm_mon  ? +1 :
	       t1.tm_mday < t2.tm_mday ? -1 :
	       t1.tm_mday > t2.tm_mday ? +1 :
	       t1.tm_hour < t2.tm_hour ? -1 :
	       t1.tm_hour > t2.tm_hour ? +1 :
	       t1.tm_min  - t2.tm_min;
}

bool
timenull(struct tm t1)
{
	struct tm t2 = {0};
	return memcmp(&t1, &t2, sizeof(struct tm)) == 0;
}

void
timewrite(FILE *fp, struct tm t)
{
	fprintf(fp, "%02d:%02d %04d-%02d-%02d", t.tm_hour, t.tm_min, t.tm_year + 1900, t.tm_mon + 1,
		t.tm_mday);
}

size_t
max(size_t a, size_t b)
{
	return a > b ? a : b;
}
