/* @@@LICENSE
*
*      Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
LICENSE@@@ */

#include "liblog.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <alloca.h>
#include <sys_malloc.h>
#include <compiler/detection.h>
#include <compiler/pure_attribute.h>
#include <pjson_syslog.h>
#include <libgen.h>
#include <strnlen.h>
#include <isatty.h>

static const char *program_name = NULL;
static bool default_program_name = true;

#if LOG_INFO < LOG_ERR
	// increasing numbers indicate higher priority
	#define IS_HIGHER_PRIORITY(actual, base) ((actual) > (base))
#else
	// decreasing numbers indicate higher priority
	#define IS_HIGHER_PRIORITY(actual, base) ((actual) < (base))
#endif

#if GCC_VERSION >= __PJ_APP_VERSION(2, 96, 0)
#define PURE_FUNCTION __attribute__((pure))
#else
#define PURE_FUNCTION
#endif /* PURE_FUNCTION */

#define SAFE_STRING_PRINT(str) ((str) != NULL ? (str) : "(null)")

void setConsumerName(const char *name)
{
	PJ_LOG_INFO("changing program name to %s", SAFE_STRING_PRINT(program_name));
	if (default_program_name)
		free((char *)program_name);
	program_name = name;
	default_program_name = name != NULL;
}

const char *getConsumerName()
{
	if (default_program_name)
		return NULL;
	return program_name;
}

#ifdef HAVE_LOG_TARGET
static size_t setProgNameUnknown(char *buffer, size_t bufferSize) PURE_FUNC;
static size_t setProgNameUnknown(char *buffer, size_t bufferSize)
{
#define DEFAULT_UNKNOWN_CMDLINE "unknown process name"
	snprintf(buffer, bufferSize, "%s", DEFAULT_UNKNOWN_CMDLINE);
	return sizeof(DEFAULT_UNKNOWN_CMDLINE);
#undef DEFAULT_UNKNOWN_CMDLINE
}

static const char *getConsumerName_internal()
{
	pid_t proc_pid;
	char path[80];
	char cmdline[1024];
	char *program = cmdline;
	size_t cmdline_size;
	size_t prog_name_size;
	FILE *cmdline_file;
	char *dyn_program_name;

	if (program_name)
		return program_name;

	assert (default_program_name);

	proc_pid = getpid();
	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)proc_pid);
	cmdline_file = fopen(path, "r");
	if (cmdline_file == NULL) {
		cmdline_size = setProgNameUnknown(cmdline, sizeof(cmdline));
	} else {
		cmdline_size = fread(cmdline, sizeof(cmdline[0]), sizeof(cmdline) - 1, cmdline_file);
		if (cmdline_size) {
			cmdline_size--;
			cmdline[cmdline_size] = 0;
			program = basename(cmdline);
			cmdline_size = strnlen(cmdline, cmdline_size);
		}
		else
			cmdline_size = setProgNameUnknown(cmdline, sizeof(cmdline));
		fclose(cmdline_file);
	}

	prog_name_size = cmdline_size + 10;	// 10 characters for pid & null character just in case
	dyn_program_name = (char *)malloc(prog_name_size);
	if (dyn_program_name) {
		snprintf((char *)dyn_program_name, prog_name_size, "%d (%s)", (int) proc_pid, program);
	}

	return (program_name = dyn_program_name);
}

#if HAVE_VFPRINTF
#define VFPRINTF(priority, file, format, ap)                        \
	do {                                                            \
		vfprintf(file, format, ap);                                 \
		if (IS_HIGHER_PRIORITY(priority, LOG_INFO))                 \
			fflush(file);                                           \
	} while (0)
#else
#define VFPRINTF(priority, file, format, ap) PJSON_NOOP
#endif

static void log_v(int priority, const char *fullPath, int line, const char *message, va_list ap)
{
	static int using_terminal = -1;
	if (using_terminal == -1) {
#if defined HAVE_VSYSLOG
#if defined HAVE_VFPRINTF && defined HAVE_ISATTY
		using_terminal = isatty(fileno(stderr));
#else
		using_terminal = 0;
#endif
#elif defined HAVE_VFPRINTF
		using_terminal = 1;
#else
		using_terminal = 0;
#endif
	}

#define LOG_PREAMBLE "%s PJSON %s:%d :: "

	char *pathCopy = strdup(fullPath);
	char *path = strstr(pathCopy, "src/pjson_c");
	if (!path)
		path = pathCopy;
	// TODO: memoize the program name string length
	size_t messageLen = strlen(message) + strlen(path) + 4 /* line number */ + 100 /* chars for message */;
	const char *programNameToPrint = getConsumerName_internal();
	size_t formatLen = messageLen + sizeof(LOG_PREAMBLE) + (using_terminal ? 1 : 0) + strlen(programNameToPrint);
	char *format = alloca(formatLen);
	snprintf(format, formatLen, LOG_PREAMBLE "%s%s", programNameToPrint, path, line, message, using_terminal ? "\n" : "");

#if HAVE_VSYSLOG
	if (LIKELY(!using_terminal)) {
		vsyslog(priority, format, ap);
	} else {
		VFPRINTF(priority, stderr, format, ap);
	}
#elif HAVE_VFPRINTF
	VFPRINTF(priority, stderr, format, ap);
#else
#error Logging mechanism not implemented
#endif

	free(pathCopy);
}

void log_info(const char *path, int line, const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	log_v(LOG_DEBUG, path, line, message, ap);
	va_end(ap);
}

void log_warn(const char *path, int line, const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	log_v(LOG_WARNING, path, line, message, ap);
	va_end(ap);
}

void log_fatal(const char *path, int line, const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	log_v(LOG_CRIT, path, line, message, ap);
	va_end(ap);
}

#ifndef NDEBUG
void __assert_fail_msg (__const char *__assertion, __const char *__file,
			   unsigned int __line, __const char *__function, const char *format, ...)
{
	size_t bufferLen = (strlen(__assertion) + strlen(format)) * 2;
	if (bufferLen < 4096)
		bufferLen = 4096;

	char *buffer = (char *)malloc(bufferLen);
	int printed = snprintf(buffer, bufferLen, "%s: ", __assertion);

	va_list ap;
	va_start(ap, format);

	vsnprintf(buffer + printed, bufferLen - printed, format, ap);

	va_end(ap);

	__assert_fail(buffer, __file, __line, __function);

	free(buffer);
}
#endif

#endif /* HAVE_LOG_TARGET */
