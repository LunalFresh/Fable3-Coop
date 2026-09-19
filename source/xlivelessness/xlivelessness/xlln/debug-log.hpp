#pragma once
#include <stdint.h>

#define XLLN_LOG_LEVEL_MASK		(0b00111111)
// Function call tracing.
#define XLLN_LOG_LEVEL_TRACE	(0b00000001)
// Function, variable and operation logging.
#define XLLN_LOG_LEVEL_DEBUG	(0b00000010)
// Generally useful information to log (service start/stop, configuration assumptions, etc).
#define XLLN_LOG_LEVEL_INFO		(0b00000100)
// Anything that can potentially cause application oddities, but is being handled adequately.
#define XLLN_LOG_LEVEL_WARN		(0b00001000)
// Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).
#define XLLN_LOG_LEVEL_ERROR	(0b00010000)
// Errors that will terminate the application.
#define XLLN_LOG_LEVEL_FATAL	(0b00100000)

#define XLLN_LOG_CONTEXT_MASK			(0b11000111 << 8)
// Logs related to Xlive(GFWL) functionality.
#define XLLN_LOG_CONTEXT_XLIVE			(0b00000001 << 8)
// Logs related to XLiveLessNess functionality.
#define XLLN_LOG_CONTEXT_XLIVELESSNESS	(0b00000010 << 8)
// Logs related to XLLN-Module functionality.
#define XLLN_LOG_CONTEXT_XLLN_MODULE	(0b00000100 << 8)
// Logs related to functionality from other areas of the application.
#define XLLN_LOG_CONTEXT_OTHER			(0b10000000 << 8)
// Logs related to the Title functionality.
#define XLLN_LOG_CONTEXT_TITLE			(0b01000000 << 8)

// #41143
uint32_t WINAPI XLLNDebugLog(uint32_t log_level, const char* message);
// #41144
uint32_t WINAPI XLLNDebugLogF(uint32_t log_level, const char* const message_format, ...);
// #41147
uint32_t WINAPI XLLNGetDebugLogLevel(uint32_t* log_level);
// #41148
uint32_t WINAPI XllnDebugLogECodeF(uint32_t log_level, uint32_t error_code, const char* const message_format, ...);

void XllnTraceFunc(const char* function_name);

#ifdef _DEBUG
#define XLLN_DEBUG
#endif

#ifdef XLLN_DEBUG
#define XLLN_DEBUG_LOG(log_level, message_format, ...) XLLNDebugLogF(log_level, message_format, __VA_ARGS__)
#define XLLN_DEBUG_LOG_ECODE(error_code, log_level, message_format, ...) XllnDebugLogECodeF(log_level, error_code, message_format, __VA_ARGS__)
#define GET_SOCKADDR_INFO(sockAddrStorage) GetSockAddrInfo(sockAddrStorage)
#define TRACE_FX() XllnTraceFunc(__func__)
#else
#define XLLN_DEBUG_LOG(log_level, message_format, ...)
#define XLLN_DEBUG_LOG_ECODE(error_code, log_level, message_format, ...)
#define GET_SOCKADDR_INFO(sockAddrStorage) (0)
#define TRACE_FX()
#endif

extern uint32_t xlln_debug_log_level;
extern CRITICAL_SECTION xlln_critsec_debug_log;
extern size_t debug_log_cache_max;

bool InitDebugLog();
bool UninitDebugLog();

void XllnDebugBreak(char* message);
void XllnDebugBreak(const char* message);
void XllnDebugBreak(wchar_t* message);
void XllnDebugBreak(const wchar_t* message);
// Malloc'd result.
wchar_t* XllnGetDebugLogCacheMessages(size_t* number_of_messages);
// Malloc'd result.
char* XllnGetDebugLogTraceBlacklist();
bool XllnParseDebugLogTraceBlacklist(const char* blacklist_text);
void XllnAddDebugLogTraceBlacklist(const char* function_name);
