/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Derived from system/core/init/watchdogd.c in the Android 4.2.1 source code.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/watchdog.h>

#define DEV_NAME		"/dev/watchdog"
#define KLOG_DEV_NAME		"/dev/kmsg"
#define KLOG_MSG_MAXLEN		256

static void disable_oom_kill(void);
static void klog_write(const char *fmt, ...);

int main(int argc, char **argv) {
	int fd;
	int ret;
	int interval = 10;
	int margin = 10;
	int timeout;

	umask(0077);

	/* Make stdin/stdout/stderr /dev/null */
	if ((fd = open("/dev/null", O_RDWR)) >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
	}

	/* Increase scheduling priority and disable OOM kill for this process */
	nice(-19);
	disable_oom_kill();

	klog_write("Starting watchdogd\n");

	if (argc >= 2)
		interval = atoi(argv[1]);

	if (argc >= 3)
		margin = atoi(argv[2]);

	timeout = interval + margin;

	fd = open(DEV_NAME, O_RDWR);
	if (fd < 0) {
		klog_write("watchdogd: Failed to open %s: %s\n", DEV_NAME, strerror(errno));
		return 1;
	}

	ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
	if (ret) {
		klog_write("watchdogd: Failed to set timeout to %d: %s\n", timeout, strerror(errno));
		ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
		if (ret) {
			klog_write("watchdogd: Failed to get timeout: %s\n", strerror(errno));
		} else {
			if (timeout > margin)
				interval = timeout - margin;
			else
				interval = 1;
			klog_write("watchdogd: Adjusted interval to timeout returned by driver: timeout %d, interval %d, margin %d\n",
				  timeout, interval, margin);
		}
	}

	while(1) {
		write(fd, "", 1);
		sleep(interval);
	}
}

static void disable_oom_kill(void) {
	int fd;
	if ((fd = open("/proc/self/oom_score_adj", O_WRONLY)) == -1) {
		fprintf(stderr, "Couldn't open /proc/self/oom_score_adj");
		return;
	}
	write(fd, "-1000", 5);
	close(fd);
}

static void klog_write(const char *fmt, ...) {
	static int fd = -1;
	static char buf[KLOG_MSG_MAXLEN];
	va_list ap;

	if (fd == -1) {
		if ((fd = open(KLOG_DEV_NAME, O_WRONLY)) == -1) {
			fprintf(stderr, "Couldn't open " KLOG_DEV_NAME);
			return;
		}
	}

	va_start(ap, fmt);
	vsnprintf(buf, KLOG_MSG_MAXLEN, fmt, ap);
	va_end(ap);
	write(fd, buf, strlen(buf));
}
