#include "types.h"

#include <QtCore/qobjectdefs.h>
#include <QtCore/qvariant.h>

Q26_OBJECT_IMPL(Object)
Q26_SIGNAL_IMPL(void Object::destroyed())

const Qt26::QStaticMetaObject *Object::metaObject() const
{
    return &staticMetaObject;
}

QVariant Object::property(std::string_view name) const
{
    std::size_t index = metaObject()->indexOfProperty(name);
    if (index < 0)
        return false;

    QVariant result;
    void *args[] = {&result};
    metaObject()->meta_call(Qt26::QStaticMetaObject::Call::GetProperty, index,
                            const_cast<Object *>(this), args);
    return result;
}

bool Object::setProperty(std::string_view name, const QVariant &value)
{
    std::size_t index = metaObject()->indexOfProperty(name);
    if (index < 0)
        return false;
    const void *args[] = { nullptr, &value };
    return metaObject()->meta_call(Qt26::QStaticMetaObject::Call::SetProperty, index, this, const_cast<void **>(args)) >= 0;
}

void Object::connectImpl(std::string_view name, QtPrivate::SlotObjUniquePtr slotObject)
{
    connections[name].push_back(std::move(slotObject));
}
