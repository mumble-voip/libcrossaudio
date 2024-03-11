// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_TESTS_KEY_H
#define CROSSAUDIO_TESTS_KEY_H

#if defined(OS_WINDOWS)
#	include <conio.h>
#else
#	include <stdio.h>
#	include <termios.h>
#	include <unistd.h>
#endif

enum Key { KEY_NONE, KEY_BREAK, KEY_PAUSE };

#if !defined(OS_WINDOWS)
static int getch(void) {
	struct termios oldAttr;
	tcgetattr(STDIN_FILENO, &oldAttr);
	struct termios newAttr = oldAttr;
	newAttr.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newAttr);
	const int ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldAttr);

	return ch;
}
#endif

static inline enum Key getKey(void) {
	switch (getch()) {
		case '\n':
		case '\r':
			return KEY_BREAK;
		case 'p':
		case 'P':
			return KEY_PAUSE;
	}

	return KEY_NONE;
}

#endif
