#pragma once

#include "object.h"

template <typename T>
struct Generic : Object
{
    Q26_OBJECT_INLINE

    Q26_SIGNAL void valueChanged()
    {
        // Q_UNUSED(newValue);
    }
};
