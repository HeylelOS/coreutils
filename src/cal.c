#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloca.h>

typedef unsigned long cal_t;

enum cal_format {
	CalendarFormatYear,
	CalendarFormatMonth,
	CalendarFormatMonthYear
};

struct cal_date {
	cal_t day; /**< Day of the month, first is 1 */
	cal_t month; /**< Month of the year, between 1 and 12 */
	cal_t year; /**< Year, between 1 and 9999 */
};

struct cal_moninfo {
	cal_t fwday; /**< Index in a week of the first day of the month range, Sunday = 0 */
	cal_t lday; /**< Index of the last day of the month (first is 1) */
};

/**
 * Init the minfo structure with date.month informations
 * @param minfo Pointer to a valid struct cal_minfo to fill
 * @param date Pointer to a valid struct cal_date containg the
 * interesting month and year
 */
static void
cal_moninfo_init(struct cal_moninfo *minfo,
	const struct cal_date *date) {

	/* Determine the week with Gauss' formula */
	long w, y, c, Y, d, m;

	m = date->month - 2;
	d = date->day;
	Y = date->year;
	if(m <= 0) {
		m += 12;
		Y -= 1;
	}
	c = Y / 100;
	y = Y % 100;
	w = (d + (13 * m - 1) / 5 + y + y / 4 + c / 4 - 2 * c) % 7;

	minfo->fwday = w < 0 ? w + 7 : w;

	/* Determine the last day of month */
	static const long gregorian[] = {
		0 /* UNUSED */, 31, 28 /* UNUSED */, 31,
		30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if(date->month != 2) {
		minfo->lday = gregorian[date->month];
	} else {
		/* Handle leap year */
		if((date->year % 4 == 0
			&& date->year % 100 != 0)
			|| date->year % 400 == 0) {

			minfo->lday = 29;
		} else {
			minfo->lday = 28;
		}
	}
}

/**
 * string centered formatted time
 */
static void
cal_fillftime(char *buffer,
	size_t bufferlen,
	enum cal_format f,
	const struct cal_date *date) {
	const struct tm timeinfo = {
		.tm_mday = 1,
		.tm_mon = date->month - 1,
		.tm_year = date->year - 1900,
	};
	char *ftime = alloca(bufferlen);
	const char *format;

	if(f == CalendarFormatYear) {
		format = "%Y";
	} else if(f == CalendarFormatMonth) {
		format = "%B";
	} else { /* CalendarFormatMonthYear */
		format = "%B %Y";
	}

	size_t length = strftime(ftime, bufferlen, format, &timeinfo);
	const size_t offset = (bufferlen - length) / 2;

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
		strftime(buffer, 2, "%a", &t);
		buffer[2] = ' ';
	}
	strftime(buffer, 2, "%a", &t);
	buffer += 2;

	return buffer;
}

/**
 * Prints an equivalent of the format "%2lu" in
 * buffer, not appending a NUL character
 * @param buffer Valid pointer to a >= 2 allocated buffer
 * @param day The value to print
 */
static inline void
cal_sday(char *buffer,
	cal_t day) {
	const char decimal = day / 10 + '0';
	const char digit = day % 10 + '0';

	buffer[0] = decimal == '0' ? ' ' : decimal;
	buffer[1] = digit;
}

/**
 * Used to print days of a week of a month in a buffer >= 20,
 * The buffer filled with spaces before and after the valid week days,
 * Note spaces between following dates won't be re-written.
 * @param buffer A valid pointer to a >= 20 allocated buffer
 * @param minfo Valid pointer to a cal_moninfo of the month being printed
 * @param step Where we are in the month
 * @return The new step
 */
static cal_t
cal_filldays(char *buffer,
	const struct cal_moninfo *minfo,
	cal_t step) {
	char * const end = buffer + 20;

	if(step == 1) {
		const size_t blanks = minfo->fwday * 3;
		memset(buffer, ' ', blanks);
		buffer += blanks;

		char *spaces = buffer + 2;
		while(spaces < end) {
			*spaces = ' ';
			spaces += 3;
		}
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

/**
 * Prints date's month description on standard output
 * @param date Pointer to the date described, date.day unused
 */
static void
cal_print_month(const struct cal_date *date) {
	const size_t bufferlen = 21;
	char * const buffer = alloca(bufferlen);
	buffer[bufferlen - 1] = '\0';

	cal_fillftime(buffer, bufferlen - 1,
		CalendarFormatMonthYear, date);
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

/**
 * Prints date's year description on standard output
 * @param date Pointer to the date described, date.day and date.month unused
 */
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

	cal_fillftime(buffer, bufferlen - 1, CalendarFormatYear, date);
	puts(buffer);

	struct cal_date d = {
		.day = 1,
		.month = 0,
		.year = date->year
	};

	while(d.month != 12) {
		struct cal_moninfo minfos[3];

		for(int i = 0; i < 3; i += 1) {
			d.month += 1;
			cal_moninfo_init(minfos + i, &d);
			cal_fillftime(buffer + 22 * i, 20,
				CalendarFormatMonth, &d);
		}
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
	struct cal_date date = { 1 };

	if(argc > 3) {
		cal_usage(*argv);
	} else if(argc != 1) {
		char **argpos = argv + 1;

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

