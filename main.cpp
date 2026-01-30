#include <QtCore>

#include <QtCore/private/qmetaobject_p.h>
#include "types.h"

QDebug operator<<(QDebug dbg, const QMetaProperty &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();
    dbg << "QMetaProperty(" << that.typeName() << " " << that.name() << ")";
    return dbg;
}
QDebug operator<<(QDebug dbg, const QMetaEnum &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();
    dbg << "QMetaEnum(" << that.name() << "[";
    for (int i = 0; i < that.keyCount(); ++i) {
        dbg << that.key(i) << " = " << that.value(i);
        if (i < that.keyCount() - 1)
            dbg << ", ";
    }
    dbg << "])";
    return dbg;
}

QDebug operator<<(QDebug dbg, const QMetaMethod &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();
    return dbg;
}

QDebug operator<<(QDebug dbg, const QMetaClassInfo &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote();
    return dbg;
}

QDebug operator<<(QDebug dbg, const QMetaObject &that)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace().noquote() << "class " << that.className();
    if (const auto *super = that.superClass())
        dbg << " : " << super->className();
    dbg << Qt::endl;

    dbg << "ClassInfos:" << that.classInfoOffset() << " + "
                         << that.classInfoCount() - that.classInfoOffset() << Qt::endl;
    for (int i = 0; i < that.classInfoCount(); ++i)
        dbg << "- " << i << ": " << that.classInfo(i) << Qt::endl;

    dbg << "Properties:" << that.propertyOffset() << " + "
                         << that.propertyCount() - that.propertyOffset() << Qt::endl;
    for (int i = 0; i < that.propertyCount(); ++i)
        dbg << "- " << i << ": " << that.property(i) << Qt::endl;

    dbg << "Enumerators:" << that.enumeratorOffset() << " + "
                          << that.enumeratorCount() - that.enumeratorOffset() << Qt::endl;
    for (int i = 0; i < that.enumeratorCount(); ++i)
        dbg << "- " << i << ": " << that.enumerator(i) << Qt::endl;

    dbg << "Methods:" << that.methodOffset() << " + "
                              << that.methodCount() - that.methodOffset() << Qt::endl;
    for (int i = 0; i < that.methodCount(); ++i)
        dbg << "- " << i << ": " << that.method(i) << Qt::endl;

    return dbg;
}

template <typename MetaObject>
void print(const MetaObject &mo)
{
    const auto moTypeName = QMetaType::fromType<MetaObject>().name();
    qDebug() << Qt::endl << moTypeName << "for" << mo;
}

int main()
{
    UserWithNumber user;
    Object *userAsObject = &user;
    auto *q26MetaObject = userAsObject->q26MetaObject();

    print (*user.Object::q26MetaObject());

    print(*q26MetaObject);
    print(*userAsObject->metaObject());
    print(Qt26::QStaticMetaObject::fromType<decltype(user)>());

    qDebug() << Qt::endl << "Signal test";
    user.connect(&User::nameChanged, [&user]{
        qDebug() << "nameChanged to" << user.name();
    });
    user.connect(&User::valueChanged, [&user]{
        qDebug() << "valueChanged to" << user.value();
    });
    user.connect(&User::signalWithArgs, [](int x, double y){
        qDebug() << "Slot with args" << x << y;
    });

    qDebug() << "user.value = " << user.property("value");
    user.setValue(26);

    qDebug() << "user.name = " << user.property("name");
    user.setProperty("name", QString("new value for name"));
    qDebug() << "user.name = " << user.property("name");
    qDebug();

    user.signalWithArgs(42, 3.14159);

    { // this works if we get the q26MetaObject statically
        constexpr auto staticMetaObject = Qt26::QStaticMetaObject::fromType<decltype(user)>();
        constexpr auto propertyCount = staticMetaObject.propertyCount();
        constexpr auto firstProperty = staticMetaObject.property(0);
        constexpr auto signalCount = staticMetaObject.signalCount();
    }

    return 0;
}
