#include "RuntimeCrashCapture.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif

namespace heritage::diagnostics {
namespace {

std::filesystem::path g_diagnosticsDirectory;

std::filesystem::path requestedProjectRoot(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--project-root")
            return std::filesystem::path(argv[i + 1]).lexically_normal();
    }
    return std::filesystem::current_path();
}

void writeTopLevelException(const char* kind, const char* detail) noexcept
{
    try
    {
        if (g_diagnosticsDirectory.empty())
            return;
        std::filesystem::create_directories(g_diagnosticsDirectory);
        std::ofstream out(
            g_diagnosticsDirectory / "RuntimeCrashLatest.txt",
            std::ios::out | std::ios::trunc);
        if (!out)
            return;
        out << "Heritage Engine runtime failure\n";
        out << "kind: " << (kind ? kind : "unknown") << '\n';
        if (detail && *detail)
            out << "detail: " << detail << '\n';
        out.flush();
    }
    catch (...)
    {
        // Diagnostics must never replace the original failure.
    }
}

#ifdef _WIN32
LONG WINAPI writeUnhandledException(EXCEPTION_POINTERS* exceptionPointers)
{
    try
    {
        if (!g_diagnosticsDirectory.empty())
        {
            std::filesystem::create_directories(g_diagnosticsDirectory);
            std::ofstream out(
                g_diagnosticsDirectory / "RuntimeCrashLatest.txt",
                std::ios::out | std::ios::trunc);
            if (out)
            {
                out << "Heritage Engine unhandled Windows exception\n";
                if (exceptionPointers && exceptionPointers->ExceptionRecord)
                {
                    out << "exception_code: 0x" << std::hex
                        << static_cast<unsigned long>(
                            exceptionPointers->ExceptionRecord->ExceptionCode)
                        << std::dec << '\n';
                    out << "exception_address: "
                        << exceptionPointers->ExceptionRecord->ExceptionAddress << '\n';
                }
                out << "process_id: " << GetCurrentProcessId() << '\n';
                out << "thread_id: " << GetCurrentThreadId() << '\n';
                out.flush();
            }

            // Load DbgHelp dynamically so normal startup has no additional
            // import/link dependency. The text diagnostic above is authoritative
            // if dump creation is unavailable.
            HMODULE dbghelp = LoadLibraryW(L"Dbghelp.dll");
            if (dbghelp)
            {
                using MiniDumpWriteDumpFn = BOOL (WINAPI *)(
                    HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                    PMINIDUMP_EXCEPTION_INFORMATION,
                    PMINIDUMP_USER_STREAM_INFORMATION,
                    PMINIDUMP_CALLBACK_INFORMATION);
                const auto miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(
                    GetProcAddress(dbghelp, "MiniDumpWriteDump"));
                if (miniDumpWriteDump)
                {
                    const auto dumpPath = g_diagnosticsDirectory / "RuntimeCrashLatest.dmp";
                    HANDLE dumpFile = CreateFileW(
                        dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (dumpFile != INVALID_HANDLE_VALUE)
                    {
                        MINIDUMP_EXCEPTION_INFORMATION info{};
                        info.ThreadId = GetCurrentThreadId();
                        info.ExceptionPointers = exceptionPointers;
                        info.ClientPointers = FALSE;
                        miniDumpWriteDump(
                            GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
                            MiniDumpWithThreadInfo,
                            exceptionPointers ? &info : nullptr,
                            nullptr, nullptr);
                        FlushFileBuffers(dumpFile);
                        CloseHandle(dumpFile);
                    }
                }
                FreeLibrary(dbghelp);
            }
        }
    }
    catch (...)
    {
        // Never throw from the Windows unhandled-exception filter.
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void installRuntimeCrashCapture(int argc, char** argv) noexcept
{
    try
    {
        g_diagnosticsDirectory = requestedProjectRoot(argc, argv)
            / "UserData" / "Diagnostics";
        std::filesystem::create_directories(g_diagnosticsDirectory);
#ifdef _WIN32
        SetUnhandledExceptionFilter(writeUnhandledException);
#endif
    }
    catch (...)
    {
        // Crash capture is optional and must never block engine startup.
    }
}

int reportUncaughtException(const std::exception& error) noexcept
{
    writeTopLevelException("uncaught std::exception", error.what());
    try
    {
        std::cerr << "Heritage Engine uncaught exception: " << error.what() << '\n';
    }
    catch (...)
    {
    }
    return -2;
}

int reportUnknownException() noexcept
{
    writeTopLevelException("uncaught non-standard C++ exception", "");
    try
    {
        std::cerr << "Heritage Engine uncaught non-standard exception.\n";
    }
    catch (...)
    {
    }
    return -3;
}

} // namespace heritage::diagnostics
