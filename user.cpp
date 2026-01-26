#include "types.h"

Q26_OBJECT_IMPL(User)

Q26_SIGNAL_IMPL(void User::nameChanged())
Q26_SIGNAL_IMPL(void User::valueChanged())

void User::signalWithArgs(int x, double y)
{
    using That = std::remove_pointer_t<decltype(this)>;
    using ThatMO = Qt26::QStaticMetaObjectImpl<That>;
    constexpr std::string_view signal_name = __FUNCTION__;
    constexpr auto index = ThatMO::indexOfMethod(signal_name);

    constexpr auto member = *ThatMO::memberFunction(signal_name);
    using ReturnType = [:return_type_of(member):];
    constexpr auto pCount = parameters_of(member).size();

    QStaticMetaMethod::SignalArgs<pCount, ReturnType> args;

    [:Qt26Private::expand(parameters_of(member)):] >>
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
