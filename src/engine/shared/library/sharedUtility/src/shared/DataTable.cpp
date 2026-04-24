// ======================================================================
//
// DataTable.cpp
// 
// copyright 2002 Sony Online Entertainment
//
// ======================================================================

#include "sharedUtility/FirstSharedUtility.h"
#include "sharedUtility/DataTable.h"

#include "sharedFile/Iff.h"
#include "sharedUtility/DataTableCell.h"

#include <map>
#include <vector>
#ifdef _WIN64
#include <unordered_map>
#else
#include <hash_map>
#endif
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#if defined(_WIN32)
#include <windows.h>
#include <cstdarg>
#include <cstdio>
namespace
{
	// One OutputDebugString per line — tools like DbgView often show each call as a separate row,
	// so prefix-only lines looked like "empty" traces.
	void dtLoadTraceStr (char const * const s)
	{
		char line[640];
		_snprintf_s (
			line, sizeof (line), _TRUNCATE,
			"[Titan] DataTable::load: %s\r\n",
			s ? s : "");
		OutputDebugStringA (line);
	}

	void dtLoadTraceFmt (char const * const fmt, ...)
	{
		char msg[512];
		va_list ap;
		va_start (ap, fmt);
		_vsnprintf_s (msg, sizeof (msg), _TRUNCATE, fmt, ap);
		va_end (ap);
		char line[640];
		_snprintf_s (
			line, sizeof (line), _TRUNCATE,
			"[Titan] DataTable::load: %s\r\n",
			msg);
		OutputDebugStringA (line);
	}
}
#else
namespace
{
	inline void dtLoadTraceStr (char const *)
	{
	}

	inline void dtLoadTraceFmt (char const *, ...)
	{
	}
}
#endif

namespace DataTableLoadNamespace
{
	// Sane upper bounds: corrupt DTI! can have negative int32s (huge as size_t) and blow vector::reserve or OOM.
	enum { kMaxDataTableColumns = 100000, kMaxDataTableRows = 10000000 };

	// int rows*cols can overflow; malloc(0) may return null on some C libs; both yield AVs in _readCell.
	bool validateCellGrid (int const numRows, int const numCols, size_t *outCellBytes)
	{
		if (numRows < 0 || numCols < 0)
			return false;
		// 0 * N and N * 0 are 0, but a non-zero row count with zero columns (or the converse for cells) is invalid DTI! data.
		if (numRows > 0 && numCols == 0)
			return false;
		const uint64_t n = static_cast<uint64_t>(numRows) * static_cast<uint64_t>(numCols);
		// Generous cap: catches corrupt IFFs without blocking legitimate large ship manifests.
		if (n > 500000000ULL)
			return false;
		// Max linear index (numRows-1)*numCols + (numCols-1) must fit in ptrdiff_t for getDataTableCell().
		if (n > 0)
		{
			const uint64_t maxIndex =
				(static_cast<uint64_t>(numRows) - 1ULL) * static_cast<uint64_t>(numCols)
				+ (static_cast<uint64_t>(numCols) > 0 ? static_cast<uint64_t>(numCols) - 1ULL : 0ULL);
			if (maxIndex > static_cast<uint64_t>(PTRDIFF_MAX))
				return false;
		}
		const uint64_t bytes = n * static_cast<uint64_t>(sizeof(DataTableCell));
		if (bytes > static_cast<uint64_t>(SIZE_MAX))
			return false;
		*outCellBytes = static_cast<size_t>(bytes);
		return true;
	}
} // namespace

using DataTableLoadNamespace::validateCellGrid;

//----------------------------------------------------------------------------

const Tag DataTable::m_dataTableIffId = TAG(D,T,I,I);

//----------------------------------------------------------------------------

DataTable::DataTable() :
m_numRows(0),
m_numCols(0),
m_cells(0),
m_columns(),
m_index(),
m_types(),
m_columnIndexMap(new ColumnIndexMap(17)),
m_name()
{
}

//----------------------------------------------------------------------------

DataTable::~DataTable()
{
	std::vector<void *>::iterator k = m_index.begin();
	int count = 0;
	for (; k != m_index.end(); ++k, ++count)
	{
		if (static_cast<size_t>(count) >= m_types.size ())
		{
			WARNING (true, ("DataTable [%s]: destructor m_index slot %d exceeds m_types size %u — possible corrupt table; leak risk for built indexes.",
			                m_name.c_str (), count, static_cast<unsigned int> (m_types.size ())));
			break;
		}
		if (!m_types[static_cast<size_t>(count)])
		{
			WARNING (true, ("DataTable [%s]: destructor null column type at %d.", m_name.c_str (), count));
			continue;
		}

		DataTableColumnType::DataType dt = m_types[static_cast<size_t>(count)]->getBasicType();

		switch (dt)
		{
			case DataTableColumnType::DT_Int:
			{
				if (*k)
				{
					delete static_cast<std::multimap<int, int> *>(*k);
					*k = NULL;
				}
			}
			break;
			case DataTableColumnType::DT_Float:
			{
				if (*k)
				{
					delete static_cast<std::multimap<float, int> *>(*k);
					*k = NULL;
				}

			}
			break;
			case DataTableColumnType::DT_String:
			{
				if (*k)
				{
					delete static_cast<std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > *>(*k);
					*k = NULL;
				}

			}
			break;
			case DataTableColumnType::DT_HashString:
			case DataTableColumnType::DT_Enum:
			case DataTableColumnType::DT_Bool:
			case DataTableColumnType::DT_BitVector:
			case DataTableColumnType::DT_Unknown:
			case DataTableColumnType::DT_Comment:
			case DataTableColumnType::DT_PackedObjVars:
			default:
			{
				WARNING_STRICT_FATAL((*k) != NULL, ("Bad column Index %d. Unsupported datatype.  This may leak memory.", count));
			}
			break;
		}
	}

	if (m_cells)
	{
		// Use the same ptrdiff_t indexing as getDataTableCell(): int row*m_numCols can overflow for wide tables.
		ptrdiff_t const cellCount =
			static_cast<ptrdiff_t>(m_numRows) * static_cast<ptrdiff_t>(m_numCols);
		// m_cells is declared const for accessors; slab is writable (placement-new from load).
		DataTableCell *const cells = const_cast<DataTableCell *>(m_cells);
		for (ptrdiff_t idx = 0; idx < cellCount; ++idx)
		{
			cells[idx].~DataTableCell();
		}

		free((void *)cells);
		m_cells = 0;
	}

	DataTableColumnTypeVector::iterator l = m_types.begin();
	while(l != m_types.end())
	{
		delete *l++;
	}

	delete m_columnIndexMap;
	m_columnIndexMap = NULL;
}

//----------------------------------------------------------------------------

bool DataTable::doesColumnExist(const std::string & column) const
{
	DEBUG_FATAL(m_columnIndexMap->empty(), ("DataTable(%x) [%s]: Column index map is empty.\n", this, m_name.c_str()));

	return m_columnIndexMap->find(column) != m_columnIndexMap->end();
}

//----------------------------------------------------------------------------

int DataTable::findColumnNumber(const std::string & column) const
{
	DEBUG_FATAL(m_columnIndexMap->empty(), ("DataTable(%x) [%s]: Column index map is empty.\n", this, m_name.c_str()));

	int count = -1;
	ColumnIndexMap::const_iterator const it = m_columnIndexMap->find(column);

	if (it != m_columnIndexMap->end())
	{
		count = it->second;
	}

	return count;
}

//----------------------------------------------------------------------------
const std::string & DataTable::getColumnName(int column) const
{
	// Release: DEBUG_FATAL is stripped; OOB subscript here is a common silent 0xC0000005.
	FATAL (column < 0 || static_cast<size_t> (column) >= m_columns.size () || column >= getNumColumns (), (
		"DataTable [%s] getColumnName(): column %d out of range; getNumColumns()=%d m_columns.size()=%u. IFF/DTI may be corrupt or out of sync with loader.\n",
		m_name.c_str (), column, getNumColumns (), static_cast<unsigned int> (m_columns.size ())));
	return m_columns[static_cast<size_t> (column)];
}


//----------------------------------------------------------------------------

DataTableColumnType DataTable::getDataType(const std::string & type)
{
	return DataTableColumnType(type);
}

//----------------------------------------------------------------------------

DataTableColumnType const &DataTable::getDataTypeForColumn(const std::string& column) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable [%s] getDataTypeForColumn(): Column name [%s] is invalid\n", m_name.c_str(), column.c_str()));

	return getDataTypeForColumn(columnIndex);
}

//----------------------------------------------------------------------------

DataTableColumnType const &DataTable::getDataTypeForColumn(int column) const
{
	// Release: DEBUG_FATAL is stripped; OOB here is a common silent 0xC0000005 (e.g. TravelManager travel.iff load).
	FATAL (column < 0 || static_cast<size_t> (column) >= m_types.size () || column >= getNumColumns (), (
		"DataTable [%s] getDataTypeForColumn(int): column %d out of range; getNumColumns()=%d m_types.size()=%u. IFF/DTI may be corrupt.\n",
		m_name.c_str (), column, getNumColumns (), static_cast<unsigned int> (m_types.size ())));
	DEBUG_FATAL (column < 0 || column >= getNumColumns (), ("DataTable [%s] getDataTypeForColumn(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str (), column, getNumColumns ()));
	return *m_types[static_cast<size_t> (column)];
}

//----------------------------------------------------------------------------

int DataTable::getIntValue(const std::string & column, int row) const
{
	int const columnIndex = findColumnNumber(column);
	FATAL (columnIndex < 0 || columnIndex >= getNumColumns (), (
		"DataTable [%s] getIntValue(): column name [%s] not found or invalid; getNumColumns()=%d.\n",
		m_name.c_str (), column.c_str (), getNumColumns ()));
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable [%s] getIntValue(): Column name [%s] is invalid\n", m_name.c_str(), column.c_str()));
	return getIntValue(columnIndex, row);
}

//----------------------------------------------------------------------------

int DataTable::getIntValue(int column, int row) const
{
	// Release: DEBUG_FATAL stripped; OOB m_cells+offset is 0xC0000005 with no log line.
	FATAL (row < 0 || row >= getNumRows (), (
		"DataTable [%s] getIntValue(): row %d out of range; getNumRows()=%d. IFF/DTI or caller may be corrupt.\n",
		m_name.c_str (), row, getNumRows ()));
	FATAL (column < 0 || static_cast<size_t> (column) >= m_types.size () || column >= getNumColumns (), (
		"DataTable [%s] getIntValue(): column %d out of range; getNumColumns()=%d. IFF/DTI may be corrupt.\n",
		m_name.c_str (), column, getNumColumns ()));
	DEBUG_FATAL (row < 0 || row >= getNumRows (), ("DataTable [%s] getIntValue(): Invalid row number [%d].  Rows=[%d]\n", m_name.c_str (), row, getNumRows ()));
	DEBUG_FATAL (column < 0 || column >= getNumColumns (), ("DataTable [%s] getIntValue(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str (), column, getNumColumns ()));
	
	// we can return the value of an int column or the crc value of a string column
	if (m_types[static_cast<size_t>(column)]->getBasicType() == DataTableColumnType::DT_Int)
	{
		const DataTableCell *cell = getDataTableCell(column, row);
		DEBUG_FATAL(cell->getType()!=DataTableCell::CT_int, ("Could not convert row %d column %d to int value", row, column));
		return cell->getIntValue();
	}
	else if (m_types[static_cast<size_t>(column)]->getBasicType() == DataTableColumnType::DT_String)
	{
		const DataTableCell *cell = getDataTableCell(column, row);
		DEBUG_FATAL(cell->getType()!=DataTableCell::CT_string, ("Could not convert row %d column %d to string value", row, column));
		return cell->getStringValueCrc();
	}

	FATAL (true, (
		"DataTable [%s] getIntValue(): column %d has basic type %d (not int or string). IFF/DTI or cell load may be corrupt.\n",
		m_name.c_str (), column, static_cast<int> (m_types[static_cast<size_t> (column)]->getBasicType ())));
	return 0;
}

//----------------------------------------------------------------------------

int DataTable::getIntDefaultForColumn(const std::string & column) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable [%s] getIntDefaultForColumn(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), columnIndex, getNumColumns()));
	return getIntDefaultForColumn(columnIndex);
}

//----------------------------------------------------------------------------

int DataTable::getIntDefaultForColumn(int column) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] getIntDefaultForColumn(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));
	DEBUG_FATAL(m_types[static_cast<size_t>(column)]->getBasicType() != DataTableColumnType::DT_Int, ("Wrong data type for column %d.", column));
	std::string value;
	IGNORE_RETURN( getDataTypeForColumn(column).mangleValue(value) );
	return atoi(value.c_str());
}

//----------------------------------------------------------------------------

float DataTable::getFloatValue(const std::string & column, int row) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	return getFloatValue(columnIndex, row);
}

//----------------------------------------------------------------------------

float DataTable::getFloatValue(int column, int row) const
{
	DEBUG_FATAL(row < 0 || row >= getNumRows(), ("Row is invalid."));
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] getFloatValue(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));
	DEBUG_FATAL(m_types[static_cast<size_t>(column)]->getBasicType() != DataTableColumnType::DT_Float, ("Wrong data type for column %d.", column));

	const DataTableCell *cell = getDataTableCell(column, row);
	DEBUG_FATAL(cell->getType()!=DataTableCell::CT_float, ("Could not convert row %d column %d to float value", row, column));
	return cell->getFloatValue();

}

//----------------------------------------------------------------------------

float DataTable::getFloatDefaultForColumn(const std::string & column) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	return getFloatDefaultForColumn(columnIndex);
}

//----------------------------------------------------------------------------

float DataTable::getFloatDefaultForColumn(int column) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] getFloatDefaultForColumn(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));
	DEBUG_FATAL(m_types[static_cast<size_t>(column)]->getBasicType() != DataTableColumnType::DT_Float, ("Wrong data type for column %d.", column));
	std::string value;
	IGNORE_RETURN ( getDataTypeForColumn(column).mangleValue(value) );
	return static_cast<float>(atof(value.c_str()));
}

//----------------------------------------------------------------------------

const char *DataTable::getStringValue(const std::string & column, int row) const
{
	int const columnIndex = findColumnNumber(column);
	FATAL (columnIndex < 0 || columnIndex >= getNumColumns (), (
		"DataTable [%s] getStringValue(): column name [%s] not found or invalid; getNumColumns()=%d.\n",
		m_name.c_str (), column.c_str (), getNumColumns ()));
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	return getStringValue(columnIndex, row);
}

//----------------------------------------------------------------------------

const char *DataTable::getStringValue(int column, int row) const
{
	// Release: wrong column type (e.g. int in col 0) reads int bits as m_sz in getStringValue() -> 0xC0000005.
	FATAL (row < 0 || row >= getNumRows (), (
		"DataTable [%s] getStringValue(): row %d out of range; getNumRows()=%d. IFF/DTI may be corrupt.\n",
		m_name.c_str (), row, getNumRows ()));
	FATAL (column < 0 || static_cast<size_t> (column) >= m_types.size () || column >= getNumColumns (), (
		"DataTable [%s] getStringValue(): column %d out of range; getNumColumns()=%d. IFF/DTI may be corrupt.\n",
		m_name.c_str (), column, getNumColumns ()));
	if (m_types[static_cast<size_t> (column)]->getBasicType () != DataTableColumnType::DT_String)
	{
		FATAL (true, (
			"DataTable [%s] getStringValue(): column %d must be string (e.g. column 0 in travel.iff). Type is %s. Re-export with correct DTI; Release does not type-check on DEBUG.\n",
			m_name.c_str (), column, getDataTypeForColumn (column).getTypeSpecString ().c_str ()));
	}
	DEBUG_FATAL (row < 0 || row >= getNumRows (), ("Row [%d] is invalid.", row));
	DEBUG_FATAL (column < 0 || column >= getNumColumns (), ("Column [%d] is invalid.", column));
	DEBUG_FATAL (m_types[static_cast<size_t> (column)]->getBasicType () != DataTableColumnType::DT_String, ("Wrong data type for column %s (%d). Current data type is %s", getColumnName (column).c_str (), column, getDataTypeForColumn (column).getTypeSpecString ().c_str ()));

	const DataTableCell * const cell = getDataTableCell (column, row);
	FATAL (cell->getType () != DataTableCell::CT_string, (
		"DataTable [%s] getStringValue(): cell at row %d col %d is not string storage (IFF row data vs DTI mismatch?).\n",
		m_name.c_str (), row, column));
	DEBUG_FATAL (cell->getType () != DataTableCell::CT_string, ("Could not convert row %d column %d to string value", row, column));
	return cell->getStringValue ();

}

//----------------------------------------------------------------------------

std::string DataTable::getStringDefaultForColumn(const std::string & column) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	return getStringDefaultForColumn(columnIndex);
}

//----------------------------------------------------------------------------

std::string DataTable::getStringDefaultForColumn(int column) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] getStringDefaultForColumn(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));

	DEBUG_FATAL(m_types[static_cast<size_t>(column)]->getBasicType() != DataTableColumnType::DT_String, ("Wrong data type for column %d.", column));
	std::string value;
	IGNORE_RETURN( getDataTypeForColumn(column).mangleValue(value) );
	return value;
}

//----------------------------------------------------------------------------

void DataTable::getIntColumn(const std::string& column, std::vector<int>& returnVector) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	getIntColumn(columnIndex, returnVector);
}

//----------------------------------------------------------------------------

void DataTable::getIntColumn(const std::string& column, std::vector<long>& returnVector) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	getIntColumn(columnIndex, returnVector);
}

//----------------------------------------------------------------------------

void DataTable::getFloatColumn(const std::string& column, std::vector<float>& returnVector) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	getFloatColumn(columnIndex, returnVector);
}

//----------------------------------------------------------------------------

void DataTable::getStringColumn(const std::string& column, std::vector<const char *>& returnVector) const
{
	int const columnIndex = findColumnNumber(column);
	DEBUG_FATAL(columnIndex < 0 || columnIndex >= getNumColumns(), ("DataTable Column [%s] is invalid", column.c_str()));
	getStringColumn(columnIndex, returnVector);
}

//----------------------------------------------------------------------------

void DataTable::getIntColumn(int column, std::vector<int>& returnVector) const
{
	returnVector.clear();
	for (int i = 0; i < getNumRows(); ++i)
	{
		returnVector.push_back(getIntValue(column, i));
	}
}

//----------------------------------------------------------------------------

void DataTable::getIntColumn(int column, std::vector<long>& returnVector) const
{
	returnVector.clear();
	for (int i = 0; i < getNumRows(); ++i)
	{
		returnVector.push_back(getIntValue(column, i));
	}
}

//----------------------------------------------------------------------------

void DataTable::getFloatColumn(int column, std::vector<float>& returnVector) const
{
	returnVector.clear();
	for (int i = 0; i < getNumRows(); ++i)
	{
		returnVector.push_back(getFloatValue(column, i));
	}
}

//----------------------------------------------------------------------------

void DataTable::getStringColumn(int column, std::vector<const char *>& returnVector) const
{
	returnVector.clear();
	for (int i = 0; i < getNumRows(); ++i)
	{
		returnVector.push_back(getStringValue(column, i));
	}
}

//----------------------------------------------------------------------------

void DataTable::_readCell(Iff & iff, int column, int row)
{
	DataTableCell *cell = const_cast<DataTableCell *>(getDataTableCell(column, row));
	const DataTableColumnType &typeCol = *m_types[static_cast<size_t>(column)];
	switch (typeCol.getBasicType())
	{
	case DataTableColumnType::DT_Int:
	{
		int tmp = iff.read_int32();
		new (cell) DataTableCell(tmp);

		break;
	}
	case DataTableColumnType::DT_Float:
	{
		float tmp = iff.read_float();
		new (cell) DataTableCell(tmp);
		break;
	}
	case DataTableColumnType::DT_String:
	{
		// Large stack buffer in a hot path; x64 Release has seen 0xC0000005 during appearance_table load.
		enum { kMaxStringCellChars = 16383, kBuf = 16384 };
		std::vector<char> buffer(static_cast<size_t>(kBuf));
		buffer[0] = '\0';
		iff.read_string(buffer.data(), kMaxStringCellChars);
		buffer[static_cast<size_t>(kBuf - 1)] = '\0';
		new (cell) DataTableCell(buffer.data());
		break;
	}
	case DataTableColumnType::DT_Unknown:
	case DataTableColumnType::DT_HashString:
	case DataTableColumnType::DT_Enum:
	case DataTableColumnType::DT_Bool:
	case DataTableColumnType::DT_BitVector:
	case DataTableColumnType::DT_Comment:
	case DataTableColumnType::DT_PackedObjVars:
	default:
		FATAL (true, ("DataTable::_readCell: unsupported column type for row %d col %d [%s]. IFF/DTI corrupt or loader mismatch.", row, column, m_name.c_str ()));
		break;
	}
}

//----------------------------------------------------------------------------

void DataTable::load(Iff & iff)
{
	dtLoadTraceStr ("entered");
	IGNORE_RETURN ( iff.enterForm(m_dataTableIffId, false) );

	const Tag version = iff.getCurrentName();
	dtLoadTraceFmt ("version tag 0x%08X", static_cast<unsigned int>(version));
	if (version == TAG_0000)
	{
		dtLoadTraceStr ("branch load_0000");
		load_0000(iff);
	}
	else if (version == TAG_0001)
	{
		dtLoadTraceStr ("branch load_0001");
		load_0001(iff);
	}
	else
	{
		char buffer[5];
		
		ConvertTagToString(version, buffer);
		FATAL(true, ("unknown DataTable file format [%s]", buffer));
	}

	dtLoadTraceStr ("after inner load, before outer exitForm");
	iff.exitForm(m_dataTableIffId, false);
	dtLoadTraceStr ("after outer exitForm");

	int count = getNumColumns();
	for (int i = 0; i < count; ++i)
	{
		//initialize the table index used for searching.
		m_index.push_back(NULL);
	}

	dtLoadTraceStr ("before buildColumnIndexMap");
	buildColumnIndexMap();
	dtLoadTraceStr ("after buildColumnIndexMap");

	if (NULL != iff.getFileName())
		m_name = iff.getFileName();
	else
		m_name.clear();
}

//----------------------------------------------------------------------------

void DataTable::load_0000(Iff & iff)
{
	dtLoadTraceStr ("load_0000 enter");
	IGNORE_RETURN ( iff.enterForm(TAG_0000, false) );

	//load columns
	iff.enterChunk(TAG(C,O,L,S));
	m_numCols = iff.read_int32();
	dtLoadTraceFmt ("load_0000 m_numCols=%d", m_numCols);
	FATAL(
		m_numCols < 0 || m_numCols > DataTableLoadNamespace::kMaxDataTableColumns,
		("DataTable load_0000: invalid column count %d. IFF corrupt or incompatible.",
		 m_numCols));
	int i = 0;
	m_columns.reserve(m_numCols);
	std::string tmpString;
	for (i = 0; i < m_numCols; ++i)
	{
		iff.read_string(tmpString);
		m_columns.push_back(tmpString);
		tmpString.clear();
	}

	iff.exitChunk(TAG(C,O,L,S));	
	dtLoadTraceStr ("load_0000 after COLS");
	//load type info
	iff.enterChunk(TAG(T,Y,P,E));
	for (i = 0; i < m_numCols; ++i)
	{
		DataTableColumnType::DataType dt = static_cast<DataTableColumnType::DataType>(iff.read_int32());
		// version 0000 has only Int, Float, String, and no other format information
		switch (dt)
		{
		case DataTableColumnType::DT_Int:
			{
				m_types.push_back(new DataTableColumnType("i"));
			}
			break;
		case DataTableColumnType::DT_Float:
			{
				m_types.push_back(new DataTableColumnType("f"));
			}
			break;
		case DataTableColumnType::DT_String:
			{
				m_types.push_back(new DataTableColumnType("s"));
			}
			break;
		case DataTableColumnType::DT_Unknown:
		case DataTableColumnType::DT_HashString:
		case DataTableColumnType::DT_Enum:
		case DataTableColumnType::DT_Bool:
		case DataTableColumnType::DT_BitVector:
		case DataTableColumnType::DT_Comment:
		case DataTableColumnType::DT_PackedObjVars:

		default:
			{
				FATAL(true, ("unknown column type loaded from version 0000"));
			}
			break;
		}
	}
	iff.exitChunk(TAG(T,Y,P,E));
	dtLoadTraceStr ("load_0000 after TYPE");

	//load rows
	iff.enterChunk(TAG(R,O,W,S));
	m_numRows = iff.read_int32();
	FATAL(
		m_numRows < 0 || m_numRows > DataTableLoadNamespace::kMaxDataTableRows,
		("DataTable load_0000: invalid row count %d. IFF corrupt or incompatible.",
		 m_numRows));
	dtLoadTraceFmt ("load_0000 m_numRows=%d", m_numRows);

	size_t cellSize = 0;
	FATAL(
		!validateCellGrid(m_numRows, m_numCols, &cellSize),
		("DataTable load_0000: invalid or excessive cell grid (rows=%d, cols=%d). IFF corrupt or incompatible.",
		 m_numRows, m_numCols));
	dtLoadTraceFmt ("load_0000 cellBytes=%zu", cellSize);

	void * cellMemory = 0;
	if (cellSize > 0)
	{
		cellMemory = malloc(cellSize);
		FATAL(
			!cellMemory,
			("DataTable load_0000: malloc %zu bytes failed (rows=%d, cols=%d).",
			 cellSize, m_numRows, m_numCols));
	}
	dtLoadTraceStr ("load_0000 malloc ok");
	m_cells = (DataTableCell *)cellMemory;
	for (i = 0; i < m_numRows; ++i)
	{
		if ((i % 5000) == 0)
			dtLoadTraceFmt ("load_0000 row %d / %d", i, m_numRows);
		for (int j = 0; j < m_numCols; ++j)
		{
			_readCell(iff, j, i);
		}
	}

	dtLoadTraceStr ("load_0000 cells done");
	iff.exitChunk(TAG(R,O,W,S));
	dtLoadTraceStr ("load_0000 after ROWS chunk");
	iff.exitForm(TAG_0000, false);
	dtLoadTraceStr ("load_0000 exitForm 0000");
}

//----------------------------------------------------------------------------

void DataTable::load_0001(Iff & iff)
{
	dtLoadTraceStr ("load_0001 enter");
	IGNORE_RETURN( iff.enterForm(TAG_0001, false) );

	//load columns
	iff.enterChunk(TAG(C,O,L,S));
	m_numCols = iff.read_int32();
	FATAL(
		m_numCols < 0 || m_numCols > DataTableLoadNamespace::kMaxDataTableColumns,
		("DataTable load_0001: invalid column count %d. IFF corrupt or incompatible.",
		 m_numCols));
	dtLoadTraceFmt ("load_0001 m_numCols=%d", m_numCols);
	int i = 0;
	m_columns.reserve(m_numCols);
	std::string tmpString;
	for (i = 0; i < m_numCols; ++i)
	{
		iff.read_string(tmpString);
		m_columns.push_back(tmpString);
		tmpString.clear();
	}

	iff.exitChunk(TAG(C,O,L,S));	
	dtLoadTraceStr ("load_0001 after COLS");
	//load type info
	dtLoadTraceStr ("load_0001 before enterChunk(TYPE)");
	iff.enterChunk(TAG(T,Y,P,E));
	dtLoadTraceStr ("load_0001 after enterChunk(TYPE)");
	for (i = 0; i < m_numCols; ++i)
	{
		// version 0001 has a format string for the type
		std::string const typeStr (iff.read_stdstring ());
		FATAL (
			typeStr.empty (),
			("DataTable load_0001: empty TYPE format string at column %d [%s]",
			 i,
			 iff.getFileName () ? iff.getFileName () : "(memory)"));
		// Defer DataTableCell default allocation until after all TYPE strings are consumed (IFF cursor
		// stays inside TYPE chunk; avoids pooled-cell + nested table load interactions during read_stdstring).
		m_types.push_back (new DataTableColumnType (typeStr, true));
	}
	for (i = 0; i < m_numCols; ++i)
		const_cast<DataTableColumnType *>(m_types[static_cast<size_t>(i)])->finalizeDeferredDefaultCell ();
	iff.exitChunk(TAG(T,Y,P,E));
	dtLoadTraceStr ("load_0001 after TYPE");

	//load rows
	iff.enterChunk(TAG(R,O,W,S));
	m_numRows = iff.read_int32();
	FATAL(
		m_numRows < 0 || m_numRows > DataTableLoadNamespace::kMaxDataTableRows,
		("DataTable load_0001: invalid row count %d. IFF corrupt or incompatible.",
		 m_numRows));
	dtLoadTraceFmt ("load_0001 m_numRows=%d", m_numRows);

	size_t cellSize = 0;
	FATAL(
		!validateCellGrid(m_numRows, m_numCols, &cellSize),
		("DataTable load_0001: invalid or excessive cell grid (rows=%d, cols=%d). IFF corrupt or incompatible.",
		 m_numRows, m_numCols));
	dtLoadTraceFmt ("load_0001 cellBytes=%zu", cellSize);

	void * cellMemory = 0;
	if (cellSize > 0)
	{
		cellMemory = malloc(cellSize);
		FATAL(
			!cellMemory,
			("DataTable load_0001: malloc %zu bytes failed (rows=%d, cols=%d).",
			 cellSize, m_numRows, m_numCols));
	}
	dtLoadTraceStr ("load_0001 malloc ok");
	m_cells = (DataTableCell *)cellMemory;
	for (i = 0; i < m_numRows; ++i)
	{
		if ((i % 5000) == 0)
			dtLoadTraceFmt ("load_0001 row %d / %d", i, m_numRows);
		for (int j = 0; j < m_numCols; ++j)
		{
			_readCell(iff, j, i);
		}
	}

	dtLoadTraceStr ("load_0001 cells done");
	iff.exitChunk(TAG(R,O,W,S));
	dtLoadTraceStr ("load_0001 after ROWS chunk");
	iff.exitForm(TAG_0001, false);
	dtLoadTraceStr ("load_0001 exitForm 0001");
}

// ----------------------------------------------------------------------

int DataTable::searchColumnString( int column, const std::string & searchValue ) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] searchColumnString(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));

	int retval = -1;

	void * voidIndex = m_index[ static_cast<size_t>(column) ];
	if (!voidIndex)
	{
		std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > * indexPair = new std::pair<std::multimap<const std::string, int>, std::multimap<int, int> >;
		NOT_NULL(indexPair);

		m_index[static_cast<size_t>(column)] = static_cast<void *>(indexPair);

		//no index has been built yet for this column
		int rowCount = getNumRows();
		for (int i = 0; i < rowCount; ++i)
		{
			std::string const valueString(getStringValue(column, i));
			IGNORE_RETURN ( indexPair->first.insert(std::pair<const std::string, int>(valueString, i)) );
			int valueCrc = getIntValue(column,i);
			IGNORE_RETURN ( indexPair->second.insert(std::pair<int, int>(valueCrc, i)) );
			if (retval == -1 && valueString == searchValue)
				retval = i;
		}
	}
	else
	{
		std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > * indexPair = static_cast<std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > *>(voidIndex);
		std::multimap<const std::string, int>::iterator iter = indexPair->first.find(searchValue);
		if (iter != indexPair->first.end())
			retval = iter->second;	
	}

	return retval;
}

// ----------

int DataTable::searchColumnFloat( int column, float searchValue ) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] searchColumnFloat(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));

	int retval = -1;

	void * voidIndex = m_index[ static_cast<size_t>(column) ];
	if (!voidIndex)
	{
		std::multimap<float, int> * index = new std::multimap<float, int>;
		NOT_NULL(index);

		m_index[static_cast<size_t>(column)] = static_cast<void *>(index);

		//no index has been built yet for this column
		for (int i = 0; i < getNumRows(); ++i)
		{
			float rowValue = getFloatValue(column,i);
			IGNORE_RETURN ( index->insert(std::pair<float, int>(rowValue, i)) );
			if (retval == -1 && rowValue == searchValue) //lint !e777 //ok to compare floats
				retval = i;
		}
	}
	else
	{
		std::multimap<float, int> * index = static_cast<std::multimap<float, int> *>(voidIndex);
		std::multimap<float, int>::iterator iter = index->find(searchValue);
		if (iter != index->end())
			retval = iter->second;
	}

	return retval;
}

// ----------

int DataTable::searchColumnInt( int column, int searchValue ) const
{
	DEBUG_FATAL(column < 0 || column >= getNumColumns(), ("DataTable [%s] searchColumnInt(): Invalid col number [%d].  Cols=[%d]\n", m_name.c_str(), column, getNumColumns()));

	int retval = -1;
	DataTableColumnType::DataType columnType = m_types[static_cast<size_t>(column)]->getBasicType();

	void * voidIndex = m_index[ static_cast<size_t>(column) ];
	if (!voidIndex)
	{
		if (columnType == DataTableColumnType::DT_Int)
		{
			std::multimap<int, int> * index = new std::multimap<int, int>;
			NOT_NULL(index);

			m_index[static_cast<size_t>(column)] = static_cast<void *>(index);

			//no index has been built yet for this column
			int rowCount = getNumRows();
			for (int i = 0; i < rowCount; ++i)
			{
				int rowValue = getIntValue(column,i);
				IGNORE_RETURN ( index->insert(std::pair<int, int>(rowValue, i)) );
				if (retval == -1 && rowValue == searchValue)
					retval = i;
			}
		}
		else if (columnType == DataTableColumnType::DT_String)
		{
			std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > * indexPair = new std::pair<std::multimap<const std::string, int>, std::multimap<int, int> >;
			NOT_NULL(indexPair);

			m_index[static_cast<size_t>(column)] = static_cast<void *>(indexPair);

			//no index has been built yet for this column
			int rowCount = getNumRows();
			for (int i = 0; i < rowCount; ++i)
			{
				std::string const valueString(getStringValue(column, i));
				IGNORE_RETURN ( indexPair->first.insert(std::pair<const std::string, int>(valueString, i)) );
				int valueCrc = getIntValue(column,i);
				IGNORE_RETURN ( indexPair->second.insert(std::pair<int, int>(valueCrc, i)) );
				if (retval == -1 && valueCrc == searchValue)
					retval = i;
			}
		}
	}
	else
	{
		if (columnType == DataTableColumnType::DT_Int)
		{
			std::multimap<int, int> * index = static_cast<std::multimap<int, int> *>(voidIndex);
			std::multimap<int, int>::iterator iter = index->find(searchValue);
			if (iter != index->end())
				retval = iter->second;
		}
		else if (columnType == DataTableColumnType::DT_String)
		{
			std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > * indexPair = static_cast<std::pair<std::multimap<const std::string, int>, std::multimap<int, int> > *>(voidIndex);
			std::multimap<int, int>::iterator iter = indexPair->second.find(searchValue);
			if (iter != indexPair->second.end())
				retval = iter->second;
		}
	}

	return retval;

}

// ----------

void DataTable::buildColumnIndexMap()
{
	std::vector<std::string>::const_iterator i = m_columns.begin();
	for (int columnIndex = 0; i != m_columns.end(); ++i, ++columnIndex)
	{
		(*m_columnIndexMap)[*i] = columnIndex;
	}
}

//----------------------------------------------------------------------------

