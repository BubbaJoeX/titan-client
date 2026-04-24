// ======================================================================
//
// DataTableColumnType.h
//
// copyright 2002 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_DataTableColumnType_H
#define INCLUDED_DataTableColumnType_H

#include <string>

class DataTableCell;

class DataTableColumnType
{
public:
	/// When \a deferDefaultCell is true, call \ref finalizeDeferredDefaultCell after TYPE parsing (load_0001).
	explicit DataTableColumnType(std::string const &desc, bool deferDefaultCell = false);
	~DataTableColumnType ();
	DataTableColumnType(DataTableColumnType const &rhs);
	DataTableColumnType &operator=(DataTableColumnType const &rhs);

	void finalizeDeferredDefaultCell ();

	enum DataType
	{
		DT_Int,
		DT_Float,
		DT_String,
		DT_Unknown,
		DT_Comment,
		// specializations of DT_Int basic type
		DT_HashString,
		DT_Enum,
		DT_Bool,		
		// specializations of DT_String basic type
		DT_PackedObjVars,
		DT_BitVector,
	};

	DataType getType() const;
	DataType getBasicType() const;
	std::string const &getTypeSpecString() const;
	bool mangleValue(std::string &value) const;
	DataTableCell const *getDefaultCell() const;
	bool areUniqueCellsRequired() const;
	/** Reverse lookup: given enum int value, return the label. Returns false if not found. */
	bool getEnumLabelForValue(int value, std::string & outLabel) const;
	/** Reverse lookup: given bitvector int value, return comma-separated labels. Returns false if enum map missing. */
	bool getBitVectorLabelsForValue(int value, std::string & outLabels) const;

private:
	bool lookupEnum(std::string const &label, int &result) const;
	bool lookupBitVector(std::string const &label, int &result) const;

private:
	DataTableColumnType();
	void createDefaultCell();

	std::string m_typeSpecString;
	DataType m_type;
	DataType m_basicType;
	std::string m_defaultValue;
	typedef stdmap<std::string, int>::fwd StringIntMap;
	StringIntMap * m_enumMap;
	DataTableCell * m_defaultCell;
};

inline const DataTableCell * DataTableColumnType::getDefaultCell() const
{
	return m_defaultCell;
}

#endif // INCLUDED_DataTableColumnType_H

