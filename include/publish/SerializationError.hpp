#pragma once
#include <stdexcept>

class SerializationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


