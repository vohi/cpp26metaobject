#include "user.h"

Q26_OBJECT_IMPL(User)

Q26_SIGNAL_IMPL(void User::nameChanged())
Q26_SIGNAL_IMPL(void User::valueChanged())

void User::signalWithArgs(int x, double y)
{
    constexpr auto qt_signal = std::meta::current_function();
    using That = [:parent_of(qt_signal):];
    using ThatMO = Qt26::QStaticMetaObjectImpl<That>;
    constexpr auto index = ThatMO::indexOfMemberWithType<qt_signal, Qt26::MetaEntity::Signal>();

    using ReturnType = [:return_type_of(qt_signal):];
    constexpr auto pCount = parameters_of(qt_signal).size();

    QStaticMetaMethod::SignalArgs<pCount, ReturnType> args;

    [:Qt26Private::expand(parameters_of(qt_signal)):] >>
    [&, i = 0]<auto param>() mutable {
        // args.m_args[++i] = &[:param:];
        ++i;
        switch (i) {
        case 1:
            args.m_args[i] = &x;
            break;
        case 2:
            args.m_args[i] = &y;
            break;
        }
    };

    ThatMO::activate(this, index, args.m_args.data());
    return args.result();
}

Q26_OBJECT_IMPL(UserNumberAttribute)
Q26_SIGNAL_IMPL(void UserNumberAttribute::numberChanged())

Q26_OBJECT_IMPL(UserWithNumber)
