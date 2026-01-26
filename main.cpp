#include <QtCore>

#include "types.h"

int main()
{
    UserWithNumber user;

    qDebug() << "Baseclass";
    qDebug() << *user.Object::metaObject();

    Object *userAsObject = &user;
    auto *metaObject = userAsObject->metaObject();

    qDebug() << "Inspect polymorphic";
    qDebug() << metaObject->className() << metaObject->bases();
    qDebug() << "ClassInfo count" << metaObject->classInfoCount();
    for (int i = 0; i < metaObject->classInfoCount(); ++i) {
        qDebug().nospace() << "- " << i << ": " << metaObject->classInfo(i);
    }

    qDebug() << "Enumerator count" << metaObject->enumeratorCount();
    for (int i = 0; i < metaObject->enumeratorCount(); ++i) {
        qDebug().nospace() << "- " << i << ": " << metaObject->enumerator(i);
    }

    qDebug() << "Property count" << metaObject->propertyOffset() << "+"
                                 << metaObject->propertyCount() - metaObject->propertyOffset();
    for (int i = 0 /*metaObject->propertyOffset()*/; i < metaObject->propertyCount(); ++i) {
        const auto property = metaObject->property(i);
        qDebug().nospace() << "- " << i << ": " << property << " -> " << user.property(property.m_name);
    }
//    qDebug() << "Property index of 'value' =" << metaObject->indexOfProperty("value");

    qDebug() << "Methods";
    for (int i = 0; i < metaObject->methodCount(); ++i) {
        const auto method = metaObject->method(i);
        qDebug().nospace() << "- " << i << ": " << method;
    }

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

    constexpr auto object = Qt26::QStaticMetaObject::fromType<User>();
    { // this works if we get the metaObject statically
        constexpr auto propertyCount = object.propertyCount();
        constexpr auto firstProperty = object.property(0);
        constexpr auto signalCount = object.signalCount();
    }
    qDebug() << "Inspect statically";
    qDebug() << object;

    for (int i = 0; i < object.propertyCount(); ++i) {
        qDebug().nospace() << "- " << i << ": " << object.property(i);
    }
    qDebug() << Qt::endl;

    qDebug() << "Methods" << object.methods();
    qDebug() << "Signal count" << object.signalCount();

    return 0;
}
