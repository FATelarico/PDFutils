#ifndef OPERATION_RESULT_H
#define OPERATION_RESULT_H

#include <QString>

struct OperationResult
{
    bool ok = false;
    QString message;

    static OperationResult success(const QString& message = QString());
    static OperationResult failure(const QString& message);
};

#endif // OPERATION_RESULT_H
