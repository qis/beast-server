#pragma once

#ifndef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

#define LOGT SPDLOG_TRACE
#define LOGD SPDLOG_DEBUG
#define LOGI SPDLOG_INFO
#define LOGW SPDLOG_WARN
#define LOGE SPDLOG_ERROR
#define LOGC SPDLOG_CRITICAL

#ifndef NDEBUG
#define LOG_PATTERN "[%T.%e] [%^%L%$] [%@] %v"
#else
#define LOG_PATTERN "[%Y-%m-%d %T.%e] [%^%L%$] %v"
#endif
