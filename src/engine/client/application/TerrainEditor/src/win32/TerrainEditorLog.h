#ifndef INCLUDED_TerrainEditorLog_H
#define INCLUDED_TerrainEditorLog_H

#include <cstdio>
#include <cstdarg>
#include <windows.h>

namespace TerrainEditorLog
{
	inline FILE*& getFileHandle()
	{
		static FILE* s_logFile = NULL;
		return s_logFile;
	}

	inline void write(const char* fmt, ...)
	{
		char buf[2048];
		va_list args;
		va_start(args, fmt);
		_vsnprintf(buf, sizeof(buf) - 1, fmt, args);
		buf[sizeof(buf) - 1] = '\0';
		va_end(args);

		char out[2100];
		_snprintf(out, sizeof(out) - 1, "[TerrainEditor] %s\n", buf);
		out[sizeof(out) - 1] = '\0';

		OutputDebugStringA(out);

		FILE*& f = getFileHandle();
		if (!f)
			f = fopen("TerrainEditor_debug.log", "a");
		if (f)
		{
			fprintf(f, "%s", out);
			fflush(f);
		}
	}
}

#endif
