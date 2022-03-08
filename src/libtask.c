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
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "task.h"

#define EOK 0
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SKPSPC(s)				\
	do {					\
		while (isspace(*s))		\
			s++;			\
	} while (0)

static bool header(char *s);
static int parsehead(char *s, struct task *tsk);
static int parsetitle(char *s, struct task *tsk);
static int parseauthor(char *s, struct task *tsk);
static int parsetframe(char *s, struct task *tsk);
static int appendbody(char *s, ssize_t len, struct task *tsk);

int
readtask(FILE *fp, struct task *tsk)
{
	int err, lineno = 0;
	bool inhead = true;
	char *line = NULL;
	size_t len = 0;
	ssize_t nr;

	memset(tsk, 0, sizeof(struct task));

	while ((nr = getline(&line, &len, fp)) > 0) {
		line[nr - 1] = '\0';
		if (lineno == 0 && !header(line))
			return EBADMSG;
		else if (lineno > 0) {
			if (header(line)) {
				if (tsk->title == NULL)
					return EBADMSG;
				inhead = false;
			} else if (!inhead) {
				if (*line != '\0')
					return EBADMSG;
				break;
			} else if (inhead && (err = parsehead(line, tsk)) != EOK)
				return err;
		}

		lineno++;
	}

	if (nr == -1)
		return errno;

	while ((nr = getline(&line, &len, fp)) > 0) {
		if ((err = appendbody(line, nr, tsk)) != EOK)
			return err;
	}

	if (nr == -1)
		return errno;

	free(line);
	return EOK;
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
	return strncmp(s, "Title:", 6) == 0 ? parsetitle(s + 6, tsk)
		: strncmp(s, "Author:", 7) == 0 ? parseauthor(s + 7, tsk)
		: strncmp(s, "Time Frame:", 11) == 0 ? parsetframe(s + 11, tsk)
		: EBADMSG;
}

int
parsetitle(char *s, struct task *tsk)
{
	size_t len;

	SKPSPC(s);
	len = strlen(s);
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

	len = strlen(s);

	if (tsk->authors == NULL) {
		if ((tsk->authors = malloc(sizeof(char *) * 2)) == NULL)
			return errno;
		tsk->_author_cap = 1;
		tsk->author_cnt = 0;
		tsk->authors[1] = NULL;
	} else if (tsk->_author_cap == tsk->author_cnt) {
		int oaucap = tsk->_author_cap;

		tsk->_author_cap *= 2;
		if ((tsk->authors = realloc(tsk->authors,
					    sizeof(char *) * tsk->_author_cap + 1)) == NULL)
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
	} else
		return EBADMSG;

	SKPSPC(s);
	return *s == '\0' ? EOK : EBADMSG;
}

int
appendbody(char *s, ssize_t len, struct task *tsk)
{
	if (tsk->body == NULL) {
		tsk->_body_cap = MAX(256, len);
		if ((tsk->body = malloc(tsk->_body_cap + 1)) == NULL)
			return errno;
	}
	while (tsk->body_len + len > tsk->_body_cap) {
		tsk->_body_cap *= 2;
		if ((tsk->body = realloc(tsk->body, sizeof(char) * tsk->_body_cap + 1)) == NULL)
			return errno;
	}

	strcat(tsk->body, s);
	return EOK;
}
