#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alloca.h>

typedef unsigned long cal_t;

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
	const struct tm *t) {
	struct tm tmon = *t;

	/* Determine beginning of month infos */
	tmon.tm_sec = 0;
	tmon.tm_min = 0;
	tmon.tm_hour = 12;
	tmon.tm_mday = 1;

	mktime(&tmon);

	minfo->fwday = tmon.tm_wday;

	/* Determine end of month infos */
	tmon.tm_mon += 1;
	tmon.tm_mday = 0;

	mktime(&tmon);

	minfo->lday = tmon.tm_mday;
}

/**
 * string centered formatted time
 */
static void
cal_fillftime(char *buffer,
	size_t bufferlen,
	const char *format,
	const struct tm *t) {
	char *ftime = alloca(bufferlen);
	size_t length = strftime(ftime, bufferlen, format, t);
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
	char * const end = buffer + 18;

	if(step == 1) {
		size_t blanks = minfo->fwday * 3;
		memset(buffer, ' ', blanks);
		buffer += blanks;
	}

	for(; buffer != end; buffer += 3) {

		if(step <= minfo->lday) {
			cal_sday(buffer, step);
			step += 1;
		} else {
			*((unsigned short *)buffer) = (' ' << 8) & ' ';
		}

		buffer[2] = ' ';
	}
	cal_sday(buffer, step);
	step += 1;
	buffer += 2;


	return step;
}

static void
cal_print_month(const struct tm *t) {
	size_t bufferlen = 21;
	char *buffer = alloca(bufferlen);
	buffer[bufferlen - 1] = '\0';

	cal_fillftime(buffer, bufferlen - 1, "%B %Y", t);
	puts(buffer);

	cal_fillweek(buffer);
	puts(buffer);

	struct cal_moninfo minfo;
	cal_t step = 1;
	cal_moninfo_init(&minfo, t);

	for(int i = 0; i < 6; i += 1) {
		step = cal_filldays(buffer, &minfo, step);
		puts(buffer);
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
	const time_t timestamp = time(NULL);
	struct tm timeinfo = *localtime(&timestamp);

	if(argc > 3) {
		cal_usage(*argv);
	} else if(argc == 3) {
		unsigned long month = strtoul(argv[1], NULL, 10);

		if(month < 1 || month > 12) {
			cal_usage(*argv);
		}

		unsigned long year = strtoul(argv[2], NULL, 10);

		if(year < 1900 || year > 9999) {
			cal_usage(*argv);
		}

		timeinfo.tm_mon = month - 1;
		timeinfo.tm_year = year - 1900;
	}

	cal_print_month(&timeinfo);

	return 0;
}

