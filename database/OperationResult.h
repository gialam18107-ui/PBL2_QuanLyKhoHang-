#ifndef OPERATION_RESULT_H
#define OPERATION_RESULT_H

#include <string>

/**
 * @brief Standard operation result contract for Database & User CRUD operations (Rule 6.3.1).
 * Returns operation status, error/success message, and count of affected records.
 */
struct OperationResult {
    bool success;
    std::string message;
    int affectedCount = 0;

    OperationResult(bool s = false, const std::string& msg = "", int count = 0)
        : success(s), message(msg), affectedCount(count) {}

    operator bool() const { return success; }
};

#endif // OPERATION_RESULT_H

