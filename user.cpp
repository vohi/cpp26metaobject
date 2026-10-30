#include "user.h"

Q26_OBJECT_IMPL(User)

Q26_SIGNAL_IMPL(void User::nameChanged())
Q26_SIGNAL_IMPL(void User::valueChanged())
Q26_SIGNAL_IMPL(void User::signalWithArgs(int, double))

Q26_OBJECT_IMPL(UserNumberAttribute)
Q26_SIGNAL_IMPL(void UserNumberAttribute::numberChanged())

Q26_OBJECT_IMPL(UserWithNumber)
