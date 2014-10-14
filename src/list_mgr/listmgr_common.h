/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 * Copyright (C) 2008, 2009 CEA/DAM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the CeCILL License.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license (http://www.cecill.info) and that you
 * accept its terms.
 */
#ifndef _LIST_MGR_COMMON_H
#define _LIST_MGR_COMMON_H

#include "list_mgr.h"
#include "listmgr_internal.h"
#include "database.h"
#include <inttypes.h>
#include <glib.h>
#include "status_manager.h"

#define ASSIGN_UNION( _u, _type, _address ) do {            \
                    switch( _type )                         \
                    {                                       \
                      case DB_ID:                           \
                        _u.val_id = *((entry_id_t*)(_address)); \
                        break;                              \
                      case DB_ENUM_FTYPE:                   \
                      case DB_TEXT:                         \
                        _u.val_str = (char*)(_address);     \
                        break;                              \
                      case DB_INT:                          \
                        _u.val_int = *((int*)(_address));   \
                        break;                              \
                      case DB_UINT:                         \
                        _u.val_uint = *((unsigned int*)(_address));   \
                        break;                              \
                      case DB_SHORT:                        \
                        _u.val_short = *((short*)(_address));   \
                        break;                              \
                      case DB_USHORT:                         \
                        _u.val_ushort = *((unsigned short*)(_address));  \
                        break;                              \
                      case DB_BIGINT:                       \
                        _u.val_bigint = *((long long*)(_address));   \
                        break;                              \
                      case DB_BIGUINT:                      \
                        _u.val_biguint = *((unsigned long long*)(_address));  \
                        break;                              \
                      case DB_BOOL:                         \
                        _u.val_bool = *((bool*)(_address));   \
                        break;                              \
                      default:                              \
                        DisplayLog( LVL_CRIT, LISTMGR_TAG, "Unexpected type in ASSIGN_UNION: %d !!!", _type);\
                    }\
                    } while(0)

#define UNION_GET_VALUE( _u, _type, _address ) do {            \
                    switch( _type )                         \
                    {                                       \
                      case DB_ID:                           \
                        *((entry_id_t*)(_address)) = _u.val_id; \
                        break;                              \
                      case DB_ENUM_FTYPE:                   \
                      case DB_TEXT:                         \
                        strcpy( (char*)(_address), _u.val_str ); \
                        break;                              \
                      case DB_INT:                          \
                        *((int*)(_address)) =  _u.val_int ;   \
                        break;                              \
                      case DB_UINT:                         \
                        *((unsigned int*)(_address)) = _u.val_uint;   \
                        break;                              \
                      case DB_SHORT:                          \
                        *((short*)(_address)) =  _u.val_short ;   \
                        break;                              \
                      case DB_USHORT:                       \
                        *((unsigned short*)(_address)) = _u.val_ushort;   \
                        break;                              \
                      case DB_BIGINT:                       \
                        *((long long*)(_address)) = _u.val_bigint ;   \
                        break;                              \
                      case DB_BIGUINT:                      \
                        *((unsigned long long*)(_address)) = _u.val_biguint;  \
                        break;                              \
                      case DB_BOOL:                         \
                        *((bool*)(_address)) = _u.val_bool ;   \
                        break;                              \
                      default:                              \
                        DisplayLog( LVL_CRIT, LISTMGR_TAG, "Unexpected type in UNION_GET_VALUE: %d !!!", _type);\
                    }\
                    } while(0)


#define DIFF_UNION( _diff, _type, _address1, _address2 ) do { \
                    db_type_u _u1, _u2;                     \
                    switch( _type )                         \
                    {                                       \
                      case DB_ID:                           \
                        _u1.val_id = *((entry_id_t*)(_address1)); \
                        _u2.val_id = *((entry_id_t*)(_address2)); \
                        _diff = !entry_id_equal( &_u1.val_id, &_u2.val_id); \
                        break;                              \
                      case DB_ENUM_FTYPE:                   \
                      case DB_TEXT:                         \
                        _u1.val_str = (char*)(_address1);   \
                        _u2.val_str = (char*)(_address2);   \
                        _diff = strcmp(_u1.val_str, _u2.val_str); \
                        break;                              \
                      case DB_INT:                          \
                        _u1.val_int = *((int*)(_address1));   \
                        _u2.val_int = *((int*)(_address2));   \
                        _diff = (_u1.val_int != _u2.val_int); \
                        break;                              \
                      case DB_UINT:                         \
                        _u1.val_uint = *((unsigned int*)(_address1)); \
                        _u2.val_uint = *((unsigned int*)(_address2)); \
                        _diff = (_u1.val_uint != _u2.val_uint); \
                        break;                              \
                      case DB_SHORT:                          \
                        _u1.val_short = *((short*)(_address1));   \
                        _u2.val_short = *((short*)(_address2));   \
                        _diff = (_u1.val_short != _u2.val_short); \
                        break;                               \
                      case DB_USHORT:                        \
                        _u1.val_ushort = *((unsigned short*)(_address1)); \
                        _u2.val_ushort = *((unsigned short*)(_address2)); \
                        _diff = (_u1.val_ushort != _u2.val_ushort); \
                        break;                              \
                      case DB_BIGINT:                       \
                        _u1.val_bigint = *((long long*)(_address1));   \
                        _u2.val_bigint = *((long long*)(_address2));   \
                        _diff = (_u1.val_bigint != _u2.val_bigint); \
                        break;                              \
                      case DB_BIGUINT:                      \
                        _u1.val_biguint = *((unsigned long long*)(_address1));  \
                        _u2.val_biguint = *((unsigned long long*)(_address2));  \
                        _diff = (_u1.val_biguint != _u2.val_biguint); \
                        break;                              \
                      case DB_BOOL:                         \
                        _u1.val_bool = *((bool*)(_address1));   \
                        _u2.val_bool = *((bool*)(_address2));   \
                        _diff = (_u1.val_bool != _u2.val_bool); \
                        break;                              \
                      default:                              \
                        DisplayLog( LVL_CRIT, LISTMGR_TAG, "Unexpected type in ASSIGN_UNION: %d !!!", _type);\
                    }\
                    } while(0)

/* precomputed masks for testing attr sets efficiently */
extern uint64_t  main_attr_set;
extern uint64_t  names_attr_set;
extern uint64_t  annex_attr_set;
extern uint64_t  gen_attr_set;
extern uint64_t  stripe_attr_set;
extern uint64_t  dir_attr_set;
extern uint64_t  slink_attr_set;
extern uint64_t  acct_attr_set;
extern uint64_t  acct_pk_attr_set;
extern uint64_t  softrm_attr_set;

/* extern int     readonly_attr_set; => moved to listmgr.h */

void           init_attrset_masks(const lmgr_config_t *lmgr_config);

/** return the sub mask of main fields in attr_mask */
static inline uint64_t main_fields(uint64_t attr_mask)
{
    return attr_mask & main_attr_set;
}

/** return the sub mask of name fields in attr_mask */
static inline uint64_t names_fields(uint64_t attr_mask)
{
    return attr_mask & names_attr_set;
}

/** return the sub mask of annex fields in attr_mask */
static inline uint64_t annex_fields(uint64_t attr_mask)
{
    return attr_mask & annex_attr_set;
}

/** return the sub mask of generated fields in attr_mask */
static inline uint64_t gen_fields(uint64_t attr_mask)
{
    return attr_mask & gen_attr_set;
}

/** return the sub mask of stripe fields in attr_mask */
static inline uint64_t stripe_fields(uint64_t attr_mask)
{
    return attr_mask & stripe_attr_set;
}

/** return the sub mask of readonly fields in attr_mask */
static inline uint64_t readonly_fields(uint64_t attr_mask)
{
    return attr_mask & readonly_attr_set;
}

/** return the sub mask of directory specific attributes in attr_mask */
static inline uint64_t dirattr_fields(uint64_t attr_mask)
{
    return attr_mask & dir_attr_set;
}

/** return the sub mask of symlink specific attributes in attr_mask */
static inline uint64_t slinkattr_fields(uint64_t attr_mask)
{
    return attr_mask & slink_attr_set;
}

/**
 * indicate if the field is in ACCT_STAT table
 * /!\ Can only be used after init_attrset_masks() has been called
 */
static inline bool is_acct_field(unsigned int attr_index)
{
    return !!((1 << attr_index) & acct_attr_set);
}

/**
 * indicate if the field is part of the ACCT_STAT primary key
 * /!\ Can only be used after init_attrset_masks() has been called
 */
static inline bool is_acct_pk(unsigned int attr_index)
{
    return !!((1 << attr_index) & acct_pk_attr_set);
}

/**
 * indicate if the field is part of the SOFTRM table
 * /!\ Can only be used after init_attrset_masks() has been called
 */
static inline bool is_softrm_field(unsigned int attr_index)
{
    return !!((1 << attr_index) & softrm_attr_set);
}

/* ------------ */

/** indicate if the attribute is a status field */
static inline bool is_status_field(unsigned int attr_index)
{
    return !!((attr_index >= ATTR_COUNT) &&
              (attr_index < ATTR_COUNT + sm_inst_count));
}

/** indicate if the attribute is a status field not stored in DB */
static inline bool is_no_db_status(unsigned int attr_index)
{
    return is_status_field(attr_index) &&
           ((get_sm_instance(attr_index - ATTR_COUNT)->sm->flags & SM_NODB) != 0);
}

/** check if one of the given flags is set for the given field */
static inline bool test_field_flag(unsigned int attr_index, int flags)
{
    return ((field_infos[attr_index].flags & flags) != 0);
}

/** indicate if the field is read only */
static inline bool is_read_only_field(unsigned int attr_index)
{
    return ((attr_index < ATTR_COUNT)
                && test_field_flag(attr_index, GENERATED | DIR_ATTR | REMOVED | FUNC_ATTR))
           || is_no_db_status(attr_index);
}

/** indicate if the field is stripe information */
static inline bool is_stripe_field(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT)
           && ((field_infos[attr_index].db_type == DB_STRIPE_INFO)
               || (field_infos[attr_index].db_type == DB_STRIPE_ITEMS));
}

/** indicate if the field is in NAMES table */
static inline bool is_names_field(unsigned int attr_index)
{
    return ((attr_index < ATTR_COUNT) && test_field_flag(attr_index, DNAMES));
}

/** indicate if the field is in main table */
static inline bool is_main_field(unsigned int attr_index)
{
    return (is_status_field(attr_index) && !is_no_db_status(attr_index))
           || ((attr_index < ATTR_COUNT)
               && test_field_flag(attr_index, FREQ_ACCESS)
               && !is_stripe_field(attr_index)
               && !is_read_only_field(attr_index)
               && !is_names_field(attr_index));
}

static inline bool is_gen_field(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT) && test_field_flag(attr_index, GENERATED);
}

static inline bool is_indexed_field(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT) && test_field_flag(attr_index, INDEXED);
}

static inline bool is_annex_field(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT)
           && test_field_flag(attr_index, ANNEX_INFO | INIT_ONLY)
           && !is_stripe_field(attr_index)
           && !is_read_only_field(attr_index)
           && !is_names_field(attr_index);
}

static inline bool is_funcattr(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT) && test_field_flag(attr_index, FUNC_ATTR);
}

static inline bool is_dirattr(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT) && test_field_flag(attr_index, DIR_ATTR);
}

static inline bool is_slinkattr(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT) && test_field_flag(attr_index, SLINK_ATTR);
}

static inline bool is_sepdlist(unsigned int attr_index)
{
    return (attr_index < ATTR_COUNT)  && test_field_flag(attr_index, SEPD_LIST);
}

static inline bool is_recov_field(unsigned int attr_index)
{
#if 0 /** TODO implement recovery in RBHv3 */
    /* needed fields for disaster recovery */
    return !!((1 << attr_index) & RECOV_ATTR_MASK);
#else
    return false;
#endif
}

/** printing a value to a DB request */
void printdbtype(lmgr_t *p_mgr, GString *str, db_type_t type, const db_type_u *value_ptr);

/** parse a value from DB */
int  parsedbtype(char *instr, db_type_t type, db_type_u *value_out);

typedef enum
{
    T_NONE = 0,                                  /* not set */
    T_MAIN,                                      /* fields in main table */
    T_DNAMES,                                    /* files in dir names table */
    T_ANNEX,                                     /* fiels in annex table */
    T_STRIPE_INFO,                               /* field in stripe info table */
    T_STRIPE_ITEMS,                              /* field in stripe items table */
    T_ACCT,                                      /* fields in accounting table */
    T_SOFTRM,                                    /* fields in softrm table (backup and HSM flavors only) */
    T_RECOV                                      /* fields in recov table (HSM flavors only) */
} table_enum;

static inline const char * table2name(table_enum table)
{
    switch(table)
    {
        case T_NONE: return NULL;
        case T_MAIN: return MAIN_TABLE;
        case T_DNAMES: return DNAMES_TABLE;
        case T_ANNEX: return ANNEX_TABLE;
        case T_STRIPE_INFO: return STRIPE_INFO_TABLE;
        case T_STRIPE_ITEMS: return STRIPE_ITEMS_TABLE;
        case T_ACCT: return ACCT_TABLE;
        case T_SOFTRM: return SOFT_RM_TABLE;
        case T_RECOV: return RECOV_TABLE;
   }
   return NULL;
}

typedef enum {
    ADD,
    SUBSTRACT
} operation_type;

void           add_source_fields_for_gen(uint64_t *attr_mask);
void           generate_fields( attr_set_t * p_set );

int parse_entry_id(lmgr_t *p_mgr, const char *str, PK_PARG_T p_pk, entry_id_t *p_id);

int            attrmask2fieldlist(GString *str, uint64_t attr_mask,
                                  table_enum table, bool leading_comma,
                                  bool for_update, char *prefix, char *postfix);

int            attrmask2fieldcomparison(GString *str, uint64_t attr_mask,
                                  table_enum table, const char *left_prefix,
                                  const char *right_prefix,
                                  const char *comparator, const char *separator);

int            attrmask2fieldoperation(GString *str, uint64_t attr_mask,
                                       table_enum table, const char *prefix,
                                       operation_type operation);

int            attrset2valuelist(lmgr_t *p_mgr, GString *str,
                                 const attr_set_t *p_set, table_enum table,
                                 bool leading_coma);
int            attrset2updatelist(lmgr_t * p_mgr, GString *str,
                                  const attr_set_t * p_set, table_enum table,
                                  bool leading_coma, bool generic_value);

char          *compar2str(filter_comparator_t compar);

int            filter2str(lmgr_t *p_mgr, GString *str, const lmgr_filter_t *p_filter,
                          table_enum table, bool leading_and, bool prefix_table);

int            func_filter(lmgr_t *p_mgr, GString *filter_str, const lmgr_filter_t *p_filter,
                           table_enum table, bool leading_and, bool prefix_table);

struct field_count {
    unsigned int nb_main;
    unsigned int nb_annex;
    unsigned int nb_names;
    unsigned int nb_stripe_info;
    unsigned int nb_stripe_items;
};
int filter_where(lmgr_t *p_mgr, const lmgr_filter_t *p_filter,
                 struct field_count *counts, bool ignore_name_filter,
                 bool leading_and, GString *where);
void filter_from(lmgr_t *p_mgr, const struct field_count *counts,
                 bool ignore_names_filter, GString *from, bool is_first_tab,
                 table_enum *first_table, bool *select_distinct_id);

/* return the number of filter tables */
static inline unsigned int nb_field_tables(const struct field_count *counts)
{
   return (counts->nb_main?1:0) + (counts->nb_annex?1:0)
        + (counts->nb_stripe_info?1:0) + (counts->nb_stripe_items?1:0)
        + (counts->nb_names?1:0);
}



typedef enum
{
    FILTERDIR_NONE = 0,    /* no dir filter */
    FILTERDIR_EMPTY,       /* empty dir filter */
    FILTERDIR_OTHER,       /* other condition on directory attribute */
} filter_dir_e;

filter_dir_e dir_filter(lmgr_t * p_mgr, GString* filter_str,
                        const lmgr_filter_t * p_filter, unsigned int *dir_attr_index);

void append_size_range_fields(GString *str, bool leading_comma,
                              const char *prefix);

int            result2attrset( table_enum table, char **result_tab,
                               unsigned int res_count, attr_set_t * p_set );

/* return the attr string for a dirattr */
const char * dirattr2str(unsigned int attr_index);

void entry_id2pk(const entry_id_t * p_id, PK_PARG_T p_pk);
int pk2entry_id( lmgr_t * p_mgr, PK_ARG_T pk, entry_id_t * p_id );

/* those functions are used for begin/commit/rollback */
int            lmgr_begin( lmgr_t * p_mgr );
void           lmgr_rollback( lmgr_t * p_mgr );
int            lmgr_commit( lmgr_t * p_mgr );

/* to be called before closing a connection */
int            lmgr_flush_commit( lmgr_t * p_mgr );

/** manage delayed retry of retryable errors
 * \return != 0 if the transaction must be restarted
 */
#define lmgr_delayed_retry(_l, _e) _lmgr_delayed_retry(_l, _e, __func__, __LINE__)
int _lmgr_delayed_retry(lmgr_t *lmgr, int errcode, const char *func, int line);

void big_request_in_tx(lmgr_t *p_mgr);
void big_request_now(lmgr_t *p_mgr);

/* get/set variable in DB */
int lmgr_get_var(db_conn_t *pconn, const char *varname, char *value, int bufsize);
int lmgr_set_var(db_conn_t *pconn, const char *varname, const char *value);

int fullpath_attr2db(const char *attr, char *db);
void fullpath_db2attr(const char *db, char *attr);

static inline uint64_t sum_masks(attr_set_t **p_attrs, unsigned int count,
                                 uint64_t t_mask)
{
    uint64_t m = 0;
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        m |= (p_attrs[i]->attr_mask & t_mask);
    }
    return m;
}

void separated_db2list_inplace(char *list);

static inline const char *field_name(int index)
{
    if (index < ATTR_COUNT)
        return field_infos[index].field_name;
    else /* status */
        return get_sm_instance(index - ATTR_COUNT)->db_field;
}

static inline db_type_t field_type(int index)
{
    if (index < ATTR_COUNT)
        return field_infos[index].db_type;
    else /* status */
        return DB_TEXT;
}

/** helper to check empty filter */
static inline bool no_filter(const lmgr_filter_t *p_filter)
{
    return (p_filter == NULL ||
            ((p_filter->filter_type == FILTER_SIMPLE)
             && (p_filter->filter_simple.filter_count == 0)) ||
            ((p_filter->filter_type == FILTER_BOOLEXPR)
             && (p_filter->filter_boolexpr == NULL)));
}

#endif
