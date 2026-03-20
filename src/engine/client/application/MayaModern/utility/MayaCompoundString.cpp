#include "MayaCompoundString.h"

MayaCompoundString::MayaCompoundString()
    : m_separator('_')
{
}

MayaCompoundString::MayaCompoundString(const MString& compoundString, char separator)
    : m_compoundString(compoundString)
    , m_separator(separator)
{
    buildComponentStrings();
}

void MayaCompoundString::buildComponentStrings()
{
    m_componentStrings.clear();
    const unsigned len = m_compoundString.length();
    const char* src = m_compoundString.asChar();
    if (!src || len == 0)
        return;

    std::string current;
    for (unsigned i = 0; i < len; ++i)
    {
        const char c = src[i];
        if (c == m_separator)
        {
            if (i + 1 < len && src[i + 1] == m_separator)
            {
                m_componentStrings.push_back(MString(current.c_str()));
                current.clear();
                ++i;
                continue;
            }
            continue;
        }
        current += c;
    }
    if (!current.empty())
        m_componentStrings.push_back(MString(current.c_str()));
}

int MayaCompoundString::getComponentCount() const
{
    return static_cast<int>(m_componentStrings.size());
}

const MString& MayaCompoundString::getComponentString(int index) const
{
    static const MString s_empty;
    const size_t i = static_cast<size_t>(index);
    return (i < m_componentStrings.size()) ? m_componentStrings[i] : s_empty;
}

std::string MayaCompoundString::getComponentStdString(int index) const
{
    return getComponentString(index).asChar();
}
