#pragma once

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

    template <typename Fn, std::size_t ...Is>
    static constexpr void expand_while(Fn &&fn, std::index_sequence<Is...>)
    {
        ((std::forward<Fn>(fn)(std::integral_constant<size_t, Is>{}), true) && ...);
    }

    template <std::size_t S, typename Fn>
    static constexpr void expand_while(Fn &&fn)
    {
        expand_while(std::forward<Fn>(fn), std::make_index_sequence<S>{});
    }

    namespace __impl {
    template<auto... vals>
    struct replicator_type {
        template<typename F>
        constexpr void operator>>(F body) const {
            (body.template operator()<vals>(), ...);
        }
    };

    template<auto... vals>
    replicator_type<vals...> replicator = {};
    }

    template<typename R>
    consteval auto expand(R range) {
    std::vector<std::meta::info> args;
    for (auto r : range)
        args.push_back(reflect_constant(r));
    return substitute(^^__impl::replicator, args);
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
        constexpr auto count = members_of(^^Class, std::meta::access_context::current()).size();
        Qt26Private::expand_while<count>([&](auto I){
            constexpr auto member = members_of(^^Class, std::meta::access_context::current()).at(I);
            if constexpr (is_function(member) && has_identifier(member)
                       && !is_function_template(member) && !is_special_member_function(member)) {
                constexpr auto mfp = &[:member:];
                if (isSameFunction(mfp, signal)) {
                    result = identifier_of(member);
                    return false;
                }
            }
            return true;
        });
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
    consteval QStaticMetaProperty(std::string_view description = {})
        : QStaticMetaData(Qt26::MetaEntity::Property)
    {
        std::size_t i = 0;
        std::size_t sectionEnd = description.npos;
        std::size_t section = 0;
        do {
            sectionEnd = description.find(' ', i);
            auto text = description.substr(i, sectionEnd - i);
            switch (section) {
            case 0:
                m_type = text;
                break;
            case 1:
                m_name = text;
                break;
            case 2:
                if (text != "READ")
                    throw std::invalid_argument(text.data());
                break;
            case 3:
                m_getter = text;
                break;
            case 4:
                if (text != "WRITE")
                    throw std::invalid_argument(text.data());
            case 5:
                m_setter = text;
                break;
            case 6:
                if (text != "NOTIFY")
                    throw std::invalid_argument(text.data());
            case 7:
                m_notify = text;
                break;
            }
            ++section;
            i = sectionEnd + 1;
        } while (sectionEnd != description.npos);
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

        [:Qt26Private::expand(enumerators_of(^^Enum)):] >> [&result]<auto item>{
            result.emplace_back(
                identifier_of(item),
                static_cast<long>(extract<Enum>(constant_of(item)))
            );
        };

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

    static consteval std::optional<std::meta::info> memberFunction(std::string_view name)
    {
        for (auto member : members_of(^^Class, std::meta::access_context::current())) {
            if (has_identifier(member) && is_function(member) && identifier_of(member) == name) {
                return member;
            }
        }
        return std::nullopt;
    }

    static consteval std::size_t annotationCount()
    {
        using MetaObject = Class::QStaticMetaObjectData;
        return annotations_of(^^MetaObject).size();
    }

    static consteval auto metaObjectAnnotation(auto idx)
    {
        using StaticMetaObject = Class::QStaticMetaObjectData;
        constexpr auto info = annotations_of(^^StaticMetaObject).at(idx);
        auto result = annotationData<info>();
        result.m_index = idx;
        return result;
    }

public:
    static consteval void getUniqueBases(std::vector<std::meta::info> &bases)
    {
        [:Qt26Private::expand(bases_of(^^Class, std::meta::access_context::current())):]
        >> [&]<auto base> {
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
        };
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
        [:Qt26Private::expand(uniqueBases()):]
        >> [&]<auto base> {
            using Base = [:base:];
            using BaseMO = Qt26::QStaticMetaObjectImpl<Base>;
            cont = cont && (std::forward<Fn>(fn).template operator()<BaseMO>(), true);
        };
        return cont;
    }

public:
    static constexpr std::string_view className()
    {
        return identifier_of(^^Class);
    }

    static constexpr std::vector<std::string_view> bases()
    {
        std::vector<std::string_view> result;
        [:Qt26Private::expand(bases_of(^^Class, std::meta::access_context::current())):]
        >> [&result]<auto base>{
            using Base = [:type_of(base):];
            using BaseMO = Qt26::QStaticMetaObjectImpl<Base>;
            result.push_back(BaseMO::className());
        };

        return result;
    }

    static constexpr std::vector<QStaticMetaProperty> directProperties()
    {
        std::vector<QStaticMetaProperty> result;
        Qt26Private::expand_while<annotationCount()>([&](auto idx){
            constexpr auto data = metaObjectAnnotation(idx);
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::Property)
                result.push_back(data);
        });
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
        Qt26Private::expand_while<annotationCount()>([&result](auto idx) {
            constexpr auto data = metaObjectAnnotation(idx);
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::Enumerator)
                result.push_back(data);
        });
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
        Qt26Private::expand_while<annotationCount()>([&result](auto idx) {
            constexpr auto data = metaObjectAnnotation(idx);
            if constexpr (data.m_metaEntity == Qt26::MetaEntity::ClassInfo)
                result.push_back(data);
        });
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
        int i = 0;
        [:Qt26Private::expand(members_of(^^Class, std::meta::access_context::current())):] >>
            [&result, &i]<auto member> {
            if constexpr (is_function(member) && has_identifier(member)) {
                if constexpr (annotations_of_with_type(member, ^^Qt26::MetaEntity).size() == 1) {
                    constexpr Qt26::MetaEntity methodType = extract<Qt26::MetaEntity>(
                        annotations_of_with_type(member, ^^Qt26::MetaEntity).front()
                    );
                    if constexpr (methodType != Qt26::MetaEntity::Unknown) {
                        constexpr auto return_type = return_type_of(member);
                        static_assert(return_type == ^^void,
                                      "Meta-methods with return type are not supported yet!");
                        QStaticMetaMethod method(methodType, identifier_of(member));
                        method.m_index = i;
                        [:Qt26Private::expand(parameters_of(member)):] >> [&method]<auto parameter> {
                            using type = [:type_of(parameter):];
                            method.m_parameters.push_back({QMetaType::fromType<std::remove_cvref_t<type>>(),
                                                           identifier_of(parameter)});
                        };
                        result.push_back(method);
                    }
                }
            }
            ++i;
            return false;
        };
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
        int i = 0;
        for (const auto &method : methods()) {
            if (method.m_name == name)
                return i;
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
                                constexpr auto parameter = parameters_of(*member).at(0);
                                using paramType = std::remove_cvref_t<decltype([:parameter:])>;
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
                Qt26Private::expand_at<members_of(^^Class, std::meta::access_context::current()).size()>(method.m_index, [&](auto Idx){
                    constexpr auto member = members_of(^^Class, std::meta::access_context::current()).at(Idx);
                    if constexpr (is_function(member)
                               && annotations_of_with_type(member, ^^Qt26::MetaEntity).size() == 1) {
                        using ArgTuple = [:Qt26Private::tuple_type(parameters_of(member)):];
                        auto callargs = Qt26Private::make_tuple<ArgTuple>(args);
                        std::apply([object](auto &&...a) {
                            object->[:member:](a...);
                        }, callargs);
                    }
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
    struct Sizes
    {
        std::size_t strings = 0;
        std::size_t data = 16;
        std::size_t relatedMetaObjects = 0;
        std::size_t metaTypes = 0;
        std::size_t extradata = 0;
    };

    static constexpr Sizes sizes()
    {
        Sizes sizes;
        sizes.strings += className().size() + 1;
        for (auto base : bases())
            sizes.strings += base.size() + 1;
        for (auto property : properties())
            sizes.strings += property.size();
        for (auto enumerator : enumerators())
            sizes.strings += enumerator.size();
        for (auto classInfo : classInfos())
            sizes.strings += classInfo.size();
        for (auto method : methods())
            sizes.strings += method.size();
        return sizes;
    }

    template <typename Array>
    static constexpr std::size_t addInt(Array &array, std::size_t index, uint value)
    {
        array[index] = value;
        return index + 1;
    };

    template <typename Array>
    static constexpr std::size_t addString(Array &array, std::size_t index, std::string_view str)
    {
        // start of string = two ints past head
        index = addInt(array, index, index + 2*sizeof(int));
        index = addInt(array, index, str.size());

        // string with constexpr compliant 4*char -> uint conversion
        char offset = 0;
        for (const uint ch : str) {
            array[index] |= (ch << CHAR_BIT * offset);
            if (++offset == sizeof(int)) {
                offset = 0;
                ++index;
            }
        }
        if (offset)
            ++index;
        return index;
    };

    static constexpr Sizes dataSizes = sizes();

    static constexpr auto makeStringData() {
        std::array<uint, dataSizes.strings> stringData = {};
        std::size_t index = 0;
        index = addString(stringData, index, className());
        return stringData;
    }
    static constexpr auto stringData = makeStringData();
    static constexpr std::array<uint, dataSizes.data> intData {
        QtMocConstants::OutputRevision,
        0, // className
        0, // classInfoCount
        0, // classInfoData
        0, // methodCount
        0, // methodData
        0, // propertyCount
        0, // propertyData
        0, // enumeratorCount
        0, // enumeratorData
        0, // constructorCount
        0, // constructorData
        0, // flags
        0, // signalCount
        0, // metaObjectHashIndex
    };

public:
    static constexpr auto finalize()
    {
        // Rest is left as an exercise for the reader.
        // This is PoC that we can construct the data at compile time,
        // and only have to assemble the QMetaObject::Data object at initialization
        // time.

        return QMetaObject::Data{
            {}, // Super
            stringData.data(),
            intData.data()
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
    struct QStaticMetaObjectData;

#define Q26_OBJECT \
public: \
    const Qt26::QStaticMetaObject *q26MetaObject() const override { return &staticQ26MetaObject; } \
    const QMetaObject *metaObject() const override { return &staticMetaObject; } \
    int qt_metacall(QMetaObject::Call, int, void **) override; \
    Q26_OBJECT_STATICS \
    QT_META_OBJECT_VARS \
private:

#define Q26_OBJECT_IMPL(Class) \
    Qt26::QStaticMetaObject Class::staticQ26MetaObject = Qt26::QStaticMetaObject::fromType<Class>(); \
    QMetaObject Class::staticMetaObject = { Qt26::QStaticMetaObjectImpl<Class>::finalize() }; \
    int Class::qt_metacall(QMetaObject::Call call, int what, void **args) \
    { return Qt26::QStaticMetaObjectImpl<Class>::static_metacall(this, call, what, args); }

#define Q26_GADGET \
public: \
    Q26_OBJECT_STATICS \
    QT_META_OBJECT_VARS \
private:

#define Q26_GADGET_IMPL(Class) \
    Qt26::QStaticMetaObject Class::staticQ26MetaObject = Qt26::QStaticMetaObject::fromType<Class>(); \
    QMetaObject Class::staticMetaObject = { Qt26::QStaticMetaObjectImpl<Class>::finalize() };

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

#define Q26_INVOKABLE [[=Qt26::MetaEntity::Method]]

#define Q26_SIGNAL_INLINE_IMPL {                                    \
    using That = std::remove_pointer_t<decltype(this)>;             \
    using ThatMO = Qt26::QStaticMetaObjectImpl<That>;               \
    constexpr std::string_view signal_name = __FUNCTION__;          \
    constexpr auto index = ThatMO::indexOfMethod(signal_name);      \
    constexpr auto member = *ThatMO::memberFunction(signal_name);   \
    using ReturnType = [:return_type_of(member):];                  \
    constexpr auto pCount = parameters_of(member).size();           \
                                                                    \
    QStaticMetaMethod::SignalArgs<pCount, ReturnType> args;         \
    [:Qt26Private::expand(parameters_of(member)):] >>               \
    [&, i = 0]<auto param>() mutable {                              \
        args.m_args[++i] = &[:param:];                              \
    };                                                              \
    ThatMO::activate(this, index, args.m_args.data());              \
    return args.result();                                           \
}

#define Q26_SIGNAL_IMPL(Signature) \
    Signature Q26_SIGNAL_INLINE_IMPL
