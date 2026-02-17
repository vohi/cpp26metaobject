#pragma once

#include "metaobject.h"

class Object
{
public:
    Q26_OBJECT_STATICS
    Q26_PROPERTY(QString objectName READ objectName WRITE objectName)

public:
    Object() = default;
    virtual ~Object() = default;

    virtual const Qt26::QStaticMetaObject *q26MetaObject() const;
    virtual const QMetaObject *metaObject() const;
    virtual int qt_metacall(QMetaObject::Call, int, void **);

    QString objectName() const
    {
        return m_objectName;
    }
    void setObjectName(const QString &objectName)
    {
        m_objectName = objectName;
    }

    QVariant property(std::string_view name) const;
    bool setProperty(std::string_view name, const QVariant &value);

    template <typename Signal, typename Slot>
    void connect(Signal &&signal, Slot &&slot)
    {
        using Signature = Qt26::Prototype<std::remove_cvref_t<Signal>>::type;
        std::string_view name = Qt26::signalName(std::forward<Signal>(signal));

        connectImpl(name, QtPrivate::SlotObjUniquePtr
            (QtPrivate::makeCallableObject<Signature>(std::forward<Slot>(slot)))
        );
    }

    std::unordered_map<std::string_view, std::vector<QtPrivate::SlotObjUniquePtr>> connections;

    Q26_SIGNAL void destroyed();

    template <typename ...Args>
    auto invokeMethod(std::string_view name, Args &&...args)
    {
        const int index = q26MetaObject()->indexOfMethod(name);
        void *a[] = {
            nullptr,
            const_cast<void *>(reinterpret_cast<const volatile void *>(std::addressof(args)))...
        };
        qt_metacall(QMetaObject::InvokeMetaMethod, index, a);
    }

private:
    QString m_objectName;

    Q_DISABLE_COPY_MOVE(Object);
    void connectImpl(std::string_view, QtPrivate::SlotObjUniquePtr slotObject);
};

