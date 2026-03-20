#ifndef SWGMAYAEDITOR_MAYACOMPOUNDSTRING_H
#define SWGMAYAEDITOR_MAYACOMPOUNDSTRING_H

#include <maya/MString.h>
#include <string>
#include <vector>

/**
 * Parses Maya node names using __ as component separator.
 * e.g. "root__all_b_l0" -> components ["root", "all_b_l0"]
 * "lthigh__skeleton" -> ["lthigh", "skeleton"]
 * Matches MayaExporter MayaCompoundString behavior for export.
 */
class MayaCompoundString
{
public:
    MayaCompoundString();
    explicit MayaCompoundString(const MString& compoundString, char separator = '_');

    int getComponentCount() const;
    const MString& getComponentString(int index) const;
    std::string getComponentStdString(int index) const;

private:
    MString m_compoundString;
    std::vector<MString> m_componentStrings;
    char m_separator;

    void buildComponentStrings();
};

#endif
