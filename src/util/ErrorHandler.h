#pragma once

#ifdef ENVY_CRASH_REPORTING
#include "util/ExceptionHandler.h"

#define ENVY_ERROR_HANDLER_CONCAT_INNER(a, b) a##b
#define ENVY_ERROR_HANDLER_CONCAT(a, b) ENVY_ERROR_HANDLER_CONCAT_INNER(a, b)

#define BEGIN_ERROR_HANDLER                                                                                    \
    DebugExceptionHandler::ErrorBoundaryScope ENVY_ERROR_HANDLER_CONCAT(envyErrorBoundaryScope, __LINE__); \
    try {
#define END_ERROR_HANDLER                                                                \
    }                                                                                    \
    catch (const std::exception& e) {                                                    \
        LogExceptionDetails(e);                                                          \
        DebugExceptionHandler::AbortProcess();                                           \
    }                                                                                    \
    catch (...) {                                                                        \
        LogUnknownExceptionDetails("Caught unknown exception at Envy error boundary"); \
        DebugExceptionHandler::AbortProcess();                                           \
    }

#else
#define BEGIN_ERROR_HANDLER
#define END_ERROR_HANDLER
#endif
