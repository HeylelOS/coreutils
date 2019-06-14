#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloca.h>

typedef unsigned long cal_t;

enum cal_format {
	CALENDAR_FORMAT_YEAR,
	CALENDAR_FORMAT_MONTH,
	CALENDAR_FORMAT_MONTH_YEAR
};

struct cal_date {
	cal_t month; /**< Month of the year, between 1 and 12 */
	cal_t year; /**< Year, between 1 and 9999 */
};

struct cal_moninfo {
	cal_t fwday; /**< Index in a week of the first day of the month range, Sunday = 0 */
	cal_t lday; /**< Index of the last day of the month (first is 1) */
};

/**
 * Determine if date is a leap year, depending
 * on the date of adoption of Julian/Gregorian calendars
 * @param date Valid date pointer fully initialized
 * @return 1 if it's a leap year, 0 else
 */
static inline cal_t
cal_leap_year(const struct cal_date *date) {
	cal_t leap = 0;

	/* Test whether Gregorian or Julian calendar */
	if(date->year > 1752
		|| (date->year == 1752
			&& date->month > 9)) {
		if((date->year % 4 == 0
			&& date->year % 100 != 0)
			|| date->year % 400 == 0) {
			leap += 1;
		}
	} else if(date->year % 4 == 0) {
		leap += 1;
	}

	return leap;
}

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
	long m = date->month - 2;
	long Y = date->year;
	if(m <= 0) {
		m += 12;
		Y -= 1;
	}
	const long d = 1,
		c = Y / 100,
		y = Y % 100;
	const long w = (d + (13 * m - 1) / 5 + y + y / 4 + c / 4 - 2 * c) % 7;

	minfo->fwday = w < 0 ? w + 7 : w;

	/* Determine the last day of month */
	static const cal_t gregorian[] = {
		0 /* UNUSED */, 31, 28 /* UNUSED */, 31,
		30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if(date->month != 2) {
		minfo->lday = gregorian[date->month];
	} else {
		/* Handle leap year */
		minfo->lday = 28 + cal_leap_year(date);
	}
}

/**
 * Write the needed format centered into spaces
 * into the buffer buffer, surrounded with spaces
 * @param buffer Valid pointer into a buffer we write to
 * @param bufferlen Size of buffer
 * @param f Kind of format
 * @param date Pointer to the date to format
 */
static void
cal_fillftime(char *buffer,
	size_t bufferlen,
	enum cal_format f,
	const struct cal_date *date) {
#ifdef CAL_LOCALIZED
	const struct tm timeinfo = {
		.tm_mday = 1,
		.tm_mon = date->month - 1,
		.tm_year = date->year - 1900,
	};
	const char *format;

	if(f == CALENDAR_FORMAT_YEAR) {
		format = "%Y";
	} else if(f == CALENDAR_FORMAT_MONTH) {
		format = "%B";
	} else { /* CALENDAR_FORMAT_MONTH_YEAR */
		format = "%B %Y";
	}

	size_t length = strftime(buffer, bufferlen, format, &timeinfo);
#else
	static const char * const months[] = {
		"January", "Februrary", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};
	size_t length;

	if(f == CALENDAR_FORMAT_YEAR) {
		length = snprintf(buffer, bufferlen, "%lu", date->year);
	} else if(f == CALENDAR_FORMAT_MONTH) {
		length = snprintf(buffer, bufferlen, "%s", months[date->month - 1]);
	} else { /* CALENDAR_FORMAT_MONTH_YEAR */
		length = snprintf(buffer, bufferlen, "%s %lu", months[date->month - 1], date->year);
	}

	if(length >= bufferlen) {
		length = 0;
	}
#endif
	const size_t offset = (bufferlen - length) / 2;

	memmove(buffer + offset, buffer, length);
	memset(buffer, ' ', offset);
	memset(buffer + offset + length, ' ', bufferlen - length - offset);
}

/**
 * Fills buffer with a description of the week in 20 chars
 * @param buffer A buffer allocated >= 20 which will be filled
 * @return A pointer to the character following the last written
 */
static char *
cal_fillweek(char *buffer) {
#ifdef CAL_LOCALIZED
	struct tm t = { .tm_wday = 0 };

	for(; t.tm_wday < 6; t.tm_wday += 1, buffer += 3) {
		strftime(buffer, 2, "%a", &t);
		buffer[2] = ' ';
	}
	strftime(buffer, 2, "%a", &t);
	buffer += 2;

	return buffer;
#else
	return stpncpy(buffer, "Su Mo Tu We Th Fr Sa", 20);
#endif
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

	if(day >= 10) {
		buffer[0] = day / 10 + '0';
		buffer[1] = day % 10 + '0';
	} else {
		buffer[0] = ' ';
		buffer[1] = day + '0';
	}
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

	/* At the beginning, we write spaces
	up to needed + spaces between each current
	and following day writes */
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

	/* Then we write each day until its the end
	of the buffer or the end of the month */
	for(; step <= minfo->lday && buffer < end;
		step += 1, buffer += 3) {

		cal_sday(buffer, step);
	}

	/* If the month ended, we fill the buffer with spaces */
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

	/* Print name of month + year */
	cal_fillftime(buffer, bufferlen - 1,
		CALENDAR_FORMAT_MONTH_YEAR, date);
	puts(buffer);

	/* Print week description */
	cal_fillweek(buffer);
	puts(buffer);

	/* Print the month */
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

	/* We fill a 65 bytes long buffer for weeks once */
	char *w = cal_fillweek(weeks);
	w[0] = ' '; w[1] = ' ';
	memcpy(w + 2, weeks, 20);
	w[22] = ' '; w[23] = ' ';
	memcpy(w + 24, weeks, 20);

	/* Print the year at the top */
	cal_fillftime(buffer, bufferlen - 1, CALENDAR_FORMAT_YEAR, date);
	puts(buffer);

	/* Iterate through the year */
	struct cal_date d = {
		.month = 0,
		.year = date->year
	};

	while(d.month != 12) {
		struct cal_moninfo minfos[3];

		/* Print months' names */
		for(int i = 0; i < 3; i += 1) {
			d.month += 1;
			cal_moninfo_init(minfos + i, &d);
			cal_fillftime(buffer + 22 * i, 20,
				CALENDAR_FORMAT_MONTH, &d);
		}
		puts(buffer);

		/* Print weeks description */
		puts(weeks);

		/* Print each month */
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
	struct cal_date date;

	/* Argument parsing */
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

		date.month = timeinfo->tm_mon + 1;
		date.year = timeinfo->tm_year + 1900;
	}

	/* Whether we print a full year calendar, or only the month */
	if(argc != 2) {
		cal_print_month(&date);
	} else {
		cal_print_year(&date);
	}

	return 0;
}

