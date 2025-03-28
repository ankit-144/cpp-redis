// debug.h
#ifndef DEBUG_H_
#define DEBUG_H_

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <deque>
#include <stack>
#include <queue>
#include <unordered_map> // Include for map-like check
#include <unordered_set> // Include for sequence-like check
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <utility>      // std::pair, std::forward, std::index_sequence
#include <tuple>        // std::tuple
#include <optional>     // std::optional
#include <variant>      // std::variant
#include <iomanip>      // std::boolalpha
#include <iterator>     // std::begin, std::end

// --- Configuration ---
// Define DBG_OUTPUT_STREAM to change the output stream (default: std::cerr)
#ifndef DBG_OUTPUT_STREAM
#define DBG_OUTPUT_STREAM std::cerr
#endif

// Define DBG_FORCE_ENABLE to always enable debug output, even if NDEBUG is defined
// #define DBG_FORCE_ENABLE

// Disable debug output if NDEBUG is defined, unless DBG_FORCE_ENABLE is set
#if defined(NDEBUG) && !defined(DBG_FORCE_ENABLE)
    #define IS_DEBUG_ENABLED false
#else
    #define IS_DEBUG_ENABLED true
#endif

// --- Implementation Details ---
namespace dbg {

// Forward declaration
template <typename T>
void pretty_print(std::ostream& os, const T& value);

// Helper to print sequences (vector, list, set, etc.)
template <typename T>
void print_sequence(std::ostream& os, const T& container) {
    os << "[";
    bool first = true;
    for (const auto& elem : container) {
        if (!first) {
            os << ", ";
        }
        pretty_print(os, elem);
        first = false;
    }
    os << "]";
}

// Helper to print map-like containers (map, unordered_map)
template <typename T>
void print_map(std::ostream& os, const T& container) {
    os << "{";
    bool first = true;
    for (const auto& pair : container) {
        if (!first) {
            os << ", ";
        }
        pretty_print(os, pair.first);
        os << ": ";
        pretty_print(os, pair.second);
        first = false;
    }
    os << "}";
}

// Helper to print tuples
template<typename Tuple, std::size_t... Is>
void print_tuple_impl(std::ostream& os, const Tuple& t, std::index_sequence<Is...>) {
    os << "(";
    bool first = true;
    // Use fold expression (C++17)
    ((os << (first ? "" : ", "), pretty_print(os, std::get<Is>(t)), first = false), ...);
    os << ")";
}

template<typename... Args>
void print_tuple(std::ostream& os, const std::tuple<Args...>& t) {
    print_tuple_impl(os, t, std::index_sequence_for<Args...>{});
}

// --- SFINAE / Type Trait helpers (C++17 compatible) ---

// Check for stream insertion operator support
template <typename, typename = void>
struct has_ostream_operator : std::false_type {};
template <typename T>
struct has_ostream_operator<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>> : std::true_type {};

// Check if a type is iterable (has begin() and end())
template <typename, typename = void>
struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<T&>())), decltype(std::end(std::declval<T&>()))>> : std::true_type {};

// Check if a type is map-like (has key_type and mapped_type, and is iterable)
template <typename, typename = void>
struct is_map_like : std::false_type {};
template <typename T>
struct is_map_like<T, std::void_t<typename T::key_type, typename T::mapped_type, decltype(std::begin(std::declval<T&>()))>> : std::true_type {};

// Check if a type is std::optional
template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

// Check if a type is std::variant
template <typename T> struct is_variant : std::false_type {};
template <typename... Args> struct is_variant<std::variant<Args...>> : std::true_type {};

// Check if a type is std::pair
template <typename T> struct is_pair : std::false_type {};
template <typename T1, typename T2> struct is_pair<std::pair<T1, T2>> : std::true_type {};

// Check if a type is std::tuple
template <typename T> struct is_tuple : std::false_type {};
template <typename... Args> struct is_tuple<std::tuple<Args...>> : std::true_type {};


// Main pretty_print function using `if constexpr` (C++17)
template <typename T>
void pretty_print(std::ostream& os, const T& value) {
    // Use decay_t to handle arrays and function pointers more gracefully,
    // and remove cv-qualifiers for simpler type matching.
    using DecayedT = std::decay_t<T>;

    if constexpr (std::is_null_pointer_v<DecayedT>) {
        os << "nullptr";
    } else if constexpr (std::is_pointer_v<DecayedT>) {
        os << static_cast<const void*>(value); // Print address
        // Check if it's non-null and not a char* (which is treated as C-string)
        if (value != nullptr && !std::is_same_v<DecayedT, const char*> && !std::is_same_v<DecayedT, char*>) {
            // Avoid dereferencing void* or function pointers safely
            if constexpr (!std::is_void_v<std::remove_pointer_t<DecayedT>> &&
                          !std::is_function_v<std::remove_pointer_t<DecayedT>>) {
                 os << " -> ";
                 pretty_print(os, *value); // Recursively print pointed-to value
            }
        }
    } else if constexpr (std::is_same_v<DecayedT, std::string> || std::is_same_v<DecayedT, std::string_view>) {
        os << std::quoted(std::string_view(value)); // Use std::quoted for proper escaping
    } else if constexpr (std::is_same_v<DecayedT, const char*>) {
        // Handle null C-strings
        if (value == nullptr) {
            os << "nullptr (char*)";
        } else {
             os << std::quoted(value); // Use std::quoted for proper escaping
        }
    } else if constexpr (std::is_same_v<DecayedT, char>) {
        os << "'" << value << "'"; // Keep simple quotes for single char
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        os << std::boolalpha << value;
    } else if constexpr (std::is_arithmetic_v<DecayedT>) {
        os << value; // Includes integers, floats, etc.
    } else if constexpr (is_optional<DecayedT>::value) { // std::optional
        if (value) {
            pretty_print(os, *value);
        } else {
            os << "std::nullopt";
        }
    } else if constexpr (is_variant<DecayedT>::value) { // std::variant
         std::visit([&os](const auto& val) { pretty_print(os, val); }, value);
    } else if constexpr (is_pair<DecayedT>::value) { // std::pair
        os << "(";
        pretty_print(os, value.first);
        os << ", ";
        pretty_print(os, value.second);
        os << ")";
    } else if constexpr (is_tuple<DecayedT>::value) { // std::tuple
         print_tuple(os, value);
    } else if constexpr (is_iterable<DecayedT>::value &&
                         !std::is_same_v<DecayedT, std::string> && // Exclude string types handled above
                         !std::is_same_v<DecayedT, std::string_view>)
    {
        // **** THIS IS THE CORRECTED PART ****
        // Use C++17 SFINAE check instead of C++20 requires clause
        if constexpr (is_map_like<DecayedT>::value) {
            print_map(os, value);
        } else {
            print_sequence(os, value);
        }
        // **** END OF CORRECTION ****
    } else if constexpr (has_ostream_operator<DecayedT>::value) {
        // If the type has an operator<< overload, use it
        os << value;
    } else {
        // Fallback for unknown types: print type name and address
        // Use typeid().name() which might be mangled, but is standard
        os << "[unprintable object of type '" << typeid(DecayedT).name()
           << "' at " << static_cast<const void*>(&value) << "]";
    }
}


// --- Helper for variadic DEBUG macro ---
// Base case for recursion
inline void print_debug_vars(std::ostream& os, const char* names) {
    // If names is empty or just whitespace after the last comma, don't add extra space/newline
    while (*names == ' ' || *names == '\t') ++names;
    if (*names != '\0') {
         os << std::endl; // End of line only if there were variables
    }
}

// Recursive template to print variable names and values
template<typename T, typename... Args>
void print_debug_vars(std::ostream& os, const char* names, const T& first, const Args&... rest) {
    const char* start = names;
    // Skip leading whitespace
    while (*start == ' ' || *start == '\t') ++start;
    const char* comma = start;
    int paren_level = 0; // Handle cases like DEBUG(make_pair(1, 2))
    while (*comma != '\0') {
        if (*comma == '(') paren_level++;
        else if (*comma == ')') paren_level--;
        else if (*comma == ',' && paren_level == 0) break; // Found separator
        ++comma;
    }

    os << " "; // Space before variable name
    os.write(start, comma - start); // Print variable name (trimmed)
    os << " = ";
    pretty_print(os, first); // Print variable value

    if (*comma == ',') {
        os << ";"; // Separator between variables
        print_debug_vars(os, comma + 1, rest...); // Recurse for remaining args
    } else {
        os << std::endl; // End of line after the last variable
    }
}

} // namespace dbg

// --- User-facing Macros ---

// DEBUG(var1, var2, ...) macro
// Prints file:line, variable names, and their values to DBG_OUTPUT_STREAM
// Only active if IS_DEBUG_ENABLED is true
#if IS_DEBUG_ENABLED
    #define DEBUG(...) \
        do { \
            DBG_OUTPUT_STREAM << "[" << __FILE__ << ":" << __LINE__ << "]"; \
            dbg::print_debug_vars(DBG_OUTPUT_STREAM, #__VA_ARGS__, __VA_ARGS__); \
        } while (0)
#else
    #define DEBUG(...) do {} while (0) // Compile away to nothing
#endif

// DEBUG_MSG("message", var1, var2, ...) macro
// Prints a custom message followed by variables
#if IS_DEBUG_ENABLED
    // Use C++17 if constexpr to check if __VA_ARGS__ is empty
    #define DEBUG_MSG(msg, ...) \
        do { \
            DBG_OUTPUT_STREAM << "[" << __FILE__ << ":" << __LINE__ << "] " << msg; \
            if constexpr (sizeof...(__VA_ARGS__) > 0) { \
                 dbg::print_debug_vars(DBG_OUTPUT_STREAM, #__VA_ARGS__, __VA_ARGS__); \
            } else { \
                 DBG_OUTPUT_STREAM << std::endl; \
            } \
        } while (0)
#else
    #define DEBUG_MSG(msg, ...) do {} while (0) // Compile away to nothing
#endif

// --- Force Macros (always enabled) ---
// Useful for critical errors even in release builds, but use sparingly.

// DEBUG_FORCE(var1, var2, ...) macro (always active)
#define DEBUG_FORCE(...) \
    do { \
        DBG_OUTPUT_STREAM << "[FORCED] [" << __FILE__ << ":" << __LINE__ << "]"; \
        dbg::print_debug_vars(DBG_OUTPUT_STREAM, #__VA_ARGS__, __VA_ARGS__); \
    } while (0)

// DEBUG_MSG_FORCE("message", var1, var2, ...) macro (always active)
#define DEBUG_MSG_FORCE(msg, ...) \
    do { \
        DBG_OUTPUT_STREAM << "[FORCED] [" << __FILE__ << ":" << __LINE__ << "] " << msg; \
        /* Need a non-constexpr check here for the macro expansion */ \
        if (sizeof...(__VA_ARGS__) > 0) { \
             dbg::print_debug_vars(DBG_OUTPUT_STREAM, #__VA_ARGS__, __VA_ARGS__); \
        } else { \
             DBG_OUTPUT_STREAM << std::endl; \
        } \
    } while (0)


#endif // DEBUG_H_