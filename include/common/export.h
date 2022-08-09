#pragma once

#if !defined(UDB_EXPORT)

#if defined(UDB_SHARED_LIBRARY)
#if defined(_WIN32)

#if defined(UDB_COMPILE_LIBRARY)
#define UDB_EXPORT __declspec(dllexport)
#else
#define UDB_EXPORT __declspec(dllimport)
#endif // defined(UDB_COMPILE_LIBRARY)

#else // defined(_WIN32)
#if defined(UDB_COMPILE_LIBRARY)
#define UDB_EXPORT __attribute__((visibility("default")))
#else
#define UDB_EXPORT
#endif
#endif // defined(_WIN32)

#else // defined(UDB_SHARED_LIBRARY)
#define UDB_EXPORT
#endif

#endif // !defined(UDB_EXPORT)