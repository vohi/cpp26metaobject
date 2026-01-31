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

private:
    QString m_objectName;

    Q_DISABLE_COPY_MOVE(Object);
    void connectImpl(std::string_view, QtPrivate::SlotObjUniquePtr slotObject);
};

struct User : virtual public Object
{
    Q26_OBJECT
    Q26_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q26_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

    Q26_CLASSINFO("DefaultProperty", "name")
public:
    enum class Modes {
        Default,
        Value1,
        Value2 = -99,
    };
    Q26_ENUM(Modes)

    QString name() const
    {
        qDebug() << "name getter called" << m_name;
        return m_name;
    }

    void setName(const QString &name)
    {
        qDebug() << "name setter called";
        if (m_name == name)
            return;
        m_name = name;
        emit nameChanged();
    }

    Q26_SIGNAL void nameChanged();

    int value() const
    {
        qDebug() << "value getter called";
        return m_value;
    }

    void setValue(int value)
    {
        qDebug() << "value setter called";
        if (m_value == value)
            return;
        m_value = value;
        emit valueChanged();
    }

    Q26_SIGNAL void valueChanged();

    Q26_SIGNAL void signalWithArgs(int x, double y);

public:
    QString m_name = "default";
    int m_value = 42;
};

struct UserNumberAttribute : virtual Object
{
    Q26_OBJECT
    Q26_PROPERTY(double number READ number WRITE setNumber NOTIFY numberChanged)
public:
    UserNumberAttribute() = default;

    double number() const { return m_number; }
    void setNumber(double number)
    {
        if (m_number == number)
            return;
        m_number = number;
        emit numberChanged();
    }

    Q26_SIGNAL void numberChanged();

private:
    double m_number = 3.1415;
};


struct UserWithNumber : User, UserNumberAttribute
{
    Q26_OBJECT
public:
    UserWithNumber() = default;
};
