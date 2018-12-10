/*
* Copyright (C) 2018 The Regents of the University of California
* All Rights Reserved
*
* This library can redistribute it and/or modify under the terms
* of the GNU Lesser General Public License Version 2.1 as published
* by the Free Software Foundation.
*
*/


#ifndef CLS_TABULAR_UTILS_H
#define CLS_TABULAR_UTILS_H

#include <string>
#include <sstream>
#include <type_traits>

#include "include/types.h"
#include <boost/algorithm/string/classification.hpp> // for boost::is_any_of
#include <boost/algorithm/string/split.hpp> // for boost::split
#include <boost/algorithm/string.hpp> // for boost::trim
#include <boost/lexical_cast.hpp>

#include "re2/re2.h"
#include "objclass/objclass.h"

#include "cls_tabular.h"
#include "flatbuffers/flexbuffers.h"
#include "skyhookv1_generated.h"

namespace Tables {

enum TablesErrCodes {
    EmptySchema = 1, // note: must start at 1
    BadColInfoFormat,
    BadColInfoConversion,
    UnsupportedSkyDataType,
    UnsupportedAggDataType,
    UnknownSkyDataType,
    RequestedColIndexOOB,
    PredicateComparisonNotDefined,
    PredicateRegexPatternNotSet,
    OpNotRecognized,
    OpNotImplemented,
    BuildSkyIndexDecodeBlsErr,
    BuildSkyIndexExtractFbErr,
    BuildSkyIndexUnsupportedColType,
    BuildSkyIndexColTypeNotImplemented,
    BuildSkyIndexUnsupportedNumCols,
    BuildSkyIndexUnsupportedAggCol,
    BuildSkyIndexKeyCreationFailed,
    RowIndexOOB,
};

// skyhook data types, as supported by underlying data format
enum SkyDataType {
    SDT_INT8 = 1,  // note: must start at 1
    SDT_INT16,
    SDT_INT32,
    SDT_INT64,
    SDT_UINT8,
    SDT_UINT16,
    SDT_UINT32,
    SDT_UINT64,
    SDT_CHAR,
    SDT_UCHAR,
    SDT_BOOL,
    SDT_FLOAT,
    SDT_DOUBLE,
    SDT_DATE,
    SDT_STRING,
    /*
    SDT_BLOB,  // TODO
    SDT_VECTOR_INT8,  // TODO
    SDT_VECTOR_INT64,  // TODO
    SDT_VECTOR_UINT8,  // TODO
    SDT_VECTOR_UINT64,  // TODO
    SDT_VECTOR_DOUBLE,  // TODO
    SDT_VECTOR_STRING,  // TODO
    */
    SDT_FIRST = SDT_INT8,
    SDT_LAST = SDT_STRING,
};

enum SkyOpType {
    // ARITHMETIC COMPARISON
    SOT_lt = 1,  // note: must start at 1
    SOT_gt,
    SOT_eq,
    SOT_ne,
    SOT_leq,
    SOT_geq,
    // ARITHMETIC FUNCTIONS (TODO)
    SOT_add,
    SOT_sub,
    SOT_mul,
    SOT_div,
    SOT_min,
    SOT_max,
    SOT_sum,
    SOT_cnt,
    // LEXICAL (regex)
    SOT_like,
    // MEMBERSHIP (collections) (TODO)
    SOT_in,
    SOT_not_in,
    // DATE (SQL)  (TODO)
    SOT_between,
    // LOGICAL
    SOT_logical_or,
    SOT_logical_and,
    SOT_logical_not,
    SOT_logical_nor,
    SOT_logical_xor,
    SOT_logical_nand,
    // BITWISE
    SOT_bitwise_and,
    SOT_bitwise_or,
    SOT_FIRST = SOT_lt,
    SOT_LAST = SOT_bitwise_or,
};

enum AggColIdx {
    AGG_COL_MIN = -1,  // defines the schema idx for agg ops.
    AGG_COL_MAX = -2,
    AGG_COL_SUM = -3,
    AGG_COL_CNT = -4,
    AGG_COL_FIRST = AGG_COL_MIN,
    AGG_COL_LAST = AGG_COL_CNT,
};

enum SkyIdxType
{
    SIT_IDX_FB,
    SIT_IDX_RID,
    SIT_IDX_REC,
    SIT_IDX_TXT
};

const std::map<std::string, int> agg_idx_names = {
    {"min", AGG_COL_MIN},
    {"max", AGG_COL_MAX},
    {"sum", AGG_COL_SUM},
    {"cnt", AGG_COL_CNT}
};

const int offset_to_skyhook_version = 4;
const int offset_to_schema_version = 6;
const int offset_to_table_name = 8;
const int offset_to_schema = 10;
const int offset_to_delete_vec = 12;
const int offset_to_rows_vec = 14;
const int offset_to_nrows = 16;
const int offset_to_RID = 4;
const int offset_to_nullbits_vec = 6;
const int offset_to_data = 8;
const std::string PRED_DELIM_OUTER = ";";
const std::string PRED_DELIM_INNER = ",";
const std::string PROJECT_DEFAULT = "*";
const std::string SELECT_DEFAULT = "*";
const std::string REGEX_DEFAULT_PATTERN = "/.^/";  // matches nothing.
const int IDX_MAX_NUM_COLS = 4;
const std::string IDX_KEY_DELIM_MINR = "-";
const std::string IDX_KEY_DELIM_MAJR = ":";
const std::string IDX_KEY_COLS_DEFAULT = "*";
const std::string SCHEMA_NAME_DEFAULT = "*";

/*
 * Convert integer to string for index/omap of primary key
 */
static inline std::string u64tostr(uint64_t value)
{
  std::stringstream ss;
  ss << std::setw(20) << std::setfill('0') << value;
  return ss.str();
}

/*
 * Convert string into numeric value.
 */
static inline int strtou64(const std::string value, uint64_t *out)
{
  uint64_t v;

  try {
    v = boost::lexical_cast<uint64_t>(value);
  } catch (boost::bad_lexical_cast &) {
    CLS_ERR("converting key into numeric value %s", value.c_str());
    return -EIO;
  }

  *out = v;
  return 0;
}

// contains the value of a predicate to be applied
template <class T>
class PredicateValue
{
public:
    T val;
    PredicateValue(T v) : val(v) {};
    PredicateValue(const PredicateValue& rhs);
    PredicateValue& operator=(const PredicateValue& rhs);
};

// PredBase is not template typed, derived is type templated,
// allows us to have vectors of chained base class predicates
class PredicateBase
{
public:
    virtual ~PredicateBase() {}
    virtual int colIdx() = 0;  // to check info via base ptr before dynm cast
    virtual int colType() = 0;
    virtual int opType() = 0;
    virtual bool isGlobalAgg() = 0;
};
typedef std::vector<class PredicateBase*> predicate_vec;

template <typename T>
class TypedPredicate : public PredicateBase
{
private:
    const int col_idx;
    const int col_type;
    const int op_type;
    const bool is_global_agg;
    const re2::RE2* regx;
    PredicateValue<T> value;

public:
    TypedPredicate(int idx, int type, int op, const T& val) :
        col_idx(idx),
        col_type(type),
        op_type(op),
        is_global_agg(op==SOT_min || op==SOT_max ||
                      op==SOT_sum || op==SOT_cnt),
        value(val) {

            // ONLY VERIFY op type is valid for specified col type and value
            // type T, and compile regex if needed.
            switch (op_type) {
                // ARITHMETIC/COMPARISON
                case SOT_lt:
                case SOT_gt:
                case SOT_eq:
                case SOT_ne:
                case SOT_leq:
                case SOT_geq:
                case SOT_add:
                case SOT_sub:
                case SOT_mul:
                case SOT_div:
                case SOT_min:
                case SOT_max:
                case SOT_sum:
                case SOT_cnt:
                    assert (
                            std::is_arithmetic<T>::value
                            && (
                            col_type==SDT_INT8 ||
                            col_type==SDT_INT16 ||
                            col_type==SDT_INT32 ||
                            col_type==SDT_INT64 ||
                            col_type==SDT_UINT8 ||
                            col_type==SDT_UINT16 ||
                            col_type==SDT_UINT32 ||
                            col_type==SDT_UINT64 ||
                            col_type==SDT_BOOL ||
                            col_type==SDT_CHAR ||
                            col_type==SDT_UCHAR ||
                            col_type==SDT_FLOAT ||
                            col_type==SDT_DOUBLE));
                    break;

                // LEXICAL (regex)
                case SOT_like:
                    assert ((
                        (std::is_same<T, std::string>::value) == true ||
                        (std::is_same<T, unsigned char>::value) == true ||
                        (std::is_same<T, char>::value) == true)
                        && (
                        col_type==SDT_STRING ||
                        col_type==SDT_CHAR ||
                        col_type==SDT_UCHAR));
                    break;

                // MEMBERSHIP (collections)
                case SOT_in:
                case SOT_not_in:
                    assert(((std::is_same<T, std::vector<T>>::value) == true));
                    assert (TablesErrCodes::OpNotImplemented==0); // TODO
                    break;

                // DATE (SQL)
                case SOT_between:
                    assert (TablesErrCodes::OpNotImplemented==0);  // TODO
                    break;

                // LOGICAL
                case SOT_logical_or:
                case SOT_logical_and:
                case SOT_logical_not:
                case SOT_logical_nor:
                case SOT_logical_xor:
                case SOT_logical_nand:
                    assert (std::is_integral<T>::value);  // includes bool+char
                    assert (col_type==SDT_INT8 ||
                            col_type==SDT_INT16 ||
                            col_type==SDT_INT32 ||
                            col_type==SDT_INT64 ||
                            col_type==SDT_UINT8 ||
                            col_type==SDT_UINT16 ||
                            col_type==SDT_UINT32 ||
                            col_type==SDT_UINT64 ||
                            col_type==SDT_BOOL ||
                            col_type==SDT_CHAR ||
                            col_type==SDT_UCHAR);
                    break;

                // BITWISE
                case SOT_bitwise_and:
                case SOT_bitwise_or:
                    assert (std::is_integral<T>::value);
                    assert (std::is_unsigned<T>::value);
                    break;

                // FALL THROUGH OP not recognized
                default:
                    assert (TablesErrCodes::OpNotRecognized==0);
                    break;
            }

            // compile regex
            std::string pattern;
            if (op_type == SOT_like) {
                pattern = this->Val();  // force str type for regex
                regx = new re2::RE2(pattern);
                assert (regx->ok());
            }
        }

    TypedPredicate(const TypedPredicate &p) : // copy constructor as needed
        col_idx(p.col_idx),
        col_type(p.col_type),
        op_type(p.op_type),
        is_global_agg(p.is_global_agg),
        value(p.value.val) {
            regx = new re2::RE2(p.regx->pattern());
        }

    ~TypedPredicate() { }
    TypedPredicate& getThis() {return *this;}
    const TypedPredicate& getThis() const {return *this;}
    virtual int colIdx() {return col_idx;}
    virtual int colType() {return col_type;}
    virtual int opType() {return op_type;}
    virtual bool isGlobalAgg() {return is_global_agg;}
    T Val() {return value.val;}
    const re2::RE2* getRegex() {return regx;}
    void updateAgg(T newval) {value.val = newval;}
};

// col metadata used for the schema
struct col_info {
    const int idx;
    const int type;
    const bool is_key;
    const bool nullable;
    const std::string name;

    col_info(int i, int t, bool key, bool nulls, std::string n) :
        idx(i),
        type(t),
        is_key(key),
        nullable(nulls),
        name(n) {assert(type >= SDT_FIRST && type <= SDT_LAST);}

    col_info(std::string i, std::string t, std::string key, std::string nulls,
        std::string n) :
        idx(std::stoi(i.c_str())),
        type(std::stoi(t.c_str())),
        is_key(key[0]=='1'),
        nullable(nulls[0]=='1'),
        name(n) {assert(type >= SDT_FIRST && type <= SDT_LAST);}

    std::string toString() {
        return ( "   " +
            std::to_string(idx) + " " +
            std::to_string(type) + " " +
            std::to_string(is_key) + " " +
            std::to_string(nullable) + " " +
            name + "   ");}

    bool compareName(std::string colname) {return (colname==name)?true:false;}
};
typedef vector<struct col_info> schema_vec;

inline
bool compareColInfo(const struct col_info& l, const struct col_info& r) {
    return (
        (l.idx==r.idx) &&
        (l.type==r.type) &&
        (l.is_key==r.is_key) &&
        (l.nullable==r.nullable) &&
        (l.name==r.name)
    );
}

// the below are used in our root table
typedef vector<uint8_t> delete_vector;
typedef const flatbuffers::Vector<flatbuffers::Offset<Row>>* row_offs;

// the below are used in our row table
typedef vector<uint64_t> nullbits_vector;
typedef flexbuffers::Reference row_data_ref;

// skyhookdb root metadata, refering to a (sub)partition of rows
// abstracts a partition from its underlying data format/layout
struct root_table {
    const int skyhook_version;
    int schema_version;
    string table_name;
    string schema_name;
    delete_vector delete_vec;
    row_offs offs;
    uint32_t nrows;

    root_table(int skyver, int schmver, std::string tblname, std::string schm,
               delete_vector d, row_offs ro, uint32_t n) :
        skyhook_version(skyver),
        schema_version(schmver),
        table_name(tblname),
        schema_name(schm),
        delete_vec(d),
        offs(ro),
        nrows(n) {};

};
typedef struct root_table sky_root;

// skyhookdb row metadata and row data, wraps a row of data
// abstracts a row from its underlying data format/layout
struct rec_table {
    const int64_t RID;
    nullbits_vector nullbits;
    const row_data_ref data;

    rec_table(int64_t rid, nullbits_vector n, row_data_ref d) :
        RID(rid),
        nullbits(n),
        data(d) {};
};
typedef struct rec_table sky_rec;

// a test schema for the tpch lineitem table.
// format: "col_idx col_type col_is_key nullable col_name \n"
// note the col_idx always refers to the index in the table's current schema
const std::string lineitem_test_schema_string = " \
    0 " +  std::to_string(SDT_INT64) + " 1 0 orderkey \n\
    1 " +  std::to_string(SDT_INT32) + " 0 1 partkey \n\
    2 " +  std::to_string(SDT_INT32) + " 0 1 suppkey \n\
    3 " +  std::to_string(SDT_INT64) + " 1 0 linenumber \n\
    4 " +  std::to_string(SDT_FLOAT) + " 0 1 quantity \n\
    5 " +  std::to_string(SDT_DOUBLE) + " 0 1 extendedprice \n\
    6 " +  std::to_string(SDT_FLOAT) + " 0 1 discount \n\
    7 " +  std::to_string(SDT_DOUBLE) + " 0 1 tax \n\
    8 " +  std::to_string(SDT_CHAR) + " 0 1 returnflag \n\
    9 " +  std::to_string(SDT_CHAR) + " 0 1 linestatus \n\
    10 " +  std::to_string(SDT_DATE) + " 0 1 shipdate \n\
    11 " +  std::to_string(SDT_DATE) + " 0 1 commitdate \n\
    12 " +  std::to_string(SDT_DATE) + " 0 1 receipdate \n\
    13 " +  std::to_string(SDT_STRING) + " 0 1 shipinstruct \n\
    14 " +  std::to_string(SDT_STRING) + " 0 1 shipmode \n\
    15 " +  std::to_string(SDT_STRING) + " 0 1 comment \n\
    ";

// a test schema for procection over the tpch lineitem table.
// format: "col_idx col_type col_is_key nullable col_name \n"
// note the col_idx always refers to the index in the table's current schema
const std::string lineitem_test_project_schema_string = " \
    0 " +  std::to_string(SDT_INT64) + " 1 0 orderkey \n\
    1 " +  std::to_string(SDT_INT32) + " 0 1 partkey \n\
    3 " +  std::to_string(SDT_INT64) + " 1 0 linenumber \n\
    4 " +  std::to_string(SDT_FLOAT) + " 0 1 quantity \n\
    5 " +  std::to_string(SDT_DOUBLE) + " 0 1 extendedprice \n\
    ";

// these extract the current data format (flatbuf) into the skyhookdb
// root table and row table data structure defined above, abstracting
// skyhookdb data partitions design from the underlying data format.
sky_root getSkyRoot(const char *fb, size_t fb_size);
sky_rec getSkyRec(const Tables::Row *rec);

// print functions (debug only)
void printSkyRoot(sky_root *r);
void printSkyRec(sky_rec *r);
void printSkyFb(const char* fb, size_t fb_size, schema_vec &schema);

// convert provided schema to/from skyhook internal representation
void schemaFromColNames(schema_vec &ret_schema,
                        schema_vec &current_schema,
                        std::string project_col_names);
void schemaFromString(schema_vec &schema, std::string schema_string);
std::string schemaToString(schema_vec schema);

// convert provided predicates to/from skyhook internal representation
void predsFromString(predicate_vec &preds,  schema_vec &schema,
                     std::string preds_string);
std::string predsToString(predicate_vec &preds,  schema_vec &schema);

bool hasAggPreds(predicate_vec &preds);

// convert provided ops to/from internal skyhook representation (simple enums)
int skyOpTypeFromString(std::string s);
std::string skyOpTypeToString(int op);

// for proj, select, fastpath, aggregations: process data and build return fb
int processSkyFb(
        flatbuffers::FlatBufferBuilder& flatb,
        schema_vec& schema_in,
        schema_vec& schema_out,
        predicate_vec& preds,
        const char* fb,
        const size_t fb_size,
        std::string& errmsg,
        const std::vector<uint32_t>& row_nums=std::vector<uint32_t>());

inline
bool applyPredicates(predicate_vec& pv, sky_rec& rec);

inline
bool compare(const int64_t& val1, const int64_t& val2, const int& op);

inline
bool compare(const uint64_t& val1, const uint64_t& val2, const int& op);

inline
bool compare(const double& val1, const double& val2, const int& op);

inline
bool compare(const bool& val1, const bool& val2, const int& op);

inline
bool compare(const std::string& val1, const re2::RE2& regx, const int& op);

template <typename T>
T computeAgg(const T& val, const T& oldval, const int& op) {

    switch (op) {
        case SOT_min: if (val < oldval) return val;
        case SOT_max: if (val > oldval) return val;
        case SOT_sum: return oldval + val;
        case SOT_cnt: return oldval + 1;
        default: assert (TablesErrCodes::OpNotImplemented);
    }
    return oldval;
}

std::string buildKeyPrefix(
        int data_type,
        std::string schema_name,
        std::string table_name,
        std::vector<string> colnames={IDX_KEY_COLS_DEFAULT});
std::string buildKeyData(int data_type, uint64_t new_data);

} // end namespace Tables

#endif
