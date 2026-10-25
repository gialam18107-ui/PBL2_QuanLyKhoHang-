#ifndef OPERATION_RESULT_H
#define OPERATION_RESULT_H

#include <string>

/**
 * @brief Standard operation result contract for User CRUD operations (Rule 6.3.1).
 * Returns operation status, error/success message, and count of affected records.
 */
struct OperationResult {
    bool success;
    std::string message;
    int affectedCount = 0;
};

#endif // OPERATION_RESULT_H

