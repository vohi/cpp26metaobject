#pragma once

#include "object.h"

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

    Q26_INVOKABLE void callMe()
    {
        qDebug()<< "Thanks for nothing";
    }

    Q26_INVOKABLE void callMeWithNumber(int number)
    {
        qDebug()<< "Thanks for the money" << number;
    }

    Q26_INVOKABLE void callMeWithText(const QString &text)
    {
        qDebug()<< "Thanks for the message" << text;
    }

    Q26_INVOKABLE void callMeWithTextRef(QString &text)
    {
        qDebug()<< "Updating message" << text;
        text = "Screwed";
    }

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
