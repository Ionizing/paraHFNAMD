#pragma once
// debug.hpp
//
// Features:
//   - DEBUG(...)      : prints file:line:function and names+values, continues
//   - DEBUG_STOP(...) : prints file:line:function and names+values, aborts
//   - SIGINT (Ctrl+C) backtrace (best-effort) when enabled
//
// Switches:
//   - DEBUG_ENABLE (default: ON in !NDEBUG, OFF in NDEBUG)
//       compile with -DDEBUG_ENABLE=0/1
//   - DEBUG_ENABLE_SIGINT_BACKTRACE (default: follows DEBUG_ENABLE)
//       compile with -DDEBUG_ENABLE_SIGINT_BACKTRACE=0/1
//
// Notes:
//   - The SIGINT handler prints a backtrace using execinfo on Unix/macOS.
//   - Calling backtrace() from a signal handler is best-effort (not strictly async-signal-safe).
//   - For better symbols on Linux: compile with -g -O0 -rdynamic

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string_view>
#include <vector>
#include <sstream>
#include <utility>
#include <csignal>

#if defined(__unix__) || defined(__APPLE__)
  #include <unistd.h>    // write, STDERR_FILENO
  #include <execinfo.h>  // backtrace, backtrace_symbols_fd
#endif

// =====================
// Parent switch
// =====================
// Override at compile time:
//   -DDEBUG_ENABLE=1   (force on)
//   -DDEBUG_ENABLE=0   (force off)
#ifndef DEBUG_ENABLE
  #ifndef NDEBUG
    #define DEBUG_ENABLE 1   // default ON in debug builds
  #else
    #define DEBUG_ENABLE 0   // default OFF in release builds
  #endif
#endif

// SIGINT(Ctrl+C) backtrace switch (defaults to DEBUG_ENABLE)
#ifndef DEBUG_ENABLE_SIGINT_BACKTRACE
  #define DEBUG_ENABLE_SIGINT_BACKTRACE DEBUG_ENABLE
#endif

namespace debug_detail {

// Split `#__VA_ARGS__` into top-level comma-separated tokens.
// Handles nested (), [], {}, and simple string/char literals well enough for debugging.
// (Not a full C++ parser, but robust for typical DEBUG(a, b[i], foo(x,y)).)
inline std::vector<std::string_view> split_arg_names(std::string_view s) {
    auto trim = [](std::string_view v) {
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.remove_prefix(1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))  v.remove_suffix(1);
        return v;
    };

    std::vector<std::string_view> out;
    int depth_paren = 0, depth_brack = 0, depth_brace = 0;
    bool in_squote = false, in_dquote = false;
    bool escape = false;

    std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (escape) { escape = false; continue; }
        if ((in_squote || in_dquote) && c == '\\') { escape = true; continue; }

        if (!in_dquote && c == '\'') { in_squote = !in_squote; continue; }
        if (!in_squote && c == '"')  { in_dquote = !in_dquote; continue; }
        if (in_squote || in_dquote) continue;

        switch (c) {
            case '(': ++depth_paren; break;
            case ')': --depth_paren; break;
            case '[': ++depth_brack; break;
            case ']': --depth_brack; break;
            case '{': ++depth_brace; break;
            case '}': --depth_brace; break;
            case ',':
                if (depth_paren == 0 && depth_brack == 0 && depth_brace == 0) {
                    out.push_back(trim(s.substr(start, i - start)));
                    start = i + 1;
                }
                break;
            default: break;
        }
    }
    out.push_back(trim(s.substr(start)));
    if (out.size() == 1 && out[0].empty()) out.clear();
    return out;
}

inline void print_header(std::ostringstream& oss, const char* tag,
                         const char* file, int line, const char* func) {
    oss << tag << " at " << file << ":" << line << " (" << func << ")\n";
}

template <typename T>
inline void print_one(std::ostringstream& oss, std::string_view name, T&& value) {
    if (!name.empty())
        oss << "  " << name << " = " << std::forward<T>(value) << "\n";
    else
        oss << "  " << "<arg>" << " = " << std::forward<T>(value) << "\n";
}

template <typename... Args, std::size_t... I>
inline void print_all_impl(std::ostringstream& oss,
                           const std::vector<std::string_view>& names,
                           std::index_sequence<I...>,
                           Args&&... args) {
    (print_one(
         oss,
         (I < names.size() ? names[I] : std::string_view{}),
         std::forward<Args>(args)
     ), ...);
}

template <typename... Args>
inline void print_all(std::ostringstream& oss, std::string_view names_str, Args&&... args) {
    auto names = split_arg_names(names_str);
    print_all_impl(oss, names, std::index_sequence_for<Args...>{}, std::forward<Args>(args)...);
}

// ---------------------
// DEBUG_STOP (aborts)
// ---------------------

[[noreturn]] inline void stop(const char* file, int line, const char* func) {
    std::ostringstream oss;
    print_header(oss, "DEBUG_STOP", file, line, func);
    std::fputs(oss.str().c_str(), stderr);
    std::fflush(stderr);
    std::abort();
}

template <typename... Args>
[[noreturn]] inline void stop(const char* file, int line, const char* func,
                              const char* names_cstr, Args&&... args) {
    std::ostringstream oss;
    print_header(oss, "DEBUG_STOP", file, line, func);
    print_all(oss,
              names_cstr ? std::string_view{names_cstr} : std::string_view{},
              std::forward<Args>(args)...);
    std::fputs(oss.str().c_str(), stderr);
    std::fflush(stderr);
    std::abort();
}

// ---------------------
// DEBUG (prints only)
// ---------------------

inline void debug_print(const char* file, int line, const char* func) {
    std::ostringstream oss;
    print_header(oss, "DEBUG", file, line, func);
    std::fputs(oss.str().c_str(), stderr);
    std::fflush(stderr);
}

template <typename... Args>
inline void debug_print(const char* file, int line, const char* func,
                        const char* names_cstr, Args&&... args) {
    std::ostringstream oss;
    print_header(oss, "DEBUG", file, line, func);
    print_all(oss,
              names_cstr ? std::string_view{names_cstr} : std::string_view{},
              std::forward<Args>(args)...);
    std::fputs(oss.str().c_str(), stderr);
    std::fflush(stderr);
}

// ---------------------
// SIGINT (Ctrl+C) best-effort backtrace
// ---------------------
// Minimal, best-effort approach:
//   - write a short message with write() (async-signal-safe on Unix)
//   - print backtrace via backtrace_symbols_fd (best-effort)
//   - _Exit(130)
//
// Enable by including this header (and DEBUG_ENABLE_SIGINT_BACKTRACE=1).
inline void sigint_backtrace_best_effort() {
#if defined(__unix__) || defined(__APPLE__)
    void* frames[64];
    int n = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
#endif
}

inline void sigint_handler(int) {
#if defined(__unix__) || defined(__APPLE__)
    const char msg[] = "\nCaught SIGINT (Ctrl+C). Best-effort stacktrace:\n";
    ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
    sigint_backtrace_best_effort();
    const char msg2[] = "\nExiting (code 130).\n";
    ::write(STDERR_FILENO, msg2, sizeof(msg2) - 1);
#endif
    std::_Exit(130);
}

// Call this once in main() if you want explicit init.
// If you prefer auto-install, see DEBUG_AUTO_INSTALL_SIGINT below.
inline void install_sigint_backtrace_handler() {
#if DEBUG_ENABLE_SIGINT_BACKTRACE
    std::signal(SIGINT, sigint_handler);
#endif
}

} // namespace debug_detail

// =====================
// Public macros
// =====================
// Works as:
//   DEBUG_STOP();                 // prints location, aborts
//   DEBUG_STOP(k, e, band);       // prints names+values, aborts
//   DEBUG();                      // prints location
//   DEBUG(k, e, band);            // prints names+values
#if DEBUG_ENABLE
  #define DEBUG_STOP(...) \
      do { \
          debug_detail::stop(__FILE__, __LINE__, __func__ \
              __VA_OPT__(, #__VA_ARGS__, __VA_ARGS__)); \
      } while (0)

  #define DEBUG(...) \
      do { \
          debug_detail::debug_print(__FILE__, __LINE__, __func__ \
              __VA_OPT__(, #__VA_ARGS__, __VA_ARGS__)); \
      } while (0)
#else
  #define DEBUG_STOP(...) do { (void)0; } while (0)
  #define DEBUG(...)      do { (void)0; } while (0)
#endif

// =====================
// Optional auto-install for SIGINT handler
// =====================
// If you want the Ctrl+C backtrace handler installed automatically just by including
// this header, define DEBUG_AUTO_INSTALL_SIGINT before including debug.hpp:
//
//   #define DEBUG_AUTO_INSTALL_SIGINT
//   #include "debug.hpp"
//
// Otherwise, call:
//   debug_detail::install_sigint_backtrace_handler();
// in your main().
#if DEBUG_ENABLE_SIGINT_BACKTRACE && defined(DEBUG_AUTO_INSTALL_SIGINT)
namespace debug_detail {
inline const int _debug_sigint_installer = (install_sigint_backtrace_handler(), 0);
}
#endif
