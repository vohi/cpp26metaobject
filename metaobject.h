#pragma once

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <meta>
#include <ranges>
#include <unordered_set>
#include <utility>

#include <QtCore/qdebug.h>
#include <QtCore/qtmocconstants.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qvariant.h>

struct Object;

namespace Qt26 {
    enum class MetaEntity;
}

namespace Qt26Private
{
    template <typename Fn, std::size_t ...Is>
    static constexpr bool expand_at(std::size_t i, Fn &&fn, std::index_sequence<Is...>)
    {
        return (((Is == i ? (std::forward<Fn>(fn)(std::integral_constant<size_t, Is>{}), true)
                          : false) || ...));
    }

    template <std::size_t S, typename Fn>
    static constexpr bool expand_at(std::size_t i, Fn &&fn)
    {
        return expand_at(i, std::forward<Fn>(fn), std::make_index_sequence<S>{});
    }

    template <typename R>
    consteval auto tuple_type(R range) {
        std::vector<std::meta::info> args;
        for (auto r : range)
            args.push_back(type_of(r));
        return substitute(^^std::tuple, args);
    }

    template <typename Tuple, std::size_t... Is>
    auto make_tuple_impl(void **args, std::index_sequence<Is...>)
    {
        return std::make_tuple(std::ref(*static_cast<std::remove_cvref_t<std::tuple_element_t<Is, Tuple>> *>(args[Is + 1]))...);
    }

    template<typename Tuple>
    auto make_tuple(void **args)
    {
        return make_tuple_impl<Tuple>(args, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    }

    // Function types as used by QtMocHelpers::FunctionData
    template <typename R, typename ...A>
    using FunctionType = R(A...);
    template <typename R, typename ...A>
    using ConstFunctionType = R(A...) const;

    // Like moc's qt_meta_tag_XXX_t: a type that is unique to the translation unit
    // that implements the meta object.
    namespace {
    template <typename Class>
    struct MetaObjectTag {};
    }

    // The QMetaType::Type for types that moc treats as builtin, or UnknownType.
    // Like QMetaObject::normalizedType, "const T &" and "const T" are treated as "T".
    template <typename T>
    inline constexpr int builtinMetaTypeId = [] {
        using U = std::conditional_t<std::is_reference_v<T>, T, std::remove_const_t<T>>;
        if constexpr (QMetaTypeId2<U>::IsBuiltIn)
            return int(QMetaTypeId2<U>::MetaType);
        else
            return int(QMetaType::UnknownType);
    }();

    consteval bool isIdentifierChar(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '_';
    }

    consteval bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // Removes all whitespace that is not needed to separate identifiers,
    // like moc's normalizeType().
    consteval std::string normalizedType(std::string_view type)
    {
        std::string result;
        for (std::size_t i = 0; i < type.size(); ++i) {
            if (!isSpace(type[i])) {
                result += type[i];
                continue;
            }
            std::size_t next = i + 1;
            while (next < type.size() && isSpace(type[next]))
                ++next;
            if (!result.empty() && isIdentifierChar(result.back())
             && next < type.size() && isIdentifierChar(type[next])) {
                result += ' ';
            }
            i = next - 1;
        }
        return result;
    }

    consteval int parseInt(std::string_view text)
    {
        if (text.empty())
            throw std::invalid_argument("Expected an integer");
        int result = 0;
        for (char c : text) {
            if (c < '0' || c > '9')
                throw std::invalid_argument("Expected an integer");
            result = result * 10 + (c - '0');
        }
        return result;
    }

    // A meta method as moc sees it. Methods with default arguments produce
    // additional, cloned, entries with fewer arguments.
    struct MethodInfo
    {
        std::meta::info member;
        Qt26::MetaEntity type;
        std::size_t argc;
        bool cloned;
    };

    // Implements the interface of QtMocHelpers::StringRefStorage that
    // QtMocHelpers::metaObjectData uses, without requiring string literals.
    template <std::size_t Count, std::size_t Size>
    struct MocStringData
    {
        static_assert(Size <= QtMocHelpers::MaxStringSize, "Meta Object data is too big");
        static constexpr int StringCount = int(Count);
        static constexpr std::size_t StringSize = Size;

        char strings[Size] = {}; // null-terminated strings
        uint lengths[Count] = {};

        constexpr void writeTo(uint (&offsets)[2 * Count], char (&data)[Size]) const noexcept
        {
            uint offset = 0;
            for (std::size_t i = 0; i < Count; ++i) {
                offsets[2 * i] = offset + sizeof(offsets);
                offsets[2 * i + 1] = lengths[i];
                offset += lengths[i] + 1;
            }
            for (std::size_t i = 0; i < Size; ++i)
                data[i] = strings[i];
        }

        consteval uint indexOf(std::string_view str) const
        {
            std::size_t offset = 0;
            for (std::size_t i = 0; i < Count; ++i) {
                if (std::string_view(strings + offset, lengths[i]) == str)
                    return uint(i);
                offset += lengths[i] + 1;
            }
            throw std::logic_error("We forgot to register some strings");
        }
    };
}

namespace Qt26 {
    enum class MetaEntity {
        Unknown,
        Enumerator,
        Property,
        ClassInfo,
        Method,
        Signal,
        Slot,
        Constructor,
    };

    template <auto Description>
    struct Property
    {
        static consteval auto operator()() -> std::string_view {
            return Description();
        }
    };

    template <std::meta::info reflection>
    concept IsProperty = has_template_arguments(type_of(reflection))
                      && template_of(type_of(reflection)) == ^^Qt26::Property;

    template <auto Name, auto Value>
    struct ClassInfo
    {
        struct Data
        {
            std::string_view name;
            std::string_view value;
        };

        static consteval auto operator()() -> Data {
            return Data{Name(), Value()};
        }
    };

    template <std::meta::info reflection>
    concept IsClassInfo = has_template_arguments(type_of(reflection))
                       && template_of(type_of(reflection)) == ^^Qt26::ClassInfo;

    template <typename Enum>
    struct Enumerator {
        using type = Enum;
    };

    template <std::meta::info reflection>
    concept IsEnumerator = has_template_arguments(type_of(reflection))
                        && template_of(type_of(reflection)) == ^^Qt26::Enumerator;

    template <typename = void>
    struct Prototype {};

    template <typename R, typename C, typename ...A>
    struct Prototype<R(C::*)(A...)>
    {
        using type = R(*)(A...);
        using Class = C;
    };


    template <typename LHS, typename RHS>
        requires requires (LHS lhs, RHS rhs) {
            lhs == rhs;
        }
    constexpr auto isSameFunction(LHS &&lhs, RHS &&rhs)
    {
        return lhs == rhs;
    }

    template <typename LHS, typename RHS>
    auto isSameFunction(LHS &&lhs, RHS &&rhs) { return false; }

    template <typename Signal>
    static std::string_view signalName(Signal &&signal)
    {
        using Class = Prototype<std::remove_cvref_t<Signal>>::Class;
        std::string_view result;
        template for (constexpr auto member
                      : std::define_static_array(members_of(^^Class, std::meta::access_context::current()))) {
            if constexpr (is_function(member) && has_identifier(member)
                       && !is_function_template(member) && !is_special_member_function(member)) {
                constexpr auto mfp = &[:member:];
                if (isSameFunction(mfp, signal)) {
                    result = identifier_of(member);
                    break;
                }
            }
        }
        return result;
    }
}

template <typename T>
concept TypeWithStaticMetaObject = requires {
    typename T::QStaticMetaObjectData;
};

struct QStaticMetaData
{
    consteval QStaticMetaData(Qt26::MetaEntity objectType = {})
        : m_metaEntity(objectType)
    {}

    Qt26::MetaEntity m_metaEntity = {};
    std::string_view m_name;
    std::size_t m_index = -1;

    constexpr std::size_t size() const
    {
        return m_name.size() + 1;
    }

    friend inline QDebug operator<<(QDebug dbg, const QStaticMetaData &that);
};

struct QStaticMetaProperty : QStaticMetaData
{
    // Parses "type name ATTRIBUTE value ... FLAG ..." following moc's Q_PROPERTY grammar
    consteval QStaticMetaProperty(std::string_view description = {})
        : QStaticMetaData(Qt26::MetaEntity::Property)
    {
        std::vector<std::string_view> tokens;
        for (std::size_t i = 0; i < description.size();) {
            while (i < description.size() && Qt26Private::isSpace(description[i]))
                ++i;
            std::size_t end = i;
            while (end < description.size() && !Qt26Private::isSpace(description[end]))
                ++end;
            if (end > i)
                tokens.push_back(description.substr(i, end - i));
            i = end;
        }
        if (tokens.empty())
            return;

        constexpr std::string_view keywords[] = {
            "READ", "WRITE", "MEMBER", "RESET", "NOTIFY", "REVISION", "DESIGNABLE",
            "SCRIPTABLE", "STORED", "USER", "BINDABLE", "CONSTANT", "FINAL",
            "REQUIRED", "VIRTUAL", "OVERRIDE",
        };
        const auto isKeyword = [&keywords](std::string_view token) {
            return std::ranges::find(keywords, token) != std::ranges::end(keywords)
                || token.starts_with("REVISION(");
        };

        std::size_t k = 0;
        while (k < tokens.size() && !isKeyword(tokens[k]))
            ++k;
        if (k < 2)
            throw std::invalid_argument("Property declaration must start with type and name");

        std::string type;
        for (std::size_t i = 0; i < k - 1; ++i) {
            if (i)
                type += ' ';
            type += tokens[i];
        }
        std::string_view name = tokens[k - 1];
        while (name.starts_with('*') || name.starts_with('&')) {
            type += name.front();
            name.remove_prefix(1);
        }
        type = Qt26Private::normalizedType(type);
        // same special cases as in Moc::createPropertyDef
        if (type == "QMap")
            type = "QMap<QString,QVariant>";
        else if (type == "LongLong")
            type = "qlonglong";
        else if (type == "ULongLong")
            type = "qulonglong";
        m_type = std::define_static_string(type);
        m_name = name;

        while (k < tokens.size()) {
            const std::string_view key = tokens[k++];
            if (key == "CONSTANT") {
                m_constant = true;
            } else if (key == "FINAL") {
                m_final = true;
            } else if (key == "REQUIRED") {
                m_required = true;
            } else if (key == "VIRTUAL") {
                m_virtual = true;
            } else if (key == "OVERRIDE") {
                m_override = true;
            } else if (key.starts_with("REVISION(")) {
                // REVISION(major, minor), possibly split into several tokens
                std::string revision(key.substr(9));
                while (!revision.ends_with(')') && k < tokens.size())
                    revision += tokens[k++];
                if (!revision.ends_with(')'))
                    throw std::invalid_argument("Unterminated REVISION");
                revision.pop_back();
                const std::string_view text = revision;
                const auto comma = text.find(',');
                const int major = Qt26Private::parseInt(text.substr(0, comma));
                const int minor = comma == text.npos ? 0
                                                     : Qt26Private::parseInt(text.substr(comma + 1));
                m_revision = (major << 8) | minor;
            } else {
                if (k == tokens.size())
                    throw std::invalid_argument("Property attribute without value");
                const std::string_view value = tokens[k++];
                if (key == "READ")
                    m_getter = value;
                else if (key == "WRITE")
                    m_setter = value;
                else if (key == "MEMBER")
                    m_member = value;
                else if (key == "RESET")
                    m_reset = value;
                else if (key == "NOTIFY")
                    m_notify = value;
                else if (key == "BINDABLE")
                    m_bindable = value;
                else if (key == "DESIGNABLE")
                    m_designable = value;
                else if (key == "SCRIPTABLE")
                    m_scriptable = value;
                else if (key == "STORED")
                    m_stored = value;
                else if (key == "USER")
                    m_user = value;
                else if (key == "REVISION") // QTypeRevision::fromMinorVersion, major unknown
                    m_revision = (0xff << 8) | Qt26Private::parseInt(value);
                else
                    throw std::invalid_argument("Unknown property attribute");
            }
        }
    }

    friend QDebug operator<<(QDebug dbg, const QStaticMetaProperty &that)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace().noquote();
        dbg << static_cast<const QStaticMetaData &>(that) << ": ";
        dbg << that.m_type << " " << that.m_name;
        return dbg;
    }

    constexpr std::size_t size() const
    {
        return QStaticMetaData::size()
             + m_type.size() + 1
             + m_getter.size() + 1
             + m_setter.size() + 1
             + m_notify.size() + 1;
    }

    std::string_view m_type;
    std::string_view m_getter;
    std::string_view m_setter;
    std::string_view m_notify;
    std::string_view m_member;
    std::string_view m_reset;
    std::string_view m_bindable;
    std::string_view m_designable = "true";
    std::string_view m_scriptable = "true";
    std::string_view m_stored = "true";
    std::string_view m_user = "false";
    int m_revision = 0;
    bool m_constant = false;
    bool m_final = false;
    bool m_required = false;
    bool m_virtual = false;
    bool m_override = false;
};

struct QStaticMetaEnum : QStaticMetaData
{
    consteval QStaticMetaEnum()
        : QStaticMetaData(Qt26::MetaEntity::Enumerator)
    {}

    struct Item {
        std::string_view name;
        long value = -1;
    };
    std::vector<Item>(*items)() = nullptr;

    constexpr std::size_t size() const
    {
        std::size_t bytes = QStaticMetaData::size();
        for (const auto &item : items()) {
            bytes += item.name.size() + 1;
            bytes += sizeof(Item::value);
        }
        return bytes;
    }

    friend QDebug operator<<(QDebug dbg, const QStaticMetaEnum &that)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace().noquote();

        dbg << static_cast<const QStaticMetaData &>(that) << ": ";
        dbg << that.m_name << "(";
        int i = that.items().size();
        for (auto item : that.items()) {
            dbg << item.name << "=" << item.value;
            if (--i)
                dbg << ", ";
        }
        dbg << ")";
        return dbg;
    }
};

template <typename Enum>
    requires std::is_enum_v<Enum>
struct QStaticMetaEnumImpl : QStaticMetaEnum
{
    static constexpr std::size_t item_count = enumerators_of(^^Enum).size();

    static constexpr std::vector<Item> items()
    {
        std::vector<Item> result;

        template for (constexpr auto item : std::define_static_array(enumerators_of(^^Enum))) {
            result.emplace_back(
                identifier_of(item),
                static_cast<long>(extract<Enum>(constant_of(item)))
            );
        }

        return result;
    }

    consteval QStaticMetaEnumImpl(Enum enumerator = {})
    {
        m_name = identifier_of(^^Enum);
        QStaticMetaEnum::items = &QStaticMetaEnumImpl::items;
    }
};

struct QStaticMetaClassInfo : QStaticMetaData
{
    consteval QStaticMetaClassInfo(std::string_view name, std::string_view value)
        : QStaticMetaData(Qt26::MetaEntity::ClassInfo)
        , m_value(value)
    {
        m_name = name;
    }

    std::string_view m_value;

    constexpr std::size_t size() const
    {
        return QStaticMetaData::size()
             + m_value.size() + 1;
    }

    friend QDebug operator<<(QDebug dbg, const QStaticMetaClassInfo &that)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace().noquote();

        dbg << static_cast<const QStaticMetaData &>(that) << ": ";
        dbg << that.m_name << " = " << that.m_value;
        return dbg;
    }
};

struct QStaticMetaMethod : QStaticMetaData
{
    consteval QStaticMetaMethod(Qt26::MetaEntity type, std::string_view name)
        : QStaticMetaData(type), m_name(name)
    {}

    std::string_view m_name;

    struct Parameter {
        QMetaType type;
        std::string_view name;
    };
    std::vector<Parameter> m_parameters;

    constexpr std::size_t size() const
    {
        std::size_t bytes = QStaticMetaData::size();
        for (const auto &param : m_parameters) {
            bytes += param.name.size() + 1
                   + sizeof(Parameter::type);
        }
        return bytes;
    }
    template <std::size_t count, typename Ret>
    struct SignalArgs
    {
        Ret result()
        {
            return *static_cast<Ret *>(m_args[0]);
        }

        Ret m_result = {};
        std::array<void *, count + 1> m_args;
    };

    template <std::size_t count>
    struct SignalArgs<count, void>
    {
        void result() {}

        std::array<void *, count + 1> m_args;
    };

    friend QDebug operator<<(QDebug dbg, const QStaticMetaMethod &that)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace().noquote();

        dbg << static_cast<const QStaticMetaData &>(that) << ": ";
        dbg << that.m_name << "(";
        std::size_t pcount = that.m_parameters.size();
        for (auto parameter : that.m_parameters) {
            dbg << parameter.type.name() << " " << parameter.name;
            if (--pcount)
                dbg << ", ";
        }
        dbg << ")";
        return dbg;
    }
};

QDebug operator<<(QDebug dbg, const QStaticMetaData &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();

    constexpr QStaticMetaEnumImpl<Qt26::MetaEntity> enumInfo;
    for (auto item : enumInfo.items()) {
        if (item.value == std::to_underlying(that.m_metaEntity))
            dbg << item.name;
    }

    return dbg;
}

namespace Qt26 {

struct QStaticMetaObject
{
    template <typename Class>
    static constexpr auto fromType();

    std::string_view (*className)() = nullptr;
    std::vector<std::string_view> (*bases)() = nullptr;

    std::vector<QStaticMetaProperty> (*properties)() = nullptr;
    int (*propertyOffset)() = nullptr;
    int (*propertyCount)() = nullptr;
    QStaticMetaProperty (*property)(int) = nullptr;
    int (*indexOfProperty)(std::string_view) = nullptr;

    std::vector<QStaticMetaEnum> (*enumerators)() = nullptr;
    int (*enumeratorOffset)() = nullptr;
    int (*enumeratorCount)() = nullptr;
    QStaticMetaEnum (*enumerator)(int) = nullptr;
    int (*indexOfEnumerator)(std::string_view) = nullptr;

    std::vector<QStaticMetaClassInfo> (*classInfos)() = nullptr;
    int (*classInfoOffset)() = nullptr;
    int (*classInfoCount)() = nullptr;
    QStaticMetaClassInfo (*classInfo)(int) = nullptr;
    int (*indexOfClassInfo)(std::string_view) = nullptr;

    std::vector<QStaticMetaMethod> (*methods)() = nullptr;
    int (*methodOffset)() = nullptr;
    int (*methodCount)() = nullptr;
    QStaticMetaMethod (*method)(int) = nullptr;
    int (*indexOfMethod)(std::string_view) = nullptr;

    int (*static_metacall)(Object *, QMetaObject::Call, int, void **) = nullptr;

    friend QDebug operator<<(QDebug dbg, const QStaticMetaObject &that)
    {
        {
            QDebugStateSaver saver(dbg);
            dbg.nospace().noquote() << "class " << that.className();
            const auto bases = that.bases();
            if (const std::size_t count = bases.size()) {
                dbg << " : ";
                for (std::size_t i = 0; i < count; ++i) {
                    dbg << bases.at(i);
                    if (i < count - 1)
                        dbg << ", ";
                }
            }
        }
        dbg << Qt::endl;
        QDebugStateSaver saver(dbg);
        dbg << "ClassInfos:" << that.classInfoOffset() << " + "
                             << that.classInfoCount() - that.classInfoOffset() << Qt::endl;
        for (const auto &data : that.classInfos())
            dbg << "- " << data << Qt::endl;

        dbg << "Properties:" << that.propertyOffset() << " + "
                             << that.propertyCount() - that.propertyOffset() << Qt::endl;
        for (const auto &data : that.properties())
            dbg << "- " << data << Qt::endl;

        dbg << "Enumerators:" << that.enumeratorOffset() << " + "
                              << that.enumeratorCount() - that.enumeratorOffset() << Qt::endl;
        for (const auto &data : that.enumerators())
            dbg << "- " << data << Qt::endl;

        dbg << "Methods:" << that.methodOffset() << " + "
                          << that.methodCount() - that.methodOffset() << Qt::endl;
        for (const auto &data : that.methods())
            dbg << "- " << data << Qt::endl;
        return dbg;
    }

protected:
    template <std::meta::info R>
    static consteval auto annotationData()
    {
        return QStaticMetaData{};
    }

    template <std::meta::info R>
        requires IsProperty<R>
    static consteval auto annotationData()
    {
        constexpr auto prop = [:constant_of(R):];
        QStaticMetaProperty result = prop();
        return result;
    }

    template <std::meta::info R>
        requires IsEnumerator<R>
    static consteval auto annotationData()
    {
        using enumerator = [:type_of(constant_of(R)):]::type;
        QStaticMetaEnumImpl result = enumerator{};
        return result;
    }

    template <std::meta::info R>
        requires IsClassInfo<R>
    static consteval auto annotationData()
    {
        constexpr auto classInfo = [:constant_of(R):];
        constexpr auto classInfoData = classInfo();
        QStaticMetaClassInfo result = {classInfoData.name, classInfoData.value};
        return result;
    }

protected:
    explicit QStaticMetaObject() = default;
};

template <typename Class>
struct QStaticMetaObjectImpl : QStaticMetaObject
{
public:
    friend Class;

    // Like moc, include private members in the meta object
    static consteval std::meta::access_context accessContext()
    {
        return std::meta::access_context::unchecked();
    }

    static consteval std::optional<std::meta::info> memberFunction(std::string_view name)
    {
        for (auto member : members_of(^^Class, accessContext())) {
            if (has_identifier(member) && is_function(member) && identifier_of(member) == name) {
                return member;
            }
        }
        return std::nullopt;
    }

    // The meta methods of Class in the order moc generates them: signals,
    // slots, invokable methods, each in declaration order, and each method
    // with default arguments followed by its clones with fewer arguments.
    static consteval std::span<const Qt26Private::MethodInfo> methodInfos()
    {
        std::vector<Qt26Private::MethodInfo> result;
        constexpr Qt26::MetaEntity order[] = {
            Qt26::MetaEntity::Signal, Qt26::MetaEntity::Slot, Qt26::MetaEntity::Method
        };
        for (const auto entity : order) {
            for (auto member : members_of(^^Class, accessContext())) {
                if (!is_function(member) || !has_identifier(member)
                 || is_function_template(member) || is_special_member_function(member)) {
                    continue;
                }
                const auto annotations = annotations_of_with_type(member, ^^Qt26::MetaEntity);
                if (annotations.size() != 1 || extract<Qt26::MetaEntity>(annotations.front()) != entity)
                    continue;
                const auto parameters = parameters_of(member);
                std::size_t argc = parameters.size();
                result.push_back({member, entity, argc, false});
                while (argc > 0 && has_default_argument(parameters[argc - 1])) {
                    --argc;
                    result.push_back({member, entity, argc, true});
                }
            }
        }
        return std::define_static_array(result);
    }

    static consteval std::span<const std::meta::info> parametersOf(Qt26Private::MethodInfo info)
    {
        return std::define_static_array(parameters_of(info.member) | std::views::take(info.argc));
    }

    template <std::meta::info needle, Qt26::MetaEntity entity>
    static consteval std::size_t indexOfMemberWithType()
    {
        std::size_t index = methodOffset();
        for (const auto &info : methodInfos()) {
            if (info.member == needle && info.type == entity && !info.cloned)
                return index;
            ++index;
        }
        throw std::logic_error("Not a meta method of the requested type");
    }


    static consteval std::size_t annotationCount()
    {
        return annotations_of(^^ typename Class::QStaticMetaObjectData).size();
    }

    static consteval std::span<const std::size_t> annotationIndices()
    {
        return std::define_static_array(std::views::iota(std::size_t(0), annotationCount()));
    }

    static consteval auto metaObjectAnnotation(auto idx)
    {
        constexpr auto info = annotations_of(^^typename Class::QStaticMetaObjectData).at(idx);
        auto result = annotationData<info>();
        result.m_index = idx;
        return result;
    }

public:
    static consteval void getUniqueBases(std::vector<std::meta::info> &bases)
    {
        template for (constexpr auto base
                      : std::define_static_array(bases_of(^^Class, std::meta::access_context::current()))) {
            constexpr auto baseType = type_of(base);
            if (!is_virtual_base_of_type(baseType, ^^Class)) {
                bases.insert(bases.begin(), baseType);
            } else {
                auto match = std::find(bases.begin(), bases.end(), baseType);
                if (match == bases.end())
                    bases.insert(bases.begin(), baseType);
                else if (match != bases.begin())
                    std::rotate(bases.begin(), match, match + 1);
            }
            using Base = [:baseType:];
            using BaseMO = Qt26::QStaticMetaObjectImpl<Base>;
            BaseMO::getUniqueBases(bases);
        }
    }

    static consteval auto uniqueBases()
    {
        std::vector<std::meta::info> bases;
        getUniqueBases(bases);
        // std::reverse(bases.begin(), bases.end());
        return std::define_static_array(bases);
    }

    template <typename Fn>
    static constexpr auto withBases(Fn &&fn)
    {
        bool cont = true;
        template for (constexpr auto base : uniqueBases()) {
            using Base = [:base:];
            using BaseMO = Qt26::QStaticMetaObjectImpl<Base>;
            cont = cont && (std::forward<Fn>(fn).template operator()<BaseMO>(), true);
        }
        return cont;
    }

public:
    static constexpr std::string_view className()
    {
        if constexpr (has_identifier(^^Class))
            return identifier_of(^^Class);
        else
            return display_string_of(^^Class);
    }

    static constexpr std::vector<std::string_view> bases()
    {
        std::vector<std::string_view> result;
        template for (constexpr auto base
                      : std::define_static_array(bases_of(^^Class, std::meta::access_context::current()))) {
            using Base = [:type_of(base):];
            using BaseMO = Qt26::QStaticMetaObjectImpl<Base>;
            result.push_back(BaseMO::className());
        }

        return result;
    }

    static constexpr std::vector<QStaticMetaProperty> directProperties()
    {
        std::vector<QStaticMetaProperty> result;
        template for (constexpr std::size_t idx : annotationIndices()) {
            constexpr auto data = metaObjectAnnotation(std::integral_constant<std::size_t, idx>{});
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::Property)
                result.push_back(data);
        }
        return result;
    }

    static constexpr std::vector<QStaticMetaProperty> properties()
    {
        std::vector<QStaticMetaProperty> result;
        withBases([&result]<typename BaseMO>{
            result.append_range(BaseMO::directProperties());
        });

        result.append_range(directProperties());
        return result;
    }

    static constexpr int propertyOffset()
    {
        int offset = 0;
        withBases([&offset]<typename BaseMO>{
            offset += BaseMO::directProperties().size();
        });
        return offset;
    }

    static constexpr int propertyCount()
    {
        return properties().size();
    }

    static constexpr int indexOfProperty(std::string_view name)
    {
        int i = 0;
        for (const auto &property : properties()) {
            if (property.m_name == name)
                return i;
            ++i;
        }
        return -1;
    }

    static constexpr QStaticMetaProperty property(int idx)
    {
        if (idx < 0 || idx >= propertyCount())
            return {};
        return properties().at(idx);
    }

    static constexpr std::vector<QStaticMetaEnum> directEnumerators()
    {
        std::vector<QStaticMetaEnum> result;
        template for (constexpr std::size_t idx : annotationIndices()) {
            constexpr auto data = metaObjectAnnotation(std::integral_constant<std::size_t, idx>{});
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::Enumerator)
                result.push_back(data);
        }
        return result;
    }

    static constexpr std::vector<QStaticMetaEnum> enumerators()
    {
        std::vector<QStaticMetaEnum> result;
        withBases([&result]<typename BaseMO>{
            result.append_range(BaseMO::directEnumerators());
        });

        result.append_range(directEnumerators());
        return result;
    }

    static constexpr int enumeratorOffset()
    {
        int offset = 0;
        withBases([&offset]<typename BaseMO>{
            offset += BaseMO::directEnumerators().size();
        });
        return offset;
    }

    static constexpr int enumeratorCount()
    {
        constexpr auto count = enumerators().size();
        return count;
    }

    static constexpr int indexOfEnumerator(std::string_view name)
    {
        int i = 0;
        for (const auto &enumerator : enumerators()) {
            if (enumerator.m_name == name)
                return i;
            ++i;
        }
        return -1;
    }

    static constexpr QStaticMetaEnum enumerator(int idx)
    {
        return enumerators().at(idx);
    }

    static constexpr std::vector<QStaticMetaClassInfo> directClassInfos()
    {
        std::vector<QStaticMetaClassInfo> result;
        template for (constexpr std::size_t idx : annotationIndices()) {
            constexpr auto data = metaObjectAnnotation(std::integral_constant<std::size_t, idx>{});
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::ClassInfo)
                result.push_back(data);
        }
        return result;
    }

    static constexpr std::vector<QStaticMetaClassInfo> classInfos()
    {
        std::vector<QStaticMetaClassInfo> result;
        withBases([&result]<typename BaseMO>{
            result.append_range(BaseMO::directClassInfos());
        });

        result.append_range(directClassInfos());

        return result;
    }

    static constexpr int classInfoOffset()
    {
        int offset = 0;
        withBases([&offset]<typename BaseMO>{
            offset += BaseMO::directClassInfos().size();
        });
        return offset;
    }

    static constexpr int classInfoCount()
    {
        constexpr auto count = classInfos().size();
        return count;
    }

    static constexpr int indexOfClassInfo(std::string_view name)
    {
        int i = 0;
        for (const auto &classInfo : classInfos()) {
            if (classInfo.m_name == name)
                return i;
            ++i;
        }
        return -1;
    }

    static constexpr QStaticMetaClassInfo classInfo(int idx)
    {
        return classInfos().at(idx);
    }

    static constexpr std::vector<QStaticMetaMethod> directMethods()
    {
        std::vector<QStaticMetaMethod> result;
        std::size_t i = 0;
        template for (constexpr Qt26Private::MethodInfo info : methodInfos()) {
            static_assert(return_type_of(info.member) == ^^void,
                          "Meta-methods with return type are not supported yet!");
            QStaticMetaMethod method(info.type, identifier_of(info.member));
            method.m_index = i++;
            template for (constexpr auto parameter : parametersOf(info)) {
                using type = [:type_of(parameter):];
                method.m_parameters.push_back({QMetaType::fromType<std::remove_cvref_t<type>>(),
                                               parameterName(parameter)});
            }
            result.push_back(method);
        }
        return result;
    }

    static constexpr std::vector<QStaticMetaMethod> methods()
    {
        std::vector<QStaticMetaMethod> result;
        withBases([&result]<typename BaseMO>{
            result.append_range(BaseMO::directMethods());
        });

        result.append_range(directMethods());

        return result;
    }

    static constexpr int methodOffset()
    {
        int offset = 0;
        withBases([&offset]<typename BaseMO>{
            offset += BaseMO::directMethods().size();
        });
        return offset;
    }

    static constexpr int methodCount()
    {
        return methods().size();
    }

    static constexpr QStaticMetaMethod method(int idx)
    {
        return methods().at(idx);
    }

    static constexpr int indexOfMethod(std::string_view name)
    {
        const bool fullPrototype = name.ends_with(')');
        int i = 0;
        for (const auto &method : methods()) {
            if (fullPrototype) {
            } else if (method.m_name == name) {
                return i;
            }
            ++i;
        }
        return -1;
    }

    static constexpr int signalCount()
    {
        int count = 0;
        for (const auto &method : methods()) {
            if (method.m_metaEntity == Qt26::MetaEntity::Signal)
                ++count;
        }
        return count;
    }

    static constexpr std::string_view signal(int idx)
    {
        std::string_view name;
        for (const auto &method : methods()) {
            if (method.m_metaEntity == Qt26::MetaEntity::Signal) {
                if (!idx--)
                    break;
            }
        }

        return name;
    }

    static constexpr int indexOfSignal(std::string_view name)
    {
        int count = 0;
        for (const auto &method : methods()) {
            if (method.m_metaEntity == Qt26::MetaEntity::Signal) {
                if (method.m_name == name)
                    return count;
                ++count;
            }
        }
        return -1;
    }

    static constexpr int direct_static_metacall(Object *o, QMetaObject::Call call, int what, void **args)
    {
        Class *object = nullptr;
        if constexpr (is_virtual_base_of_type(^^Object, ^^Class))
            object = dynamic_cast<Class *>(o);
        else
            object = static_cast<Class *>(o);
        switch (call) {
        case QMetaObject::ReadProperty:
        case QMetaObject::WriteProperty:
            if (what < directProperties().size()) {
                const QStaticMetaProperty prop = directProperties().at(what);
                Qt26Private::expand_at<annotationCount()>(prop.m_index, [&](auto Idx){
                    constexpr auto metaData = metaObjectAnnotation(Idx);
                    if constexpr (metaData.m_metaEntity == Qt26::MetaEntity::Property) {
                        if (call == QMetaObject::ReadProperty) {
                            constexpr auto member = memberFunction(metaData.m_getter);
                            if constexpr (member)
                                *static_cast<QVariant *>(args[0]) = object->[:*member:]();
                        } else {
                            constexpr auto member = memberFunction(metaData.m_setter);
                            if constexpr (member && parameters_of(*member).size() == 1) {
                                const QVariant &value = *static_cast<QVariant *>(args[1]);
                                constexpr auto objectType = type_of(parameters_of(*member).at(0));
                                using paramType = [:remove_cvref(objectType):];
                                if (value.canConvert<paramType>()) {
                                    object->[:*member:](value.value<paramType>());
                                }
                            }
                        }
                    }
                });
            }
            what -= directProperties().size();
            break;
        case QMetaObject::InvokeMetaMethod:
            if (what < directMethods().size()) {
                const QStaticMetaMethod method = directMethods().at(what);
                Qt26Private::expand_at<methodInfos().size()>(method.m_index, [&](auto Idx){
                    constexpr Qt26Private::MethodInfo info = methodInfos()[Idx];
                    // cloned methods are called with fewer arguments
                    using ArgTuple = [:Qt26Private::tuple_type(parametersOf(info)):];
                    auto callargs = Qt26Private::make_tuple<ArgTuple>(args);
                    std::apply([object](auto &&...a) {
                        object->[:info.member:](a...);
                    }, callargs);
                });
            }
            what -= directMethods().size();
            break;
        }
        return what;
    }

    static constexpr int static_metacall(Object *o, QMetaObject::Call call, int what, void **args)
    {
        if (what < 0)
            return what;
        withBases([&]<typename BaseMO>{
            what = BaseMO::direct_static_metacall(o, call, what, args);
            return what >= 0;
        });
        if (what < 0)
            return what;

        return direct_static_metacall(o, call, what, args);
    }

    static void activate(Class *that, int index, void **args)
    {
        if (index >= 0) {
            std::string_view name = method(index).m_name;
            for (const auto &function : that->connections[name]) {
                function->call(nullptr, args);
            }
        }
    }

    consteval QStaticMetaObjectImpl()
    {
        QStaticMetaObject::className = &QStaticMetaObjectImpl::className;
        QStaticMetaObject::bases = &QStaticMetaObjectImpl::bases;

        QStaticMetaObject::properties = &QStaticMetaObjectImpl::properties;
        QStaticMetaObject::propertyOffset = &QStaticMetaObjectImpl::propertyOffset;
        QStaticMetaObject::propertyCount = &QStaticMetaObjectImpl::propertyCount;
        QStaticMetaObject::property = &QStaticMetaObjectImpl::property;
        QStaticMetaObject::indexOfProperty = &QStaticMetaObjectImpl::indexOfProperty;

        QStaticMetaObject::enumerators = &QStaticMetaObjectImpl::enumerators;
        QStaticMetaObject::enumeratorOffset = &QStaticMetaObjectImpl::enumeratorOffset;
        QStaticMetaObject::enumeratorCount = &QStaticMetaObjectImpl::enumeratorCount;
        QStaticMetaObject::enumerator = &QStaticMetaObjectImpl::enumerator;
        QStaticMetaObject::indexOfEnumerator = &QStaticMetaObjectImpl::indexOfEnumerator;

        QStaticMetaObject::classInfos = &QStaticMetaObjectImpl::classInfos;
        QStaticMetaObject::classInfoOffset = &QStaticMetaObjectImpl::classInfoOffset;
        QStaticMetaObject::classInfoCount = &QStaticMetaObjectImpl::classInfoCount;
        QStaticMetaObject::classInfo = &QStaticMetaObjectImpl::classInfo;
        QStaticMetaObject::indexOfClassInfo = &QStaticMetaObjectImpl::indexOfClassInfo;

        QStaticMetaObject::methods = &QStaticMetaObjectImpl::methods;
        QStaticMetaObject::methodOffset = &QStaticMetaObjectImpl::methodOffset;
        QStaticMetaObject::methodCount = &QStaticMetaObjectImpl::methodCount;
        QStaticMetaObject::method = &QStaticMetaObjectImpl::method;
        QStaticMetaObject::indexOfMethod = &QStaticMetaObjectImpl::indexOfMethod;

        QStaticMetaObject::static_metacall = &QStaticMetaObjectImpl::static_metacall;
    }

private:
    // Helpers that produce the strings and type information the way moc does

    static consteval std::string_view parameterName(std::meta::info parameter)
    {
        return has_identifier(parameter) ? identifier_of(parameter) : std::string_view("");
    }

    // moc doesn't support references as return types, and uses void instead
    static consteval std::meta::info returnType(std::meta::info member)
    {
        const auto type = return_type_of(member);
        return is_reference_type(type) ? ^^void : type;
    }

    static consteval int builtinTypeId(std::meta::info type)
    {
        return extract<int>(substitute(^^Qt26Private::builtinMetaTypeId, {type}));
    }

    static consteval bool isBuiltinType(std::meta::info type)
    {
        return builtinTypeId(type) != QMetaType::UnknownType;
    }

    // The fully qualified class name, as in moc's ClassDef::qualified
    static consteval std::string_view qualifiedClassName()
    {
        if (!has_identifier(^^Class))
            return display_string_of(^^Class);
        std::string name(identifier_of(^^Class));
        for (auto scope = parent_of(^^Class);
             scope != ^^:: && (is_namespace(scope) || is_type(scope));
             scope = parent_of(scope)) {
            if (has_identifier(scope)) // skip anonymous namespaces
                name = std::string(identifier_of(scope)) + "::" + name;
        }
        return std::define_static_string(name);
    }

    // The normalized name of a type, for types that are not builtin. Unlike
    // moc, we don't have the type as spelled in the source code, so we use
    // the display string, without the scope of the class itself.
    static consteval std::string_view typeName(std::meta::info type)
    {
        if (is_lvalue_reference_type(type) && is_const(remove_reference(type)))
            type = remove_const(remove_reference(type));
        else if (!is_reference_type(type) && is_const(type))
            type = remove_const(type);
        std::string name = Qt26Private::normalizedType(display_string_of(type));
        const std::string scope = std::string(qualifiedClassName()) + "::";
        if (name.starts_with(scope))
            name.erase(0, scope.size());
        return std::define_static_string(name);
    }

    // The types (as written in Q26_ENUM) of the enums registered for Class
    static consteval std::span<const std::meta::info> enumTypes()
    {
        std::vector<std::meta::info> result;
        for (auto annotation : annotations_of(^^typename Class::QStaticMetaObjectData)) {
            const auto type = type_of(annotation);
            if (has_template_arguments(type) && template_of(type) == ^^Qt26::Enumerator)
                result.push_back(template_arguments_of(type).front());
        }
        return std::define_static_array(result);
    }

    // The type of a property, from its MEMBER variable or its READ function
    static consteval std::meta::info propertyType(const QStaticMetaProperty &property)
    {
        for (auto member : members_of(^^Class, accessContext())) {
            if (!has_identifier(member))
                continue;
            if (!property.m_member.empty() && is_nonstatic_data_member(member)
             && identifier_of(member) == property.m_member) {
                return remove_cvref(type_of(member));
            }
            if (property.m_member.empty() && is_function(member) && !is_function_template(member)
             && identifier_of(member) == property.m_getter && parameters_of(member).empty()) {
                return remove_cvref(return_type_of(member));
            }
        }
        throw std::invalid_argument("Cannot determine the type of the property");
    }

    // Index of the signal within the signals of Class, or -1
    static consteval int notifySignalIndex(std::string_view name)
    {
        int index = 0;
        for (const auto &info : methodInfos()) {
            if (info.type != Qt26::MetaEntity::Signal)
                break;
            if (identifier_of(info.member) == name)
                return index;
            ++index;
        }
        return -1;
    }

    // NOTIFY signals that are not declared in Class, like moc's nonClassSignalList
    static consteval std::vector<std::string_view> nonClassSignals()
    {
        std::vector<std::string_view> result;
        for (const auto &property : directProperties()) {
            if (!property.m_notify.empty() && notifySignalIndex(property.m_notify) == -1
             && std::ranges::find(result, property.m_notify) == result.end()) {
                result.push_back(property.m_notify);
            }
        }
        return result;
    }

    // All strings, registered in the same order as moc's Generator::generateCode
    static consteval std::vector<std::string_view> collectStrings()
    {
        std::vector<std::string_view> strings;
        const auto strreg = [&strings](std::string_view str) {
            if (std::ranges::find(strings, str) == strings.end())
                strings.push_back(str);
        };

        strreg(qualifiedClassName());
        for (const auto &classInfo : directClassInfos()) {
            strreg(classInfo.m_name);
            strreg(classInfo.m_value);
        }
        for (const auto &info : methodInfos()) {
            strreg(identifier_of(info.member));
            if (const auto type = returnType(info.member); !isBuiltinType(type))
                strreg(typeName(type));
            strreg(""); // tag
            for (auto parameter : parametersOf(info)) {
                if (const auto type = type_of(parameter); !isBuiltinType(type))
                    strreg(typeName(type));
                strreg(parameterName(parameter));
            }
        }
        for (auto signal : nonClassSignals())
            strreg(signal);
        for (const auto &property : directProperties()) {
            strreg(property.m_name);
            if (!isBuiltinType(propertyType(property)))
                strreg(property.m_type);
        }
        for (auto enumType : enumTypes()) {
            strreg(identifier_of(enumType));
            for (auto enumerator : enumerators_of(dealias(enumType)))
                strreg(identifier_of(enumerator));
        }
        return strings;
    }

    static consteval auto mocStrings()
    {
        constexpr std::size_t count = collectStrings().size();
        constexpr std::size_t size = [] {
            std::size_t size = 0;
            for (auto str : collectStrings())
                size += str.size() + 1;
            return size;
        }();

        Qt26Private::MocStringData<count, size> result;
        std::size_t offset = 0;
        std::size_t index = 0;
        for (auto str : collectStrings()) {
            for (char ch : str)
                result.strings[offset++] = ch;
            result.strings[offset++] = '\0';
            result.lengths[index++] = uint(str.size());
        }
        return result;
    }

    static consteval uint stridx(std::string_view str)
    {
        static constexpr auto strings = mocStrings();
        return strings.indexOf(str);
    }

    // Like moc's Generator::generateTypeInfo
    static consteval uint typeInfo(std::meta::info type)
    {
        if (const int id = builtinTypeId(type); id != QMetaType::UnknownType)
            return uint(id);
        return QtMocConstants::IsUnresolvedType | stridx(typeName(type));
    }

    // Methods

    static consteval uint methodFlags(Qt26Private::MethodInfo info)
    {
        using namespace QtMocConstants;
        uint flags = is_public(info.member) ? AccessPublic
                   : is_protected(info.member) ? AccessProtected
                   : AccessPrivate;
        if (info.cloned)
            flags |= MethodCloned;
        return flags;
    }

    // QtMocHelpers::SignalData/SlotData/MethodData<Ret(Args...) [const]>
    static consteval std::meta::info methodDataType(Qt26Private::MethodInfo info)
    {
        std::vector<std::meta::info> signature = { returnType(info.member) };
        for (auto parameter : parametersOf(info))
            signature.push_back(type_of(parameter));
        const auto function = dealias(substitute(is_const(type_of(info.member))
                                                 ? ^^Qt26Private::ConstFunctionType
                                                 : ^^Qt26Private::FunctionType, signature));
        switch (info.type) {
        case Qt26::MetaEntity::Signal:
            return substitute(^^QtMocHelpers::SignalData, {function});
        case Qt26::MetaEntity::Slot:
            return substitute(^^QtMocHelpers::SlotData, {function});
        case Qt26::MetaEntity::Method:
            return substitute(^^QtMocHelpers::MethodData, {function});
        default:
            break;
        }
        throw std::logic_error("Unsupported method type");
    }

    template <std::size_t I>
    static constexpr auto methodData()
    {
        constexpr Qt26Private::MethodInfo info = methodInfos()[I];
        using Data = [:methodDataType(info):];

        typename Data::ParametersArray parameters = {};
        std::size_t index = 0;
        template for (constexpr auto parameter : parametersOf(info)) {
            parameters[index++] = { typeInfo(type_of(parameter)), stridx(parameterName(parameter)) };
        }
        return Data(stridx(identifier_of(info.member)), stridx(""), methodFlags(info),
                    typeInfo(returnType(info.member)), parameters);
    }

    template <std::size_t ...I>
    static constexpr auto methodsData(std::index_sequence<I...>)
    {
        return QtMocHelpers::UintData{ methodData<I>()... };
    }

    // Properties

    static consteval uint propertyTypeInfo(const QStaticMetaProperty &property)
    {
        if (const int id = builtinTypeId(propertyType(property)); id != QMetaType::UnknownType)
            return uint(id);
        return QtMocConstants::IsUnresolvedType | stridx(property.m_type);
    }

    // Like moc's Generator::addProperties
    static consteval uint propertyFlags(const QStaticMetaProperty &property)
    {
        using namespace QtMocConstants;
        const bool readable = !property.m_getter.empty() || !property.m_member.empty();
        const bool writable = (!property.m_member.empty() && !property.m_constant)
                           || !property.m_setter.empty();
        const bool designable = property.m_designable != "false";
        const bool scriptable = property.m_scriptable != "false";
        const bool stored = property.m_stored != "false";

        uint flags = Invalid;
        if (readable && designable && scriptable && stored) {
            flags |= DefaultPropertyFlags;
        } else {
            if (readable)
                flags |= Readable;
            if (designable)
                flags |= Designable;
            if (scriptable)
                flags |= Scriptable;
            if (stored)
                flags |= Stored;
        }
        if (writable)
            flags |= Writable;
        if (!property.m_reset.empty())
            flags |= Resettable;
        if (!isBuiltinType(propertyType(property)))
            flags |= EnumOrFlag;
        if (const auto name = property.m_name; !name.empty()) {
            std::string setter = "set";
            setter += (name[0] >= 'a' && name[0] <= 'z') ? char(name[0] - 'a' + 'A') : name[0];
            setter += name.substr(1);
            if (setter == property.m_setter)
                flags |= StdCppSet;
        }
        if (property.m_constant)
            flags |= Constant;
        if (property.m_final)
            flags |= Final;
        if (property.m_virtual)
            flags |= Virtual;
        if (property.m_override)
            flags |= Override;
        if (property.m_user != "false")
            flags |= User;
        if (property.m_required)
            flags |= Required;
        if (!property.m_bindable.empty())
            flags |= Bindable;
        return flags;
    }

    static consteval uint notifyId(const QStaticMetaProperty &property)
    {
        if (property.m_notify.empty())
            return uint(-1);
        if (const int index = notifySignalIndex(property.m_notify); index != -1)
            return uint(index);
        // signal is in a parent class
        return QtMocConstants::IsUnresolvedSignal | stridx(property.m_notify);
    }

    template <std::size_t I>
    static constexpr auto propertyData()
    {
        using Type = [:propertyType(directProperties()[I]):];
        return QtMocHelpers::PropertyData<Type>(stridx(directProperties()[I].m_name),
                                                propertyTypeInfo(directProperties()[I]),
                                                propertyFlags(directProperties()[I]),
                                                notifyId(directProperties()[I]),
                                                uint(directProperties()[I].m_revision));
    }

    template <std::size_t ...I>
    static constexpr auto propertiesData(std::index_sequence<I...>)
    {
        return QtMocHelpers::UintData{ propertyData<I>()... };
    }

    // Enums

    template <std::size_t I>
    static constexpr auto enumData()
    {
        constexpr auto enumType = enumTypes()[I];
        using Enum = [:enumType:];
        using Data = QtMocHelpers::EnumData<Enum>;
        constexpr uint nameIndex = stridx(identifier_of(enumType));
        constexpr uint flags = is_scoped_enum_type(dealias(enumType)) ? QtMocConstants::EnumIsScoped
                                                                      : 0;
        const Data data(nameIndex, nameIndex, flags);

        static constexpr auto enumerators = std::define_static_array(enumerators_of(dealias(enumType)));
        if constexpr (enumerators.empty()) {
            return data;
        } else {
            typename Data::EnumEntry entries[enumerators.size()] = {};
            std::size_t index = 0;
            template for (constexpr auto enumerator : enumerators) {
                entries[index++] = { int(stridx(identifier_of(enumerator))), [:enumerator:] };
            }
            return data.add(entries);
        }
    }

    template <std::size_t ...I>
    static constexpr auto enumsData(std::index_sequence<I...>)
    {
        return QtMocHelpers::UintData{ enumData<I>()... };
    }

    // Class infos

    static consteval auto classInfoData()
    {
        constexpr std::size_t count = directClassInfos().size();
        if constexpr (count == 0) {
            return QtMocHelpers::detail::UintDataBlock<0, 0>{};
        } else {
            std::array<uint, 2> infos[count] = {};
            std::size_t index = 0;
            for (const auto &classInfo : directClassInfos())
                infos[index++] = { stridx(classInfo.m_name), stridx(classInfo.m_value) };
            return QtMocHelpers::ClassInfos<int(count)>(infos);
        }
    }

    // Like moc, the superclass of the QMetaObject is the first base class
    static constexpr QMetaObject::SuperData superData()
    {
        constexpr auto bases = std::define_static_array(bases_of(^^Class, accessContext()));
        if constexpr (!bases.empty()) {
            using Base = [:type_of(bases.front()):];
            if constexpr (requires { Base::staticMetaObject; })
                return QMetaObject::SuperData::link<Base::staticMetaObject>();
        }
        return nullptr;
    }

public:
    // The equivalent of the moc-generated qt_create_metaobjectdata<Tag>()
    template <typename Tag, uint Flags = QtMocConstants::MetaObjectFlag{}>
    static constexpr auto createMetaObjectData()
    {
        return QtMocHelpers::metaObjectData<Class, Tag>(Flags, mocStrings(),
                    methodsData(std::make_index_sequence<methodInfos().size()>{}),
                    propertiesData(std::make_index_sequence<directProperties().size()>{}),
                    enumsData(std::make_index_sequence<enumTypes().size()>{}),
                    QtMocHelpers::UintData<>{}, // constructors
                    classInfoData());
    }

    template <typename Tag>
    static constexpr QMetaObject::Data finalize()
    {
        return QMetaObject::Data{
            superData(),
            Class::template qt_staticMetaObjectStaticContent<Tag>.stringdata,
            Class::template qt_staticMetaObjectStaticContent<Tag>.data,
            nullptr, // static_metacall; Q26 objects are not QObjects
            nullptr, // relatedMetaObjects
            Class::template qt_staticMetaObjectRelocatingContent<Tag>.metaTypes,
            nullptr  // extradata
        };
    }
};

template <typename Class>
constexpr auto QStaticMetaObject::fromType()
{
    return QStaticMetaObjectImpl<Class>{};
}

}

#define Q26_OBJECT_STATICS \
    static Qt26::QStaticMetaObject staticQ26MetaObject; \
    static QMetaObject staticMetaObject; \
    struct QStaticMetaObjectData; \
    QT_META_OBJECT_VARS

#define Q26_OBJECT \
public: \
    const Qt26::QStaticMetaObject *q26MetaObject() const override { return &staticQ26MetaObject; } \
    const QMetaObject *metaObject() const override { return &staticMetaObject; } \
    int qt_metacall(QMetaObject::Call, int, void **) override; \
    Q26_OBJECT_STATICS \
private:

// Expands to the same specialization of qt_create_metaobjectdata as moc generates,
// and initializes the QMetaObject from the qt_staticMetaObject*Content variables
// declared by QT_META_OBJECT_VARS.
#define Q26_METAOBJECT_IMPL(Class, Flags) \
    template <> constexpr inline auto Class::qt_create_metaobjectdata<Qt26Private::MetaObjectTag<Class>>() \
    { \
        return Qt26::QStaticMetaObjectImpl<Class>::createMetaObjectData< \
            Qt26Private::MetaObjectTag<Class>, Flags>(); \
    } \
    QMetaObject Class::staticMetaObject = { \
        Qt26::QStaticMetaObjectImpl<Class>::finalize<Qt26Private::MetaObjectTag<Class>>() \
    };

#define Q26_OBJECT_IMPL(Class) \
    Qt26::QStaticMetaObject Class::staticQ26MetaObject = Qt26::QStaticMetaObject::fromType<Class>(); \
    Q26_METAOBJECT_IMPL(Class, QtMocConstants::MetaObjectFlag{}) \
    int Class::qt_metacall(QMetaObject::Call call, int what, void **args) \
    { return Qt26::QStaticMetaObjectImpl<Class>::static_metacall(this, call, what, args); }

#define Q26_GADGET \
public: \
    Q26_OBJECT_STATICS \
private:

#define Q26_OBJECT_INLINE \
public: \
    struct QStaticMetaObjectData; \
    using Class = [:parent_of(^^QStaticMetaObjectData):]; \
    static inline Qt26::QStaticMetaObject staticQ26MetaObject = Qt26::QStaticMetaObject::fromType<Class>(); \
    const Qt26::QStaticMetaObject *q26MetaObject() const override { return &staticQ26MetaObject; } \
    int qt_metacall(QMetaObject::Call call, int what, void **args) override \
    { return Qt26::QStaticMetaObjectImpl<Class>::static_metacall(this, call, what, args); } \

#define Q26_GADGET_IMPL(Class) \
    Qt26::QStaticMetaObject Class::staticQ26MetaObject = Qt26::QStaticMetaObject::fromType<Class>(); \
    Q26_METAOBJECT_IMPL(Class, QtMocConstants::PropertyAccessInStaticMetaCall)

#define Q26_PROPERTY(Declaration) \
public: \
    struct [[=Qt26::Property<[]{return #Declaration ;}>{}]] QStaticMetaObjectData; \
private:
#define Q26_ENUM(Name) \
    struct [[=Qt26::Enumerator<Name>{}]] QStaticMetaObjectData;
#define Q26_CLASSINFO(Name, Value) \
public: \
    struct [[=Qt26::ClassInfo<[]{return Name;}, []{return Value;}>{}]] QStaticMetaObjectData; \
private:

#define Q26_SIGNAL [[=Qt26::MetaEntity::Signal]]

#define Q26_SLOT [[=Qt26::MetaEntity::Slot]]

#define Q26_INVOKABLE [[=Qt26::MetaEntity::Method]]

#define Q26_SIGNAL_INLINE_IMPL {                                    \
    constexpr auto qt_signal = std::meta::current_function();         \
    using That = [:parent_of(qt_signal):];                          \
    using ThatMO = Qt26::QStaticMetaObjectImpl<That>;               \
    constexpr auto index = ThatMO::indexOfMemberWithType<qt_signal, Qt26::MetaEntity::Signal>(); \
    using ReturnType = [:return_type_of(qt_signal):];               \
    constexpr auto pCount = parameters_of(qt_signal).size();        \
                                                                    \
    QStaticMetaMethod::SignalArgs<pCount, ReturnType> args;         \
    std::size_t i = 0;                                              \
    template for (constexpr auto e : std::define_static_array(parameters_of(qt_signal))) { \
        args.m_args[++i] = &[:variable_of(e):];                     \
    }                                                               \
    ThatMO::activate(this, index, args.m_args.data());              \
    return args.result();                                           \
}

#define Q26_SIGNAL_IMPL(Signature) \
    Signature Q26_SIGNAL_INLINE_IMPL
