#include "variant.h"

#include <stdexcept>

Variant::Variant()
    : type(Type::NONE)
{}

Variant::Variant(const std::string& val)
    : s(val)
    , type(Type::STRING)
{}

Variant::Variant(const bool val)
    : b(val)
    , type(Type::BOOL)
{}

Variant::Variant(const double val)
    : d(val)
    , type(Type::DOUBLE)
{}

Variant::Variant(const long long int val)
    : l(val)
    , type(Type::LONG)
{}

Variant::Variant(const unsigned long long int val)
    : u(val)
    , type(Type::UNSIGNED)
{}

Variant::Variant(const std::vector<long long int>& val)
    : vl(val)
    , type(Type::VECTOR_LONG)
{}

Variant::Variant(const std::vector<unsigned long long int>& val)
    : vu(val)
    , type(Type::VECTOR_UNSIGNED)
{}

Variant::Variant(const std::vector<double> &val)
    : vd(val)
    , type(Type::VECTOR_DOUBLE)
{}

Variant::Variant(const std::vector<std::string> &val)
    : vs(val)
    , type(Type::VECTOR_STRING)
{}

bool Variant::get_bool() const
{
    if (type == Type::BOOL) {
        return b;
    } else if (type == Type::LONG) {
        return (l != 0);
    } else if (type == Type::UNSIGNED) {
        return (u != 0);
    } else if (type == Type::DOUBLE) {
        return (d != 0.0);
    } else {
        throw ConvertError();
    }
}

int64_t Variant::get_long() const
{
    if (type == Type::BOOL) {
        return (b ? 1 : 0);
    } else if (type == Type::LONG) {
        return l;
    } else if (type == Type::UNSIGNED) {
        return u;
    } else if (type == Type::DOUBLE) {
        return d;
    } else if (type == Type::STRING) {
        return std::stoll(s);
    } else {
        throw ConvertError();
    }
}

uint64_t Variant::get_unsigned() const
{
    if (type == Type::BOOL) {
        return (b ? 1 : 0);
    } else if (type == Type::LONG) {
        return l;
    } else if (type == Type::UNSIGNED) {
        return u;
    } else if (type == Type::DOUBLE) {
        return d;
    } else if (type == Type::STRING) {
        return std::stoull(s);
    } else {
        throw ConvertError();
    }
}

double Variant::get_double() const
{
    if (type == Type::BOOL) {
        return (b ? 1.0 : 0.0);
    } else if (type == Type::LONG) {
        return l;
    } else if (type == Type::UNSIGNED) {
        return u;
    } else if (type == Type::DOUBLE) {
        return d;
    } else if (type == Type::STRING) {
        return std::stod(s);
    } else {
        throw ConvertError();
    }
}

std::string Variant::get_string() const
{
    if (type == Type::BOOL) {
        return (b ? "True" : "False");
    } else if (type == Type::LONG) {
        return std::to_string(l);
    } else if (type == Type::UNSIGNED) {
        return std::to_string(u);
    } else if (type == Type::DOUBLE) {
        return std::to_string(d);
    } else if (type == Type::STRING) {
        return s;
    } else {
        throw ConvertError();
    }
}

std::vector<long long int> Variant::get_long_array() const
{
    if (type == Type::VECTOR_LONG) {
        return vl;
    } else if (type == Type::VECTOR_UNSIGNED) {
        std::vector<long long int> out;
        for (auto& v: vu) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_DOUBLE) {
        std::vector<long long int> out;
        for (auto& v: vd) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_STRING) {
        std::vector<long long int> out;
        for (auto& v: vs) {
            out.push_back(std::stoll(v));
        }
        return out;
    } else {
        throw ConvertError();
    }
}

std::vector<unsigned long long int> Variant::get_unsigned_array() const
{
    if (type == Type::VECTOR_LONG) {
        std::vector<unsigned long long int> out;
        for (auto& v: vl) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_UNSIGNED) {
        return vu;
    } else if (type == Type::VECTOR_DOUBLE) {
        std::vector<unsigned long long int> out;
        for (auto& v: vd) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_STRING) {
        std::vector<unsigned long long int> out;
        for (auto& v: vs) {
            out.push_back(std::stoull(v));
        }
        return out;
    } else {
        throw ConvertError();
    }
}

std::vector<double> Variant::get_double_array() const
{
    if (type == Type::VECTOR_LONG) {
        std::vector<double> out;
        for (auto& v: vl) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_UNSIGNED) {
        std::vector<double> out;
        for (auto& v: vu) {
            out.push_back(v);
        }
        return out;
    } else if (type == Type::VECTOR_DOUBLE) {
        return vd;
    } else if (type == Type::VECTOR_STRING) {
        std::vector<double> out;
        for (auto& v: vs) {
            out.push_back(std::stod(v));
        }
        return out;
    } else {
        throw ConvertError();
    }
}

std::vector<std::string> Variant::get_string_array() const
{
    if (type == Type::VECTOR_LONG) {
        std::vector<std::string> out;
        for (auto& v: vl) {
            out.push_back(std::to_string(v));
        }
        return out;
    } else if (type == Type::VECTOR_UNSIGNED) {
        std::vector<std::string> out;
        for (auto& v: vu) {
            out.push_back(std::to_string(v));
        }
        return out;
    } else if (type == Type::VECTOR_DOUBLE) {
        std::vector<std::string> out;
        for (auto& v: vd) {
            out.push_back(std::to_string(v));
        }
        return out;
    } else if (type == Type::VECTOR_STRING) {
        return vs;
    } else {
        throw ConvertError();
    }
}
/*
    std::vector<std::string> get_string_array() const;
*/