#include "operation_result.h"

OperationResult OperationResult::success(const QString& message)
{
    return OperationResult{true, message};
}

OperationResult OperationResult::failure(const QString& message)
{
    return OperationResult{false, message};
}
