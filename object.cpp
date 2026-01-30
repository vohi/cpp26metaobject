#include "types.h"

#include <QtCore/qobjectdefs.h>
#include <QtCore/qvariant.h>

Q26_OBJECT_IMPL(Object)
Q26_SIGNAL_IMPL(void Object::destroyed())

const Qt26::QStaticMetaObject *Object::q26MetaObject() const
{
    return &staticQ26MetaObject;
}

const QMetaObject *Object::metaObject() const
{
    return &staticMetaObject;
}

int Object::qt_metacall(QMetaObject::Call call, int what, void **args)
{
    return what;
}

QVariant Object::property(std::string_view name) const
{
    std::size_t index = q26MetaObject()->indexOfProperty(name);
    if (index < 0)
        return false;

    QVariant result;
    void *args[] = {&result};
    const_cast<Object *>(this)->qt_metacall(QMetaObject::ReadProperty, index, args);
    return result;
}

bool Object::setProperty(std::string_view name, const QVariant &value)
{
    std::size_t index = q26MetaObject()->indexOfProperty(name);
    if (index < 0)
        return false;
    const void *args[] = { nullptr, &value };
    return qt_metacall(QMetaObject::WriteProperty, index, const_cast<void **>(args)) >= 0;
}

void Object::connectImpl(std::string_view name, QtPrivate::SlotObjUniquePtr slotObject)
{
    connections[name].push_back(std::move(slotObject));
}
