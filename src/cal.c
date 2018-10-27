#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloca.h>

typedef unsigned long cal_t;

struct cal_date {
	cal_t day; /**< Day in the month, first is 1 */
	cal_t month; /**< Month of the year, between 1 and 12 */
	cal_t year; /**< Year, between 1 and 9999 */
};

struct cal_moninfo {
	cal_t fwday; /**< Index in a week of the first day of the month range, Sunday = 0 */
	cal_t lday; /**< Index of the last day of the month (first is 1) */
};

/**
 * Init the minfo structure
 * with t's month informations
 * @param minfo Pointer to a valid struct cal_minfo to fill
 * @param t Pointer to a valid struct tm containg the interesting month and year
 */
static void
cal_moninfo_init(struct cal_moninfo *minfo,
	const struct cal_date *date) {
	struct tm timeinfo = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 12,
		.tm_mday = 1,
		.tm_mon = date->month - 1,
		.tm_year = date->year - 1900,
	};

	/* Determine beginning of month infos */
	mktime(&timeinfo);

	minfo->fwday = timeinfo.tm_wday;

	/* Determine end of month infos */
	timeinfo.tm_mon += 1;
	timeinfo.tm_mday = 0;

	mktime(&timeinfo);

	minfo->lday = timeinfo.tm_mday;
}

/**
 * string centered formatted time
 */
static void
cal_fillftime(char *buffer,
	size_t bufferlen,
	const char *format,
	const struct cal_date *date) {
	const struct tm timeinfo = {
		.tm_mday = date->day,
		.tm_mon = date->month - 1,
		.tm_year = date->year - 1900,
	};
	char *ftime = alloca(bufferlen);
	size_t length = strftime(ftime, bufferlen, format, &timeinfo);
	size_t offset = (bufferlen - length) / 2;

	memset(buffer, ' ', offset);
	length = strlen(strncpy(buffer + offset, ftime, bufferlen - offset));
	memset(buffer + offset + length, ' ', bufferlen - length - offset);
}

/**
 * Fills buffer with a description of the week in 20 chars
 * @param buffer A buffer allocated >= 20 which will be filled
 * @return A pointer to the character following the last written
 */
static char *
cal_fillweek(char *buffer) {
	struct tm t = { .tm_wday = 0 };

	for(; t.tm_wday < 6; t.tm_wday += 1, buffer += 3) {
		strftime(buffer, 3, "%a", &t);
		buffer[2] = ' ';
	}
	strftime(buffer, 2, "%a", &t);
	buffer += 2;

	return buffer;
}

static void
cal_sday(char *buffer,
	cal_t day) {
	const char decimal = day / 10 + '0';
	const char digit = day % 10 + '0';

	buffer[0] = decimal == '0' ? ' ' : decimal;
	buffer[1] = digit;
}

static cal_t
cal_filldays(char *buffer,
	const struct cal_moninfo *minfo,
	cal_t step) {
	char * const end = buffer + 20;

	if(step == 1) {
		size_t blanks = minfo->fwday * 3;
		memset(buffer, ' ', blanks);
		buffer += blanks;
	}

	char *spaces = buffer + 2;
	while(spaces < end) {
		*spaces = ' ';
		spaces += 3;
	}

	for(; step <= minfo->lday && buffer < end;
		step += 1, buffer += 3) {

		cal_sday(buffer, step);
	}

	if(buffer < end) {
		memset(buffer, ' ', end - buffer);
	}

	return step;
}

static void
cal_print_month(const struct cal_date *date) {
	const size_t bufferlen = 21;
	char * const buffer = alloca(bufferlen);
	buffer[bufferlen - 1] = '\0';

	cal_fillftime(buffer, bufferlen - 1, "%B %Y", date);
	puts(buffer);

	cal_fillweek(buffer);
	puts(buffer);

	struct cal_moninfo minfo;
	cal_t step = 1;
	cal_moninfo_init(&minfo, date);

	for(int i = 0; i < 6; i += 1) {
		step = cal_filldays(buffer, &minfo, step);
		puts(buffer);
	}
}

static void
cal_print_year(const struct cal_date *date) {
	const size_t bufferlen = 65;
	char * const buffer = alloca(bufferlen);
	char * const weeks = alloca(bufferlen);
	buffer[bufferlen - 1] = '\0';
	weeks[bufferlen - 1] = '\0';

	{
		char *w = cal_fillweek(weeks);
		w[0] = ' '; w[1] = ' ';
		memcpy(w + 2, weeks, 20);
		w[22] = ' '; w[23] = ' ';
		memcpy(w + 24, weeks, 20);
	}

	cal_fillftime(buffer, bufferlen - 1, "%Y", date);
	puts(buffer);

	struct cal_date d = {
		.day = 1,
		.month = 0,
		.year = date->year
	};

	while(d.month != 12) {
		char *b = buffer;
		struct cal_moninfo minfos[3];

		d.month += 1;
		cal_moninfo_init(minfos, &d);
		cal_fillftime(b, 20, "%B", &d);
		b += 22;

		d.month += 1;
		cal_moninfo_init(minfos + 1, &d);
		cal_fillftime(b, 20, "%B", &d);
		b += 22;

		d.month += 1;
		cal_moninfo_init(minfos + 2, &d);
		cal_fillftime(b, 20, "%B", &d);

		puts(buffer);

		puts(weeks);

		cal_t steps[3] = { 1, 1, 1 };

		for(int i = 0; i < 6; i += 1) {
			steps[0] = cal_filldays(buffer, minfos, steps[0]);
			steps[1] = cal_filldays(buffer + 22, minfos + 1, steps[1]);
			steps[2] = cal_filldays(buffer + 44, minfos + 2, steps[2]);

			puts(buffer);
		}
	}
}

static void
cal_usage(const char *calname) {
	fprintf(stderr, "usage: %s [[month] year]\n", calname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	struct cal_date date = { 0 };

	if(argc > 3) {
		cal_usage(*argv);
	} else if(argc != 1) {
		char **argpos = argv + 1;
		date.day = 0;

		if(argc == 3) {
			date.month = strtoul(*argpos, NULL, 10);

			if(date.month < 1 || date.month > 12) {
				cal_usage(*argv);
			}

			argpos += 1;
		}

		date.year = strtoul(*argpos, NULL, 10);
		if(date.year < 1 || date.year > 9999) {
			cal_usage(*argv);
		}
	} else {
		const time_t timestamp = time(NULL);
		const struct tm *timeinfo = localtime(&timestamp);

		date.day = timeinfo->tm_mday;
		date.month = timeinfo->tm_mon + 1;
		date.year = timeinfo->tm_year + 1900;
	}

	if(argc != 2) {
		cal_print_month(&date);
	} else {
		cal_print_year(&date);
	}

	return 0;
}

