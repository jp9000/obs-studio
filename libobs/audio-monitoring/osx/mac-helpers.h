#pragma once

static bool success_(OSStatus stat, const char *func, const char *call)
{
	if (stat != noErr) {
		blog(LOG_WARNING, "%s: %s failed: %d", func, call, (int)stat);
		return false;
	}

	return true;
}

#define success(stat, call) success_(stat, __FUNCTION__, call)
