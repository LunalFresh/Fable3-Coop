#include <winsock2.h>
#include <Windows.h>
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "./xlln-config.hpp"
#include "../utils/utils.hpp"
#include "./wnd-debug-log.hpp"
#include <vector>
#include <sys/timeb.h>

uint32_t xlln_debug_log_level = (XLLN_LOG_CONTEXT_MASK | XLLN_LOG_LEVEL_MASK) & (~(uint32_t)XLLN_LOG_LEVEL_TRACE);

CRITICAL_SECTION xlln_critsec_debug_log;

static HANDLE xlln_debug_log_file = 0;

// Number is high to begin with as this cache is also used as temporary storage before the debug log file is opened.
// Once the on screen interactive debug log window initialises and logs are displayed, this number is automatically reduced.
size_t debug_log_cache_max = 1000;
static bool debug_log_accept_cache = true;
static std::vector<wchar_t*> debug_log_cache;

static std::vector<char*> debug_log_trace_blacklist;

// Malloc'd result.
wchar_t* XllnGetDebugLogCacheMessages(size_t* number_of_messages)
{
	EnterCriticalSection(&xlln_critsec_debug_log);
	size_t debugLogMessagesJoinedSizeLength = 1;
	wchar_t* debugLogMessagesJoined = (wchar_t*)malloc(debugLogMessagesJoinedSizeLength * sizeof(wchar_t));
	bool isFirst = true;
	for (const auto &message : debug_log_cache) {
		if (!message || message[0] == 0) {
			continue;
		}
		if (!isFirst) {
			debugLogMessagesJoinedSizeLength+=2;
		}
		
		size_t messageLength = wcslen(message);
		debugLogMessagesJoined = (wchar_t*)realloc(debugLogMessagesJoined, (debugLogMessagesJoinedSizeLength + messageLength) * sizeof(wchar_t));
		if (isFirst) {
			isFirst = false;
		}
		else {
			debugLogMessagesJoined[debugLogMessagesJoinedSizeLength-3] = L'\r';
			debugLogMessagesJoined[debugLogMessagesJoinedSizeLength-2] = L'\n';
		}
		memcpy_s(
			&debugLogMessagesJoined[debugLogMessagesJoinedSizeLength-1]
			, messageLength * sizeof(wchar_t)
			, message
			, messageLength * sizeof(wchar_t)
		);
		debugLogMessagesJoinedSizeLength += messageLength;
	}
	debugLogMessagesJoined[debugLogMessagesJoinedSizeLength-1] = 0;
	if (number_of_messages) {
		*number_of_messages = debug_log_cache.size();
	}
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	return debugLogMessagesJoined;
}

static uint32_t XllnDebugLogWrite(uint32_t log_level, const wchar_t* message, size_t message_length)
{
	wchar_t* messagePrefix = 0;
	size_t messagePrefixLength = 0;
	{
		__timeb64 currentTime;
		_ftime64_s(&currentTime);
		wchar_t* prefixTime = FormMallocString(L"%I64d:%03hu %x", currentTime.time, currentTime.millitm, GetCurrentThreadId());
		
		const wchar_t* prefixParts[13];
		size_t prefixPartsSize[13];
		size_t iPrefixPart = 0;
		{
			prefixParts[iPrefixPart] = prefixTime;
			messagePrefixLength += wcslen(prefixTime);
			prefixPartsSize[iPrefixPart++] = (sizeof(wchar_t) * messagePrefixLength);
		}
#define AddPrefixPart(logLevelTrigger, prefixText) {\
		if (!logLevelTrigger || log_level & logLevelTrigger) {\
			static const wchar_t prefixPart[] = prefixText;\
			prefixParts[iPrefixPart] = prefixPart;\
			prefixPartsSize[iPrefixPart] = sizeof(prefixPart) - sizeof(prefixPart[0]);\
			messagePrefixLength += (prefixPartsSize[iPrefixPart++]/sizeof(prefixPart[0]));\
		}\
}
		AddPrefixPart(XLLN_LOG_CONTEXT_XLIVE, L" XLIVE");
		AddPrefixPart(XLLN_LOG_CONTEXT_XLIVELESSNESS, L" XLLN");
		AddPrefixPart(XLLN_LOG_CONTEXT_XLLN_MODULE, L" XMOD");
		AddPrefixPart(XLLN_LOG_CONTEXT_OTHER, L" OTHER");
		AddPrefixPart(XLLN_LOG_CONTEXT_TITLE, L" TITLE");
		AddPrefixPart(XLLN_LOG_LEVEL_FATAL, L" FATAL");
		AddPrefixPart(XLLN_LOG_LEVEL_ERROR, L" ERROR");
		AddPrefixPart(XLLN_LOG_LEVEL_WARN, L" WARN");
		AddPrefixPart(XLLN_LOG_LEVEL_INFO, L" INFO");
		AddPrefixPart(XLLN_LOG_LEVEL_DEBUG, L" DEBUG");
		AddPrefixPart(XLLN_LOG_LEVEL_TRACE, L" TRACE");
		AddPrefixPart(0, L"\t> ");
#undef AddPrefixPart
		
		messagePrefix = new wchar_t[messagePrefixLength + 1];
		wchar_t* messagePrefixPart = messagePrefix;
		for (size_t iPushPart = 0; iPushPart < iPrefixPart; iPushPart++) {
			memcpy_s(
				messagePrefixPart
				, prefixPartsSize[iPushPart]
				, prefixParts[iPushPart]
				, prefixPartsSize[iPushPart]
			);
			messagePrefixPart = (wchar_t*)&((uint8_t*)messagePrefixPart)[prefixPartsSize[iPushPart]];
		}
		messagePrefix[messagePrefixLength] = 0;
		
		free(prefixTime);
		prefixTime = 0;
	}
	
	if (message_length == (size_t)-1) {
		message_length = wcslen(message);
	}
	
	EnterCriticalSection(&xlln_critsec_debug_log);
	
	if (!debug_log_accept_cache && !xlln_debug_log_file) {
		LeaveCriticalSection(&xlln_critsec_debug_log);
		delete[] messagePrefix;
		messagePrefix = 0;
		messagePrefixLength = 0;
		return ERROR_NOT_READY;
	}
	
	size_t messagePreviousPart = 0;
#define LogMessage(iPos) {\
	size_t messagePartLength = iPos - messagePreviousPart;\
	size_t messagePartLength2 = iPos - messagePreviousPart;\
	if (messagePartLength) {messagePartLength += messagePrefixLength;}\
	wchar_t* messagePart = new wchar_t[messagePartLength + 1];\
	wchar_t* messagePart2 = messagePart;\
	if (messagePartLength) {\
		messagePart2 = &messagePart2[messagePrefixLength];\
		memcpy_s(messagePart, (sizeof(wchar_t) * messagePrefixLength), messagePrefix, (sizeof(wchar_t) * messagePrefixLength));\
	}\
	memcpy_s(messagePart2, (sizeof(wchar_t) * (messagePartLength2 + 1)), &message[messagePreviousPart], sizeof(wchar_t) * (messagePartLength2));\
	messagePart[messagePartLength] = 0;\
	if (xlln_debug_log_file) {\
		WriteFile(xlln_debug_log_file, messagePart, (uint32_t)(messagePartLength * sizeof(wchar_t)), 0, 0);\
		WriteFile(xlln_debug_log_file, L"\r\n", (uint32_t)(2 * sizeof(wchar_t)), 0, 0);\
	}\
	if (debug_log_accept_cache) {\
		debug_log_cache.push_back(messagePart);\
	}\
	else {\
		delete[] messagePart;\
		messagePart = 0;\
	}\
}
	for (size_t iChar = 0; iChar < message_length; iChar++) {
		if (message[iChar] == L'\r' || message[iChar] == L'\n') {
			LogMessage(iChar);
			// Correctly skip two part new line characters.
			if (iChar + 1 < message_length && message[iChar + 1] != message[iChar] && (message[iChar + 1] == L'\r' || message[iChar + 1] == L'\n')) {
				iChar++;
			}
			messagePreviousPart = iChar + 1;
		}
	}
	LogMessage(message_length);
#undef LogMessage
	
	delete[] messagePrefix;
	messagePrefix = 0;
	messagePrefixLength = 0;
	
	while (debug_log_accept_cache && debug_log_cache.size() > debug_log_cache_max) {
		auto itrMessage = debug_log_cache.begin();
		wchar_t* message = *itrMessage;
		debug_log_cache.erase(itrMessage);
		delete[] message;
		message = 0;
	}
	
	PostMessageW(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::EVENT_DEBUG_TBX_LOG_UPDATE, 0, 0);
	
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	if (!xlln_debug_log_file) {
		return ERROR_INVALID_HANDLE;
	}
	
	return ERROR_SUCCESS;
}

static uint32_t XllnDebugLogWrite(uint32_t log_level, const wchar_t* message)
{
	return XllnDebugLogWrite(log_level, message, (size_t)-1);
}

static uint32_t WINAPI XllnDebugLogF(uint32_t log_level, const wchar_t* const message_format, ...)
{
	if (!(log_level & xlln_debug_log_level & XLLN_LOG_CONTEXT_MASK) || !(log_level & xlln_debug_log_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}
	
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscwprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	size_t messageSizeLength = messageLength + 1;
	wchar_t* message = new wchar_t[messageSizeLength];
	messageLength = _vsnwprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t resultCode = XllnDebugLogWrite(log_level, message);
	
	delete[] message;
	message = 0;
	
	return resultCode;
}

// #41143
uint32_t WINAPI XLLNDebugLog(uint32_t log_level, const char* message)
{
	if (!(log_level & xlln_debug_log_level & XLLN_LOG_CONTEXT_MASK) || !(log_level & xlln_debug_log_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}
	
	return XllnDebugLogF(log_level, L"%hs", message);
}

// #41144
uint32_t WINAPI XLLNDebugLogF(uint32_t log_level, const char* const message_format, ...)
{
	if (!(log_level & xlln_debug_log_level & XLLN_LOG_CONTEXT_MASK) || !(log_level & xlln_debug_log_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}
	
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	size_t messageSizeLength = messageLength + 1;
	char* message = new char[messageSizeLength];
	messageLength = _vsnprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t resultCode = XLLNDebugLog(log_level, message);
	
	delete[] message;
	message = 0;
	
	return resultCode;
}

// #41147
uint32_t WINAPI XLLNGetDebugLogLevel(uint32_t* log_level)
{
	if (!log_level) {
		return ERROR_INVALID_PARAMETER;
	}
	
	*log_level = 0;
	
	if (!xlln_debug) {
		return ERROR_FUNCTION_FAILED;
	}
	
	*log_level = xlln_debug_log_level;
	
	return ERROR_SUCCESS;
}

// #41148
uint32_t WINAPI XllnDebugLogECodeF(uint32_t log_level, uint32_t error_code, const char* const message_format, ...)
{
	if (!(log_level & xlln_debug_log_level & XLLN_LOG_CONTEXT_MASK) || !(log_level & xlln_debug_log_level & XLLN_LOG_LEVEL_MASK)) {
		return ERROR_SUCCESS;
	}
	
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	size_t messageSizeLength = messageLength + 1;
	char* message = new char[messageSizeLength];
	messageLength = _vsnprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return ERROR_INVALID_PARAMETER;
	}
	
	wchar_t* logMessage = 0;
	wchar_t* msgBuf = 0;
	if (FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK
			, NULL
			, error_code
			, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
			, (LPTSTR)&msgBuf
			, 0
			, NULL
		)
	) {
		logMessage = FormMallocString(L"%hs (0x%08x, %s).", message, error_code, msgBuf);
		LocalFree(msgBuf);
		msgBuf = 0;
	}
	else {
		logMessage = FormMallocString(L"%hs (0x%08x).", message, error_code);
	}
	
	delete[] message;
	message = 0;
	
	uint32_t resultCode = XllnDebugLogWrite(log_level, logMessage);
	
	free(logMessage);
	logMessage = 0;
	
	return resultCode;
}

void XllnTraceFunc(const char* function_name)
{
	EnterCriticalSection(&xlln_critsec_debug_log);
	for (const auto &functionName : debug_log_trace_blacklist) {
		if (strcmp(function_name, functionName) == 0) {
			LeaveCriticalSection(&xlln_critsec_debug_log);
			return;
		}
	}
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	XllnDebugLogF(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_TRACE, L"%hs", function_name);
}

void XllnDebugBreak(char* message)
{
	XllnDebugBreak((const char*)message);
}
void XllnDebugBreak(const char* message)
{
	XllnDebugLogF(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL, L"%hs", message);
	__debugbreak();
}
void XllnDebugBreak(wchar_t* message)
{
	XllnDebugBreak((const wchar_t*)message);
}
void XllnDebugBreak(const wchar_t* message)
{
	XllnDebugLogF(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL, message);
	__debugbreak();
}

// Malloc'd result.
char* XllnGetDebugLogTraceBlacklist()
{
	EnterCriticalSection(&xlln_critsec_debug_log);
	size_t debugLogTraceBlacklistJoinedSize = 1;
	char* debugLogTraceBlacklistJoined = (char*)malloc(debugLogTraceBlacklistJoinedSize);
	bool isFirst = true;
	for (const auto &functionName : debug_log_trace_blacklist) {
		if (!functionName || functionName[0] == 0) {
			continue;
		}
		if (isFirst) {
			isFirst = false;
		}
		else {
			debugLogTraceBlacklistJoined[debugLogTraceBlacklistJoinedSize-1] = ',';
			debugLogTraceBlacklistJoinedSize++;
		}
		
		size_t functionNameLength = strlen(functionName);
		debugLogTraceBlacklistJoined = (char*)realloc(debugLogTraceBlacklistJoined, debugLogTraceBlacklistJoinedSize + functionNameLength);
		memcpy_s(
			&debugLogTraceBlacklistJoined[debugLogTraceBlacklistJoinedSize-1]
			, functionNameLength
			, functionName
			, functionNameLength
		);
		debugLogTraceBlacklistJoinedSize += functionNameLength;
	}
	debugLogTraceBlacklistJoined[debugLogTraceBlacklistJoinedSize-1] = 0;
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	return debugLogTraceBlacklistJoined;
}

void XllnAddDebugLogTraceBlacklist(const char* function_name)
{
	if (!function_name || function_name[0] == 0) {
		return;
	}
	
	EnterCriticalSection(&xlln_critsec_debug_log);
	
	char* functionName = CloneString(function_name);
	debug_log_trace_blacklist.push_back(functionName);
	
	LeaveCriticalSection(&xlln_critsec_debug_log);
}

bool XllnParseDebugLogTraceBlacklist(const char* blacklist_text)
{
	EnterCriticalSection(&xlln_critsec_debug_log);
	
	while (debug_log_trace_blacklist.size()) {
		auto itrFunctionName = debug_log_trace_blacklist.begin();
		char* functionName = *itrFunctionName;
		debug_log_trace_blacklist.erase(itrFunctionName);
		delete[] functionName;
		functionName = 0;
	}
	
	if (blacklist_text && blacklist_text[0] != 0) {
		const char* blacklistPrevious = blacklist_text;
		const char* blacklistCurrent = blacklist_text;
		while (1) {
			bool isEnd = (*blacklistCurrent == 0);
			if (isEnd || *blacklistCurrent == ',') {
				size_t functionNameLength = blacklistCurrent - blacklistPrevious;
				if (functionNameLength > 0) {
					char* functionName = new char[functionNameLength + 1];
					memcpy_s(
						functionName
						, functionNameLength
						, blacklistPrevious
						, functionNameLength
					);
					functionName[functionNameLength] = 0;
					debug_log_trace_blacklist.push_back(functionName);
					functionName = 0;
				}
				
				if (isEnd) {
					break;
				}
				blacklistPrevious = &blacklistCurrent[1];
			}
			blacklistCurrent = &blacklistCurrent[1];
		}
	}
	
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	return true;
}

bool InitDebugLog()
{
	debug_log_accept_cache = true;
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the log directory cannot be determined.");
		return false;
	}
	
	if (xlln_debug_log_file) {
		return false;
	}
	
#ifdef XLLN_DEBUG
	{
		EnterCriticalSection(&xlln_critsec_debug_log);
		
		wchar_t* debugLogPath = 0;
		{
			wchar_t* configPath = PathFromFilename(xlln_file_config_path);
			if (configPath) {
				debugLogPath = FormMallocString(L"%slogs/", configPath);
				delete[] configPath;
				configPath = 0;
			}
		}
		uint32_t errorMkdir = EnsureDirectoryExists(debugLogPath);
		if (errorMkdir || !debugLogPath) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, debugLogPath ? debugLogPath : L"");
			if (debugLogPath) {
				free(debugLogPath);
				debugLogPath = 0;
			}
			LeaveCriticalSection(&xlln_critsec_debug_log);
			return false;
		}
		
		wchar_t* debugLogFilePath = FormMallocString(L"%sxlln-debug-%u.log", debugLogPath, xlln_local_instance_id);
		free(debugLogPath);
		debugLogPath = 0;
		
		{
			HANDLE hFile = CreateFileW(debugLogFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (!hFile || hFile == INVALID_HANDLE_VALUE) {
				uint32_t errorCode = GetLastError();
				XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s Failed to open debug log file: \"%ls\"."
					, __func__
					, debugLogFilePath
				);
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
					, "%s Opened debug log file successfully at: \"%ls\"."
					, __func__
					, debugLogFilePath
				);
				xlln_debug_log_file = hFile;
			}
		}
		free(debugLogFilePath);
		debugLogFilePath = 0;
		
		if (!xlln_debug_log_file) {
			LeaveCriticalSection(&xlln_critsec_debug_log);
			return false;
		}
		
		// Write any logs already stored in the cache to the file.
		for (const auto& message : debug_log_cache) {
			size_t messageLength = wcslen(message);
			WriteFile(xlln_debug_log_file, message, (uint32_t)(messageLength * sizeof(wchar_t)), 0, 0);\
			WriteFile(xlln_debug_log_file, L"\r\n", 2 * sizeof(wchar_t), 0, 0);
		}
		
		LeaveCriticalSection(&xlln_critsec_debug_log);
	}
#endif
	
	return true;
}

bool UninitDebugLog()
{
	if (xlln_debug_log_file) {
		CloseHandle(xlln_debug_log_file);
		xlln_debug_log_file = 0;
	}
	
	EnterCriticalSection(&xlln_critsec_debug_log);
	
	debug_log_accept_cache = false;
	
	while (debug_log_cache.size()) {
		auto itrMessage = debug_log_cache.begin();
		wchar_t* message = *itrMessage;
		debug_log_cache.erase(itrMessage);
		delete[] message;
		message = 0;
	}
	
	while (debug_log_trace_blacklist.size()) {
		auto itrFunctionName = debug_log_trace_blacklist.begin();
		char* functionName = *itrFunctionName;
		debug_log_trace_blacklist.erase(itrFunctionName);
		delete[] functionName;
		functionName = 0;
	}
	
	LeaveCriticalSection(&xlln_critsec_debug_log);
	
	return true;
}
