/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 * Copyright (C) 2009, 2010 CEA/DAM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the CeCILL License.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license (http://www.cecill.info) and that you
 * accept its terms.
 */

/**
 * Command for retrieving stats about filesystem.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "list_mgr.h"
#include "cmd_helpers.h"
#include "rbh_cfg.h"
#include "rbh_logs.h"
#include "rbh_misc.h"
#include "xplatform_print.h"
#include "Memory.h"
#include "entry_processor.h"

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <string.h>

#define REPORT_TAG    "Report"

#define DEFAULT_TOP_SIZE 20

/* Array of options for getopt_long().
 * Each record consists of: { const char *name, int has_arg, int * flag, int val }
 */

#define OPT_DUMP_USER   256
#define OPT_DUMP_GROUP  257
#define OPT_DUMP_OST    258
#define OPT_DUMP_STATUS 259
#define OPT_CLASS_INFO  260
#define OPT_STATUS_INFO 261

#define SET_NEXT_MAINT    300
#define CLEAR_NEXT_MAINT  301

#define OPT_BY_COUNT      310
#define OPT_BY_AVGSIZE    311
#define OPT_COUNT_MIN     312

#define OPT_TOPRMDIR      320

#define OPT_SIZE_PROFILE  330
#define OPT_BY_SZ_RATIO   331

/* options flags */
#define OPT_FLAG_CSV        0x0001
#define OPT_FLAG_NOHEADER   0x0002
#define OPT_FLAG_GROUP      0x0004

#define OPT_FLAG_NO_ACCT 0x0010
#define OPT_FLAG_SPLITUSERGROUP 0x0020
#define OPT_FLAG_BY_COUNT       0x0040
#define OPT_FLAG_BY_AVGSIZE     0x0080
#define OPT_FLAG_REVERSE        0x0100
#define OPT_FLAG_SPROF          0x0200
#define OPT_FLAG_BY_SZRATIO     0x0400

#define CSV(_x) !!((_x)&OPT_FLAG_CSV)
#define NOHEADER(_x) !!((_x)&OPT_FLAG_NOHEADER)
#define ISGROUP(_x) !!((_x)&OPT_FLAG_GROUP)
#define ISSPLITUSERGROUP(_x) !!((_x)&OPT_FLAG_SPLITUSERGROUP)
#define FORCE_NO_ACCT(_x) !!((_x)&OPT_FLAG_NO_ACCT)
#define SORT_BY_COUNT(_x) !!((_x)&OPT_FLAG_BY_COUNT)
#define SORT_BY_AVGSIZE(_x) !!((_x)&OPT_FLAG_BY_AVGSIZE)
#define SORT_BY_SZRATIO(_x) !!((_x)&OPT_FLAG_BY_SZRATIO)
#define REVERSE(_x) !!((_x)&OPT_FLAG_REVERSE)
#define SPROF(_x) !!((_x)&OPT_FLAG_SPROF)

static profile_field_descr_t size_profile =
{
    .attr_index = ATTR_INDEX_size,
    .range_ratio_start = 0,
    .range_ratio_len = 0,
    .range_ratio_sort = SORT_NONE
};

static struct option option_tab[] = {

    /* Stats selectors */
    {"activity", no_argument, NULL, 'a'},

    {"fsinfo", no_argument, NULL, 'i'},
    {"fs-info", no_argument, NULL, 'i'},

    {"entry-info", required_argument, NULL, 'e'},
    {"entryinfo", required_argument, NULL, 'e'},

    {"userinfo", optional_argument, NULL, 'u'},
    {"user-info", optional_argument, NULL, 'u'},

    {"groupinfo", optional_argument, NULL, 'g'},
    {"group-info", optional_argument, NULL, 'g'},

    {"classinfo", optional_argument, NULL, OPT_CLASS_INFO},
    {"class-info", optional_argument, NULL, OPT_CLASS_INFO},

    {"statusinfo", required_argument, NULL, OPT_STATUS_INFO},
    {"status-info", required_argument, NULL, OPT_STATUS_INFO},

    {"topdirs", optional_argument, NULL, 'd'},
    {"top-dirs", optional_argument, NULL, 'd'},
    {"topsize", optional_argument, NULL, 's'},
    {"top-size", optional_argument, NULL, 's'},
    {"toppurge", optional_argument, NULL, 'p'},
    {"top-purge", optional_argument, NULL, 'p'},
#ifdef HAVE_RMDIR_POLICY
    {"toprmdir", optional_argument, NULL, OPT_TOPRMDIR},
    {"top-rmdir", optional_argument, NULL, OPT_TOPRMDIR},
#endif
    {"topusers", optional_argument, NULL, 'U'},
    {"top-users", optional_argument, NULL, 'U'},
#ifdef HAVE_RM_POLICY
    {"deferred-rm", no_argument, NULL, 'R' },
#endif
    {"dump", no_argument, NULL, 'D' },
    {"dump-all", no_argument, NULL, 'D' }, /* for backward compatibility */
    {"dump-user", required_argument, NULL, OPT_DUMP_USER },
    {"dump-group", required_argument, NULL, OPT_DUMP_GROUP },
#ifdef _LUSTRE
    {"dump-ost", required_argument, NULL, OPT_DUMP_OST },
#endif
    {"dump-status", required_argument, NULL, OPT_DUMP_STATUS },

    {"szprof", no_argument, NULL, OPT_SIZE_PROFILE}, /* size profile */
    {"size-profile", no_argument, NULL, OPT_SIZE_PROFILE},

    /* additional options for topusers etc... */
    {"filter-path", required_argument, NULL, 'P' },
    {"filter-class", required_argument, NULL, 'C' },
// filter status
    {"split-user-groups", no_argument, NULL, 'S'},
    {"by-count", no_argument, NULL, OPT_BY_COUNT},
    {"by-avgsize", no_argument, NULL, OPT_BY_AVGSIZE},
    {"by-avg-size", no_argument, NULL, OPT_BY_AVGSIZE},
    {"by-szratio", required_argument, NULL, OPT_BY_SZ_RATIO},
    {"by-size-ratio", required_argument, NULL, OPT_BY_SZ_RATIO},

    {"count-min", required_argument, NULL, OPT_COUNT_MIN },
    {"reverse", no_argument, NULL, 'r' },

#ifdef HAVE_MIGR_POLICY
    {"next-maintenance", optional_argument, NULL, SET_NEXT_MAINT},
    {"cancel-maintenance", no_argument, NULL, CLEAR_NEXT_MAINT},
#endif

    /* config file options */
    {"config-file", required_argument, NULL, 'f'},

    /* output format option */
    {"csv", no_argument, NULL, 'c'},
    {"no-header", no_argument, NULL, 'q'},

    /* verbosity level */
    {"log-level", required_argument, NULL, 'l'},

    /* miscellaneous options */
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"force-no-acct", no_argument, NULL, 'F'},

    {NULL, 0, NULL, 0}

};

#define SHORT_OPT_STRING    "aiDe:u:g:d:s:p:rU:P:C:Rf:cql:hVFS"

static const char *cmd_help = _B "Usage:" B_ " %s [options]\n";

static const char *stats_help =
    _B "Available stats:" B_ "\n"
    "    " _B "--activity" B_ ", " _B "-a" B_ "\n"
    "        Display stats about daemon activity.\n"
    "    " _B "--fs-info" B_ ", " _B "-i" B_ "\n"
    "        Display statistics about filesystem contents.\n"
    "    " _B "--class-info" B_ "[=" _U "class_expr" U_ "]\n"
    "        Display Fileclasses summary. Use optional parameter " _U "class_expr" U_ "\n"
    "        for retrieving stats about matching fileclasses.\n"
    "    " _B "--status-info" B_ " " _U "status_name" U_ "[:"_U "status_value" U_"]\n"
    "        Display status summary for the given policy or status name.\n"
    "        Optionally filter on "_U"status_value"U_".\n"
    "    " _B "--entry-info"B_ " "_U "path"U_"|"_U"id"U_", "
           _B "-e" B_ " "_U "path"U_"|"_U"id"U_"\n"
    "        Display all information about the given entry.\n"
    "    " _B "--user-info" B_ "[=" _U "username" U_ "], " _B "-u" B_ " " _U "username" U_ "\n"
    "        Display user statistics. Use optional parameter " _U "username" U_ " for retrieving stats about a single user.\n"
    "    " _B "--group-info" B_ "[=" _U "groupname" U_ "], " _B "-g" B_ " " _U "groupname" U_ "\n"
    "        Display group statistics. Use optional parameter " _U "groupname" U_ " for retrieving stats about a single group.\n"
    "    " _B "--top-dirs" B_ "[=" _U "cnt" U_ "], " _B "-d" B_ " " _U "cnt" U_ "\n"
    "        Display largest directories. Optional argument indicates the number of directories to be returned (default: 20).\n"
    "    " _B "--top-size" B_ "[=" _U "cnt" U_ "], " _B "-s" B_ " " _U "cnt" U_ "\n"
    "        Display largest files. Optional argument indicates the number of files to be returned (default: 20).\n"
    "    " _B "--top-purge" B_ "[=" _U "cnt" U_ "], " _B "-p" B_ " " _U "cnt" U_ "\n"
    "        Display oldest entries eligible for purge. Optional argument indicates the number of entries to be returned (default: 20).\n"
#ifdef HAVE_RMDIR_POLICY
    "    " _B "--top-rmdir" B_ "[=" _U "cnt" U_ "]\n"
    "        Display oldest empty directories eligible for rmdir. Optional argument indicates the number of dirs to be returned (default: 20).\n"
#endif
    "    " _B "--top-users" B_ "[=" _U "cnt" U_ "], " _B "-U" B_ " " _U "cnt" U_ "\n"
    "        Display top disk space consumers. Optional argument indicates the number of users to be returned (default: 20).\n"
#ifdef HAVE_RM_POLICY
    "    " _B "--deferred-rm" B_ ", " _B "-R" B_ "\n"
    "        Display files to be removed from HSM.\n"
#endif
    "    "  _B "--dump" B_ ", " _B "-D" B_ "\n"
    "        Dump all filesystem entries.\n"
    "    "  _B "--dump-user" B_ " " _U "username" U_ "\n"
    "        Dump all entries for the given user.\n"
    "    "  _B "--dump-group" B_ " " _U "groupname" U_ "\n"
    "        Dump all entries for the given group.\n"
#ifdef _LUSTRE
    "    "  _B "--dump-ost" B_ " " _U "ost_index" U_ "|" _U "ost_set" U_"\n"
    "        Dump all entries on the given OST or set of OSTs (e.g. 3,5-8).\n"
#endif
    "    "  _B "--dump-status" B_ " " _U "status_name"U_":"_U"status_value" U_ "\n"
    "        Dump all entries with the given status (e.g. lhsm_status:released).\n";

static const char *maintenance_help =
    _B "Maintenance scheduling:" B_ "\n"
    "    " _B "--next-maintenance[="B_ _U"date_time"U_"]\n"
    "        Set/display time of the next maintenance.\n"
    "        Expected "_U"date_time"U_" format is yyyymmddHHMM[SS].\n"
    "    " _B "--cancel-maintenance"B_"\n"
    "        Cancel the next scheduled maintenance.\n";

static const char *filter_help =
    _B "Filter options:" B_ "\n"
    "    The following filters can be specified for reports:\n"
    "    " _B "-P" B_ " " _U "path" U_ ", " _B "--filter-path" B_ " " _U "path" U_ "\n"
    "        Display the report only for objects in the given path.\n"
    "    " _B "-C" B_ " " _U "class_expr" U_ ", " _B "--filter-class" B_ " " _U "class_expr" U_ "\n"
    "        Only report entries in the matching fileclasses.\n"
    "    " _B "--count-min" B_ " "_U"cnt"U_"\n"
    "        Display only topuser/userinfo with at least "_U"cnt"U_" entries\n";

static const char *acct_help =
    _B "Accounting report options:" B_ "\n"
    "    " _B "--size-profile" B_ ", "_B "--szprof" B_ "\n"
    "        Display size profile statistics\n"
    "    " _B "--by-count" B_ "\n"
    "        Sort by count\n"
    "    " _B "--by-avgsize" B_ "\n"
    "        Sort by average file size\n"
    "    " _B "--by-size-ratio" B_ " "_U"range"U_", "_B "--by-szratio"B_" " _U"range"U_"\n"
    "        Sort on the ratio of files in the given size-range\n"
    "        "_U"range"U_": <val><sep><val>- or <val><sep><val-1> or <val><sep>inf\n"
    "           <val>: 0, 1, 32, 1K 32K, 1M, 32M, 1G, 32G, 1T\n"
    "           <sep>: ~ or ..\n"
    "           e.g: 1G..inf, 1..1K-, 0..31M\n"

 /* expected format:
        0
        <start_val><sep>[<end_val>]
            <start_val>: 0, 1, 32, 1K, 32K, 1M, 32M, 1G, 32G, 1T
            <sep>: ~ or ..
            <end_val>: <start_val>- (e.g. "1K-") or <start_val - 1> (e.g. 31K)
                       if no end_val is specified, the range has no upper limit

    examples:
            1G- => 1GB to infinite
            1K..1G- => 1K to 1GB-1
            1K..1023M => 1K to 1GB-1
    */

    "    " _B "--reverse" B_ "\n"
    "        Reverse sort order\n"
    "    " _B "-S" B_ ", " _B "--split-user-groups" B_ "\n"
    "        Display the report by user AND group\n"
    "    " _B "-F" B_ ", " _B "--force-no-acct" B_ "\n"
    "        Generate the report without using accounting table (slower)\n";

static const char *cfg_help =
    _B "Config file options:" B_ "\n"
    "    " _B "-f" B_ " " _U "cfg_file" U_ ", " _B "--config-file=" B_ _U "cfg_file" U_ "\n"
    "        Path to configuration file (or short name).\n";

static const char *output_help =
    _B "Output format options:" B_ "\n"
    "    " _B "-c" B_ " , " _B "--csv" B_ "\n"
    "        Output stats in a csv-like format for parsing\n"
    "    " _B "-q" B_ " , " _B "--no-header" B_ "\n"
    "        Don't display column headers/footers\n";

static const char *misc_help =
    _B "Miscellaneous options:" B_ "\n"
    "    " _B "-l" B_ " " _U "loglevel" U_ ", " _B "--log-level=" B_ _U "loglevel" U_ "\n"
    "        Force the log verbosity level (overides configuration value).\n"
    "        Allowed values: CRIT, MAJOR, EVENT, VERB, DEBUG, FULL.\n"
    "    " _B "-h" B_ ", " _B "--help" B_ "\n"
    "        Display a short help about command line options.\n"
    "    " _B "-V" B_ ", " _B "--version" B_ "\n"
    "        Display version info\n";

static inline void display_help( char *bin_name )
{
    printf(cmd_help, bin_name);
    printf("\n");
    printf(stats_help);
    printf("\n");
    printf(maintenance_help);
    printf("\n");
    printf(filter_help);
    printf("\n");
    printf(acct_help);
    printf("\n");
    printf(cfg_help);
    printf("\n");
    printf(output_help);
    printf("\n");
    printf(misc_help);
}

static inline void display_version( char *bin_name )
{
    printf( "\n" );
    printf( "Product:         " PACKAGE_NAME " reporting tool\n" );
    printf( "Version:         " PACKAGE_VERSION "-"RELEASE"\n" );
    printf( "Build:           " COMPIL_DATE "\n" );
    printf( "\n" );
    printf( "Compilation switches:\n" );

/* purpose of this daemon */
#ifdef _LUSTRE_HSM
    printf( "    Lustre-HSM Policy Engine\n" );
#elif defined(_TMP_FS_MGR)
    printf( "    Temporary filesystem manager\n" );
#elif defined(_HSM_LITE)
    printf( "    Basic HSM binding\n" );
#else
#error "No purpose was specified"
#endif

/* Access by Fid ? */
#ifdef _HAVE_FID
    printf( "    Address entries by FID\n" );
#else
    printf( "    Address entries by path\n" );
#endif

#ifdef HAVE_CHANGELOGS
    printf( "    MDT Changelogs supported\n" );
#else
    printf( "    MDT Changelogs disabled\n" );
#endif

    printf( "\n" );
#ifdef _LUSTRE
#ifdef LUSTRE_VERSION
    printf( "Lustre Version: " LUSTRE_VERSION "\n" );
#else
    printf( "Lustre FS support\n" );
#endif
#else
    printf( "No Lustre support\n" );
#endif

#ifdef _MYSQL
    printf( "Database binding: MySQL\n" );
#elif defined(_SQLITE)
    printf( "Database binding: SQLite\n" );
#else
#error "No database was specified"
#endif
    printf( "\n" );
    printf( "Report bugs to: <" PACKAGE_BUGREPORT ">\n" );
    printf( "\n" );
}

static lmgr_t  lmgr;

/* global filter variables */
char path_filter[RBH_PATH_MAX] = "";
char class_filter[1024] = "";
unsigned int count_min = 0;

/**
 * @param exact exact range value expected
 * @return index of the range it matches
 * @retval -1 on error
 */
static int szrange_val2index(uint64_t val, bool exact)
{
    int i;
    if (exact) /* search exact value */
    {
        for (i=0; i<SZ_PROFIL_COUNT; i++)
            if (val == SZ_MIN_BY_INDEX(i))
                return i;
    }
    else /* search val-1:  eg 1023M for 1G-, 31M for 32M- */
    {
        i=0;
        while (val > SZ_MIN_BY_INDEX(i))
        {
            if (i < SZ_PROFIL_COUNT-1)
            {
                if (val < SZ_MIN_BY_INDEX(i+1))
                {
                    return i;
                }
            }
            else
            {
                /* matches the last */
                return SZ_PROFIL_COUNT-1;
            }
            i++;
        }
    }
    /* not found */
    return -1;
}

#define EXPECTED_SZ_RANGES "0, 1, 32, 1K, 32K, 1M, 32M, 1G, 32G, 1T"
static int parse_size_range(const char * str, profile_field_descr_t * p_profile)
{
    char argcp[1024];
    char *beg = NULL;
    char *end = NULL;
    char *sep = NULL;
    uint64_t sz1;
    uint64_t sz2;
   /* expected format:
        0
        <start_val><sep>[<end_val>]
            <start_val>: 0, 1, 32, 1K, 32K, 1M, 32M, 1G, 32G, 1T
            <sep>: ~ or ..
            <end_val>: <start_val>- (e.g. "1K-") or <start_val - 1> (e.g. 31K)
                       if no end_val is specified, the range has no upper limit

    examples:
            1G- => 1GB to infinite
            1K..1G- => 1K to 1GB-1
            1K..1023M => 1K to 1GB-1
    */
    strcpy(argcp, str);
    /* is there a separator? */
    if ((sep = strchr(argcp, '~')))
    {
        *sep = '\0';
        beg=argcp;
        end=sep+1;
    }
    else if ((sep = strstr(argcp, "..")))
    {
        *sep = '\0';
        beg=argcp;
        end=sep+2;
    }
    else /* single value? */
    {
        beg=argcp;
        end=NULL;
    }

    /* parse first value */
    sz1 = str2size(beg);
    if (sz1 == (uint64_t)-1LL)
    {
        fprintf(stderr, "Invalid argument: '%s' is not a valid size format\n",
                                           beg );
        return -EINVAL;
    }
    if (end == NULL || !strcmp(end, "0"))
    {
        /* size value range: only 0 allowed */
        if (sz1 != 0LL)
        {
            fprintf(stderr, "Only 0 is allowed for single value range (%s not allowed)\n", beg);
            return -EINVAL;
        }
        p_profile->range_ratio_start = 0;
        p_profile->range_ratio_len = 1;
        /* sort order is determined later */
        return 0;
    }

    p_profile->range_ratio_start = szrange_val2index(sz1, true);
    if (p_profile->range_ratio_start == (unsigned int)-1)
    {
        fprintf(stderr, "Invalid argument: %s is not a valid range start. Allowed values: "EXPECTED_SZ_RANGES"\n", beg);
        return -EINVAL;
    }

    /* to the infinite ? */
    if (end[0] == '\0' || !strcasecmp(end, "inf"))
    {
        if (p_profile->range_ratio_start >= SZ_PROFIL_COUNT)
        {
            fprintf(stderr, "Error: range end < range start\n");
            return -EINVAL;
        }
        p_profile->range_ratio_len = SZ_PROFIL_COUNT - p_profile->range_ratio_start;
        return 0;
    }

    /* is second value ends with a '-' ? */
    if (end[strlen(end)-1] == '-')
    {
        int end_idx;
        /* exact match */
        end[strlen(end)-1] = '\0';
        sz2=str2size(end);
        if (sz2 == (uint64_t)-1LL)
        {
            fprintf(stderr, "Invalid argument: '%s' is not a valid size format\n",
                                               end );
            return -EINVAL;
        }
        end_idx = szrange_val2index(sz2, true); /* actually the upper index */
        if (end_idx <= 0)
        {
            fprintf(stderr, "Invalid argument: %s is not a valid range end. Allowed values: "EXPECTED_SZ_RANGES"\n", end);
            return -EINVAL;
        }
        if (p_profile->range_ratio_start >= end_idx)
        {
            fprintf(stderr, "Error: range end < range start\n");
            return -EINVAL;
        }
        p_profile->range_ratio_len = end_idx - p_profile->range_ratio_start;
        return 0;
    }
    else
    {
        int end_idx;
        sz2=str2size(end);
        if (sz2 == (uint64_t)-1LL)
        {
            fprintf(stderr, "Invalid argument: '%s' is not a valid size format\n",
                                               end );
            return -EINVAL;
        }
        end_idx = szrange_val2index(sz2, false);
        if (end_idx < 0)
        {
            fprintf(stderr, "Invalid argument: %s is not a valid range end: terminate it with '-'\n", end);
            return -EINVAL;
        }
        if (p_profile->range_ratio_start > end_idx)
        {
            fprintf(stderr, "Error: range end < range start\n");
            return -EINVAL;
        }
        p_profile->range_ratio_len = end_idx - p_profile->range_ratio_start + 1;
        return 0;
    }

    return -1;
}



/**
 *  Read variable from DB and allocate value.
 **/
static int getvar_helper(lmgr_t *p_mgr, const char *varname, char *value, int size)
{
    int rc;

    rc = ListMgr_GetVar(&lmgr, varname, value, size);
    if ( rc == DB_SUCCESS )
        return 0;
    else if ( rc == DB_NOT_EXISTS )
    {
        strcpy(value,"unknown");
        DisplayLog( LVL_VERB, REPORT_TAG, "WARNING variable %s not in database", varname );
        return rc;
    }
    else
    {
        strcpy(value,"error");
        DisplayLog( LVL_CRIT, REPORT_TAG, "ERROR %d retrieving variable %s from database", rc, varname );
        return rc;
    }
}

static void report_activity(int flags)
{
    char           value[1024];
    time_t         timestamp;
    time_t         timestamp2;
    char           date[128];
    struct tm      t;
    int            rc;
    char           scan_status[128];
    int            nb_threads;


    if (!CSV(flags))
        printf("\nFilesystem scan activity:\n\n");

#ifndef _LUSTRE_HSM
    /* Scan interval */
    if (getvar_helper(&lmgr, SCAN_INTERVAL_VAR, value) == 0)
    {
        timestamp = str2int(value);
        FormatDurationFloat(date, 128, timestamp);
        if (CSV(flags))
            printf("current_scan_interval, %s\n", date);
        else
            printf("    Current scan interval:   %s\n\n", date);
    }
#endif

    /* Previous FS scan */

    if (getvar_helper(&lmgr, PREV_SCAN_START_TIME, value, sizeof(value)) == 0)
    {
        timestamp = str2int(value);
        if (timestamp >= 0) {
            strftime(date, 128, "%Y/%m/%d %T", localtime_r(&timestamp, &t));
            if (!CSV(flags))
            {
                printf("    Previous filesystem scan:\n");
                printf("            start:           %s\n", date);
            }
            else
                printf("previous_scan_start, %s\n", date);

            if (getvar_helper(&lmgr, PREV_SCAN_END_TIME, value, sizeof(value)) == 0)
            {
                timestamp2 = str2int(value);
                if (timestamp2 >= timestamp)
                {
                    int dur = (int)difftime(timestamp2, timestamp);
                    if (!CSV(flags))
                    {
                        FormatDuration(value, 1024, dur);
                        printf("            duration:        %s\n\n", value);
                    }
                    else
                        printf("previous_scan_duration, %i sec\n", dur);
                }
            }
        }
    }

    /* Last FS scan */

    // status
    rc = getvar_helper(&lmgr, LAST_SCAN_STATUS, scan_status, sizeof(scan_status));

    if (rc == 0)
    {
        if (!CSV(flags))
        {
            printf("    Last filesystem scan:\n");
            printf("            status:          %s\n", scan_status);
        }
        else
            printf("last_scan_status, %s\n", scan_status);
    }
    else if (rc == DB_NOT_EXISTS)
    {
        if (CSV(flags))
            printf("last_scan_status, no scan done\n");
        else
            printf("    Filesystem has never been scanned\n");
    }

    // start
    if (getvar_helper(&lmgr, LAST_SCAN_START_TIME, value, sizeof(value)) == 0)
        timestamp = str2int(value);
    else
        timestamp = -1;

    if (timestamp > 0)
    {
        strftime(date, 128, "%Y/%m/%d %T", localtime_r(&timestamp, &t));
        if (CSV(flags))
            printf("last_scan_start, %s\n", date);
        else {
            int ago = difftime(time(NULL), timestamp);
            if (!strcmp(scan_status, SCAN_STATUS_RUNNING))
            {
                FormatDuration(value, 1024, ago);
                printf("            start:           %s (%s ago)\n", date, value);
            }
            else
                printf("            start:           %s\n", date);
        }
    }

    // last action
    if(!strcmp(scan_status, SCAN_STATUS_RUNNING) &&
       getvar_helper(&lmgr, LAST_SCAN_LAST_ACTION_TIME, value, sizeof(value)) == 0)
    {
        timestamp2 = str2int(value);
        if (timestamp2 > 0)
        {
            strftime(date, 128, "%Y/%m/%d %T", localtime_r(&timestamp2, &t));
            if (CSV(flags))
                printf("last_action_time, %s\n", date);
            else {
                int ago = difftime(time(NULL), timestamp2);
                if (!strcmp(scan_status, SCAN_STATUS_RUNNING))
                {
                    FormatDuration(value, 1024, ago);
                    printf("            last action:     %s (%s ago)\n", date, value);
                }
                else
                    printf("            last action:     %s\n", date);
            }
        }
    }

    // end
    if (getvar_helper(&lmgr, LAST_SCAN_END_TIME, value, sizeof(value)) == 0)
    {
        timestamp2 = str2int(value);
        if (timestamp2 >= timestamp)
        {
            strftime(date, 128, "%Y/%m/%d %T", localtime_r(&timestamp2, &t));
            if (CSV(flags))
                printf("last_scan_end, %s\n", date);
            else
                printf("            end:             %s\n", date);

            // duration
            if (timestamp > 0)
            {
                int dur = (int)difftime(timestamp2, timestamp);
                if (CSV(flags))
                    printf("last_scan_duration, %i sec\n", dur);
                else
                {
                    FormatDuration(value, 1024, dur);
                    printf("            duration:        %s\n", value);
                }
            }
        }
    }

    rc = getvar_helper(&lmgr, LAST_SCAN_ENTRIES_SCANNED, value, sizeof(value));
    if (rc == 0)
    {
        // entries scanned
        if (!CSV(flags))
        {
            printf("\n");
            printf("         Statistics:\n");
        }
        if (CSV(flags))
            printf("entries_scanned, %s\n", value);
        else
            printf("            entries scanned: %s\n", value);

        // errors
        getvar_helper(&lmgr, LAST_SCAN_ERRORS, value, sizeof(value));
        if (CSV(flags))
            printf("scan_errors, %s\n", value);
        else
            printf("            errors:          %s\n", value);

        // timeouts
        getvar_helper(&lmgr, LAST_SCAN_TIMEOUTS, value, sizeof(value));
        if (CSV(flags))
            printf("scan_timeouts, %s\n", value);
        else
            printf("            timeouts:        %s\n", value);

        // nb threads
        getvar_helper(&lmgr, LAST_SCAN_NB_THREADS, value, sizeof(value));
        nb_threads = atoi(value);
        if (CSV(flags))
            printf("scan_nb_threads, %i\n", nb_threads);
        else
            printf("            # threads:       %i\n", nb_threads);

        // average speed
        getvar_helper(&lmgr, LAST_SCAN_AVGMSPE, value, sizeof(value));
        double speed = 0.0;
        double avgmspe = atof(value);
        if (avgmspe > 0)
            speed = (1000.0 / avgmspe) * nb_threads;
        if (CSV(flags))
            printf("scan_average_speed, %.2f entries/sec\n", speed);
        else
            printf("            average speed:   %.2f entries/sec\n", speed);

        // current speed
        if (!strcmp(scan_status, SCAN_STATUS_RUNNING))
        {
            getvar_helper(&lmgr, LAST_SCAN_CURMSPE, value, sizeof(value));
            double speed = 0.0;
            double curmspe = atof(value);
            if (curmspe > 0.0)
                speed = (1000.0 / curmspe) * nb_threads;
            if (CSV(flags))
                printf("scan_current_speed, %.2f\n", speed);
            else
                printf("        >>> current speed:   %.2f entries/sec\n", speed);
        }
    }

    if (!CSV(flags))
        printf("\n");

#ifdef HAVE_CHANGELOGS
    /* changelog stats */
    rc = ListMgr_GetVar(&lmgr, CL_LAST_READ_REC_ID, value, sizeof(value));
    if (rc == DB_SUCCESS)
    {
        int i;
        unsigned int interval;

        if (CSV(flags))
            printf("changelog_last_record_id, %s\n", value);
        else
        {
            printf("\nChangelog stats:\n\n");
            printf("        Last read record id:      %s\n", value);
        }

        if (ListMgr_GetVar(&lmgr, CL_LAST_READ_REC_TIME, value, sizeof(value)) == DB_SUCCESS)
        {
            if (CSV(flags))
                printf("changelog_last_record_time, %s\n", value);
            else
                printf("        Last read record time:    %s\n", value);
        }

        if (ListMgr_GetVar(&lmgr, CL_LAST_READ_TIME, value, sizeof(value)) == DB_SUCCESS)
        {
            if (CSV(flags))
                printf("changelog_cl_recv_time, %s\n", value);
            else
                printf("        Last receive time:        %s\n", value);
        }

        if (ListMgr_GetVar(&lmgr, CL_LAST_COMMITTED, value, sizeof(value)) == DB_SUCCESS)
        {
            if (CSV(flags))
                printf("changelog_last_committed_id, %s\n", value);
            else
                printf("        Last committed record id: %s\n", value);
        }

        if (!CSV(flags))
        {
            printf("        Changelog stats:\n");
            printf("                %5s  %15s \t(%s)\t(%s)\n", "type",
                   "total", "diff", "rate");
        }
        else
            printf("%11s, %12s, %8s, %s\n",
                   "record_type", "total", "diff", "rate (ops/sec)");

        /* get diff interval */
        if (ListMgr_GetVar(&lmgr, CL_DIFF_INTERVAL, value, sizeof(value)) != DB_SUCCESS)
            interval = 0;
        else
            interval = str2int(value);

        for (i = 0; i < CL_LAST; i++)
        {
            char varname[256];
            char varname2[256];
            char diff_str[256];
            unsigned long long diff;
            double rate;

            sprintf(varname, "%s_%s", CL_COUNT_PREFIX, changelog_type2str(i));
            sprintf(varname2, "%s_%s", CL_DIFF_PREFIX, changelog_type2str(i));

            rc = ListMgr_GetVar(&lmgr, varname, value, sizeof(value));
            if (rc == DB_NOT_EXISTS)
                strcpy(value, "0");
            else if (rc != 0)
                strcpy(value, "db_error");

            if ((interval > 0) && (ListMgr_GetVar(&lmgr, varname2, diff_str, sizeof(value)) == DB_SUCCESS))
            {
                diff = str2bigint(diff_str);
                rate = (0.0+diff)/(0.0+interval);
            }
            else
            {
                diff = 0;
                rate = 0.0;
            }

            if (CSV(flags))
                printf("%11s, %12s, %8llu, %8.2f\n",  changelog_type2str(i), value,
                       diff, rate);
            else if (diff != 0)
                printf("                %5s: %15s \t(+%llu)\t(%.2f/sec)\n",
                       changelog_type2str(i), value, diff, rate);
            else
                printf("                %5s: %15s\n", changelog_type2str(i), value);

        }

        if (!CSV(flags))
            printf("\n");
    }
#endif

    /* max usage */
    rc = ListMgr_GetVar(&lmgr, USAGE_MAX_VAR, value, sizeof(value));
    if (rc == DB_SUCCESS)
    {
        if (CSV(flags))
            printf("usage_max, %s\n", value);
        else
            printf("Storage unit usage max:   %s%%\n", value);
    }
    else if (rc == DB_NOT_EXISTS)
    {
        if (CSV(flags))
            printf("usage_max, not checked\n");
        else
            printf("Storage usage has never been checked\n");
    }
    else
    {
        DisplayLog(LVL_CRIT, REPORT_TAG,
                   "ERROR retrieving variable " USAGE_MAX_VAR " from database");
    }

#if 0 /* FIXME: adapt to generic policies */
#ifdef HAVE_MIGR_POLICY
    if ( ! CSV(flags) )
        printf( "\n" );

    /* Last migration */
    rc = ListMgr_GetVar( &lmgr, LAST_MIGR_TIME, value );
    if ( rc == DB_SUCCESS )
    {
        timestamp = atoi( value );
        strftime( date, 128, "%Y/%m/%d %T", localtime_r( &timestamp, &t ) );
        if (  CSV(flags) )
            printf( "last_migration_time, %s\n", date );
        else
            printf( "Last migration:           %s\n", date );
    }
    else if ( rc == DB_NOT_EXISTS )
    {
        if ( CSV(flags) )
            printf( "last_migration_time, unknown\n" );
        else
            printf( "No migration was performed on this filesystem\n\n" );
        return;
    }
    else
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR retrieving variable " LAST_MIGR_TIME " from database" );
        return;
    }

    if ( ListMgr_GetVar( &lmgr, LAST_MIGR_STATUS, value ) == DB_SUCCESS )
    {
        if ( CSV(flags) )
            printf( "last_migration_status, %s\n", value );
        else
            printf( "    Status:               %s\n", value );
    }

    if ( ListMgr_GetVar( &lmgr, LAST_MIGR_INFO, value ) == DB_SUCCESS )
    {
        if ( CSV(flags) )
            printf( "last_migration_info, %s\n", value );
        else
            printf( "    Migration info:       %s\n", value );
    }

#endif

    if ( ! CSV(flags) )
        printf( "\n" );

    /* Last purge */
    rc = ListMgr_GetVar( &lmgr, LAST_PURGE_TIME, value );
    if ( rc == DB_SUCCESS )
    {
        timestamp = atoi( value );
        strftime( date, 128, "%Y/%m/%d %T", localtime_r( &timestamp, &t ) );
        if ( CSV(flags) )
            printf( "last_purge_time, %s\n", date );
        else
            printf( "Last purge:               %s\n", date );
    }
    else if ( rc == DB_NOT_EXISTS )
    {
        if ( CSV(flags) )
            printf( "last_purge_time, unknown\n" );
        else
            printf( "No purge was performed on this filesystem\n\n" );
        return;
    }
    else
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR retrieving variable " LAST_PURGE_TIME " from database" );
        return;
    }

    if ( ListMgr_GetVar( &lmgr, LAST_PURGE_TARGET, value ) == DB_SUCCESS )
    {
        if ( CSV(flags) )
            printf( "last_purge_target, %s\n", value );
        else
            printf( "    Target:               %s\n", value );
    }

    if ( ListMgr_GetVar( &lmgr, LAST_PURGE_STATUS, value ) == DB_SUCCESS )
    {
        if ( CSV(flags) )
            printf( "last_purge_status, %s\n", value );
        else
            printf( "    Status:               %s\n", value );
    }
    if ( !CSV(flags) )
        printf( "\n" );
#endif

}


typedef enum {DUMP_ALL, DUMP_USR, DUMP_GROUP, DUMP_OST, DUMP_STATUS } type_dump;

/*
 * Append global filters on path, class...
 * \param do_display [in] display filters?
 * \param initialized [in/out] indicate if the filter is initialized.
 */
static int mk_global_filters(lmgr_filter_t *filter, bool do_display,
                             bool *initialized)
{
    filter_value_t fv;
    char path_regexp[RBH_PATH_MAX] = "";
    size_t  len;

    /* is a filter on path specified? */
    if ( !EMPTY_STRING( path_filter ) )
    {
        if ( (initialized != NULL) && !(*initialized) )
        {
            lmgr_simple_filter_init( filter );
            *initialized = true;
        }
        if ( do_display )
            printf("filter path: %s\n", path_filter );

        len = strlen(path_filter);
        if ( path_filter[len-1] != '/' )
        {
            /* ( fullpath LIKE 'path' OR fullpath LIKE 'path/%' ) */
            fv.value.val_str = path_filter;
            lmgr_simple_filter_add( filter, ATTR_INDEX_fullpath, LIKE, fv,
                                    FILTER_FLAG_BEGIN );

            snprintf( path_regexp, RBH_PATH_MAX, "%s/*", path_filter );
            fv.value.val_str = path_regexp;
            lmgr_simple_filter_add( filter, ATTR_INDEX_fullpath, LIKE, fv,
                                    FILTER_FLAG_OR | FILTER_FLAG_END );
        }
        else /* ends with slash */
        {
            snprintf( path_regexp, RBH_PATH_MAX, "%s*", path_filter );
            /* directory or directory/% */

            fv.value.val_str = path_regexp;
            lmgr_simple_filter_add( filter, ATTR_INDEX_fullpath, LIKE, fv,
                                    FILTER_FLAG_BEGIN );
            /* remove last slash */
            path_filter[len-1] = '\0';
            fv.value.val_str = path_filter;
            lmgr_simple_filter_add( filter, ATTR_INDEX_fullpath, LIKE, fv,
                                    FILTER_FLAG_OR | FILTER_FLAG_END );
        }

    }

    if ( !EMPTY_STRING( class_filter ) )
    {
        if ( (initialized != NULL) && !(*initialized) )
        {
            lmgr_simple_filter_init( filter );
            *initialized = true;
        }
        if ( do_display )
            printf("filter class: %s\n", class_format(class_filter) );

        fv.value.val_str = class_filter;

        /* list manager as a specific fileclass management,
         * as fileclass attr may be a list of fileclasses */
        lmgr_simple_filter_add(filter, ATTR_INDEX_fileclass, LIKE, fv, 0);
    }

    return 0;
}

static uint64_t list2mask(int *attr_list, int attr_count)
{
    int i;
    uint64_t mask, tmpmask;
    mask = 0;
    for (i=0; i < attr_count; i++)
    {
        tmpmask = 1 << attr_list[i];
        mask |= tmpmask;
    }
    return mask;
}

/**
 * Manage fid2path resolution
 */
static int TryId2path(lmgr_t *p_mgr, const entry_id_t *p_id,  char *path)
{
    static int is_init = 0;
    static int is_resolvable = 0;
    int rc;
    char value[1024];

    if (!is_init) {
        is_init = 1;
        /* try to get fspath from DB */
        rc = ListMgr_GetVar(&lmgr, FS_PATH_VAR, value, sizeof(value));
        if (rc)
            return -1;

        if (InitFS() == 0)
            is_resolvable = 1;
        else
            return -1;
    }
    if (!is_resolvable)
        return -1;

#ifdef _HAVE_FID
    /* filesystem is mounted and fsname can be get: solve the fid */
    rc = Lustre_GetFullPath(p_id, path, RBH_PATH_MAX);
    return rc;
#else
    entry_id_t root_id;
    if (Path2Id(global_config.fs_path, &root_id) == 0)
    {
        if (entry_id_equal(p_id, &root_id))
        {
            strcpy(path, global_config.fs_path);
            return 0;
        }
    }
    return -1;
#endif
}

static const char *ResolvName(const entry_id_t *p_id, attr_set_t *attrs,
                               char *buff)
{
    if (ATTR_MASK_TEST(attrs, fullpath))
    {
        return  ATTR(attrs, fullpath);
    }
    /* try to get dir path from fid if it's mounted */
    else if (TryId2path(&lmgr, p_id, ATTR(attrs, fullpath)) == 0)
    {
        struct stat st;
        ATTR_MASK_SET(attrs, fullpath);

        /* we're lucky, try lstat now! */
        if (lstat(ATTR(attrs, fullpath), &st) == 0)
            PosixStat2EntryAttr(&st, attrs, true);
        return ATTR(attrs, fullpath);
    }
    /* if parent id and name are set: try to resolve parent */
    else if (ATTR_MASK_TEST(attrs, parent_id) && ATTR_MASK_TEST(attrs, name))
    {
        char tmpstr[RBH_PATH_MAX];
        if (TryId2path(&lmgr, &ATTR(attrs, parent_id), tmpstr) == 0)
        {
            snprintf(ATTR(attrs, fullpath), RBH_PATH_MAX, "%s/%s", tmpstr, ATTR(attrs, name));
            return ATTR(attrs, fullpath);
        }
        else /* print <parent_id>/name */
        {
            sprintf(buff, DFID"/%s", PFID(&ATTR(attrs, parent_id)), ATTR(attrs, name));
            return buff;
        }
    }
    else
    {
        /* last case: display the raw ID */
        sprintf(buff, DFID, PFID(p_id));
        return buff;
    }
}

static void dump_entries( type_dump type, int int_arg, char * str_arg, value_list_t * ost_list, int flags )
{
    /* get basic information */
    uint64_t       mask_sav;
    int rc;
    lmgr_filter_t  filter;
    filter_value_t fv;
    struct lmgr_iterator_t *it;
    attr_set_t     attrs;
    entry_id_t     id;
    int custom_len = 0;

    unsigned long long total_size, total_count;
    total_size = total_count = 0;

    /* list of attributes to be use for all dumps
     * except ost dump and status dump */
    static int list_std[] = {
                   ATTR_INDEX_type,
                   ATTR_INDEX_size,
                   ATTR_INDEX_owner,
                   ATTR_INDEX_gr_name,
                   ATTR_INDEX_fileclass
                };

    static int list_status[] = {
                   ATTR_INDEX_type,
                   0, /* to be set in the code */
                   ATTR_INDEX_size,
                   ATTR_INDEX_owner,
                   ATTR_INDEX_gr_name,
                   ATTR_INDEX_fileclass,
                   ATTR_INDEX_fullpath,
                };

    /* list of attributes to be used for OST dumps */
    static int list_stripe[] = {
                   ATTR_INDEX_type,
                   ATTR_INDEX_size,
                   ATTR_INDEX_fullpath,
                   ATTR_INDEX_stripe_info,
                   ATTR_INDEX_stripe_items
    };
    int * list = NULL;
    int list_cnt = 0;

    if (type == DUMP_OST)
    {
        list = list_stripe;
        list_cnt = sizeof(list_stripe)/sizeof(int);
    }
    else if (type == DUMP_STATUS)
    {
        list = list_status;
        list_cnt = sizeof(list_status)/sizeof(int);
        list[1] = ATTR_COUNT + int_arg; /* status index */
    }
    else /* std dump: display all status */
    {
        int i;

        list_cnt = sizeof(list_std)/sizeof(int);
        /* add all policy status (except for removed entries) + 1 for fullpath (always last) */
        list = calloc(list_cnt + sm_inst_count + 1, sizeof(int));
        if (list == NULL)
            exit(ENOMEM);

        memcpy(list, list_std, sizeof(list_std));
        for (i = 0; i < sm_inst_count; i++)
        {
            if ((get_sm_instance(i)->sm->flags & SM_NODB) == 0)
            {
                list[list_cnt] = i + ATTR_COUNT;
                list_cnt++;
            }
        }
        /* add fullpath */
        list[list_cnt] = ATTR_INDEX_fullpath;
        list_cnt ++; /* +1 for fullpath */
    }

    lmgr_simple_filter_init( &filter );

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), NULL );

    /* what do we dump? */
    switch( type )
    {
        case DUMP_ALL:
            /* no filter */
            break;
        case DUMP_USR:
            fv.value.val_str = str_arg;
            lmgr_simple_filter_add( &filter, ATTR_INDEX_owner, LIKE, fv, 0 );
            break;
        case DUMP_GROUP:
            fv.value.val_str = str_arg;
            lmgr_simple_filter_add( &filter, ATTR_INDEX_gr_name, LIKE, fv, 0 );
            break;
        case DUMP_OST:
            if (ost_list->count == 1)
            {
                fv.value.val_uint = ost_list->values[0].val_uint;
                lmgr_simple_filter_add( &filter, ATTR_INDEX_stripe_items, EQUAL, fv, 0 );
            }
            else
            {
                fv.list = *ost_list;
                lmgr_simple_filter_add( &filter, ATTR_INDEX_stripe_items, IN, fv, 0 );
            }
            break;

       case DUMP_STATUS:
                /* int arg: smi index */
                /* str arg: status value */

                fv.value.val_str = str_arg;
                lmgr_simple_filter_add(&filter, int_arg + ATTR_COUNT, EQUAL, fv,
                                       EMPTY_STRING(str_arg)?FILTER_FLAG_ALLOW_NULL:0);
                break;

        default:
            DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: unexpected dump command" );
            return;
    }

    /* attributes to be retrieved */
    ATTR_MASK_INIT( &attrs );
    mask_sav = attrs.attr_mask = list2mask(list, list_cnt);

    it = ListMgr_Iterator( &lmgr, &filter, NULL, NULL );

    lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not dump entries from database." );
        return;
    }

    if (!(NOHEADER(flags)))
    {
        if (type != DUMP_OST)
            print_attr_list(0, list, list_cnt, NULL, CSV(flags));
        else
        {
            char tmp[128];
            if (ost_list->count == 1)
                sprintf(tmp, "data_on_ost%u", ost_list->values[0].val_uint);
            else
                sprintf(tmp, "data_on_ost[%s]", str_arg);

            custom_len = strlen(tmp);
            /* if dump_ost is specified: add specific field
             * to indicate if file really has data on the given OST
             */
            print_attr_list_custom(0, list, list_cnt, NULL, CSV(flags), tmp,
                                   custom_len);
        }
    }

    while ( ( rc = ListMgr_GetNext( it, &id, &attrs ) ) == DB_SUCCESS )
    {
        total_count ++ ;
        total_size += ATTR( &attrs, size );

        if (type != DUMP_OST)
            print_attr_values(0, list, list_cnt, &attrs, &id,
                              CSV(flags), NULL);
#ifdef _LUSTRE
        else
        {
            const char * has_data;
#if defined(ATTR_INDEX_status) && defined(HAVE_PURGE_POLICY)
            /* no data if the file is released */
            if (ATTR_MASK_TEST(&attrs, status) && ATTR(&attrs, status) == STATUS_RELEASED)
                has_data = "no";
            else
#endif

            if (!ATTR_MASK_TEST(&attrs, size) || !ATTR_MASK_TEST(&attrs, stripe_info)
                || !ATTR_MASK_TEST(&attrs, stripe_items))
                has_data = "?";
            else
            {
                int i;
                has_data = "no";
                for (i = 0; i < ost_list->count; i++)
                {
                    if (DataOnOST(ATTR(&attrs, size), ost_list->values[i].val_uint,
                         &ATTR(&attrs,stripe_info), &ATTR(&attrs,stripe_items)))
                    {
                        has_data = "yes";
                        break;
                    }
                }
            }

            /* if dump_ost is specified: add specific field
             * to indicate if file really has data on the given OST.
             */
            print_attr_values_custom(0, list, list_cnt, &attrs, &id,
                              CSV(flags), NULL, has_data, custom_len);
        }
#endif


        ListMgr_FreeAttrs( &attrs );

        /* prepare next call */
        attrs.attr_mask = mask_sav;
    }

    ListMgr_CloseIterator( it );

    /* display summary */
    if ( !NOHEADER(flags) )
    {
        char strsz[128];
        FormatFileSize( strsz, 128, total_size );
        printf("\nTotal: %llu entries, %llu bytes (%s)\n",
               total_count, total_size, strsz);
    }
}

static void report_fs_info( int flags )
{
    unsigned int   result_count;
    struct lmgr_report_t *it;
    int            rc;

    lmgr_filter_t  filter;
    bool is_filter = false;

#define FSINFOCOUNT 6

    db_value_t     result[FSINFOCOUNT];

    /* To be retrieved:
     * - type
     * - number of items for this type
     * - MIN/MAX/SUM size
     * - MIN/MAX/SUM dircount
     */
    report_field_descr_t fs_info[FSINFOCOUNT] = {
        {ATTR_INDEX_type, REPORT_GROUP_BY, SORT_ASC, false, 0, FV_NULL},
        {ATTR_INDEX_COUNT, REPORT_COUNT, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_SUM, SORT_NONE, false, 0, FV_NULL}, /* XXX ifdef STATUS ? */
        {ATTR_INDEX_size, REPORT_MIN, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MAX, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_AVG, SORT_NONE, false, 0, FV_NULL},
    };

    unsigned long long total_size, total_count;
    total_size = total_count = 0;
    lmgr_iter_opt_t opt;
    profile_u   prof;
    bool display_header = !NOHEADER(flags);

    if (REVERSE(flags))
        fs_info[0].sort_flag = SORT_DESC;

    if (count_min) {
        fs_info[1].filter = true;
        fs_info[1].filter_compar = MORETHAN;
        fs_info[1].filter_value.value.val_biguint = count_min;
    }

    /* no limit */
    opt.list_count_max = 0;
    /* skip missing entries */
    opt.allow_no_attr = 0;
    opt.force_no_acct = FORCE_NO_ACCT(flags);

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), &is_filter );

    if ( is_filter )
        it = ListMgr_Report( &lmgr, fs_info, FSINFOCOUNT,
                             SPROF(flags)?&size_profile:NULL, &filter, &opt );
    else
        it = ListMgr_Report( &lmgr, fs_info, FSINFOCOUNT,
                             SPROF(flags)?&size_profile:NULL, NULL, &opt );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve filesystem stats from database." );
        return;
    }


    result_count = FSINFOCOUNT;

    while ((rc = ListMgr_GetNextReportItem(it, result, &result_count,
                                           SPROF(flags)?&prof:NULL))
              == DB_SUCCESS )
    {
        if (result[1].value_u.val_biguint == 0) /* count=0 (don't display)*/
            display_report(fs_info, FSINFOCOUNT, NULL, result_count,
                           SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                           CSV(flags), display_header, 0);
        else
            display_report(fs_info, FSINFOCOUNT, result, result_count,
                           SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                           CSV(flags), display_header, 0);
        display_header = false; /* just display it once */

        total_count += result[1].value_u.val_biguint;
        total_size += result[2].value_u.val_biguint;

        /* prepare next call */
        result_count = FSINFOCOUNT;
    }

    ListMgr_CloseReport( it );

    /* display summary */
    if ( !NOHEADER(flags) )
    {
        char strsz[128];
        FormatFileSize( strsz, 128, total_size );
        printf("\nTotal: %llu entries, %llu bytes (%s)\n",
               total_count, total_size, strsz);
    }
}

static int report_entry(const char *entry, int flags)
{
    bool is_id = true;
    int rc;
    entry_id_t id;
    attr_set_t attrs;

    /* try it as a fid */
    if (sscanf(entry, SFID, RFID(&id)) != FID_SCAN_CNT)
    {
        is_id = false;

        if ((rc = InitFS()) != 0)
            fprintf(stderr, "Warning: cannot access the filesystem to get entry id: %s\n",
                    strerror(-rc));
            /* try to continue anyway */

        /* try it as a path */
        if ((rc = Path2Id(entry, &id)) != 0)
        {
            fprintf(stderr, "Couldn't get id for %s: %s\n,", entry, strerror(-rc));
            return rc;
        }
    }

    /* try to get all attrs */
    attrs.attr_mask = ~0LL;

    if (CSV(flags))
        printf("id, "DFID"\n", PFID(&id));
    else
        printf("%-15s: \t"DFID"\n", "id", PFID(&id));

    if (ListMgr_Get(&lmgr, &id, &attrs) == DB_SUCCESS)
    {
        int mask, i;
        char str[RBH_PATH_MAX];

        for (i = 0, mask = 1; i < ATTR_COUNT + sm_inst_count; i++, mask <<= 1)
        {
            if (mask & attrs.attr_mask)
            {
                if (attrindex2len(i, CSV(flags)) != 1) /* for '?' */
                {
                    if (!CSV(flags))
                        printf("%-15s: \t%s\n", attrindex2name(i),
                               attr2str(&attrs, &id, i, CSV(flags), NULL,
                               str, sizeof(str)));
                    else
                        printf("%s, %s\n", attrindex2name(i),
                               attr2str(&attrs, &id, i, CSV(flags), NULL,
                               str, sizeof(str)));
                }
            }
        }
        return 0;
    }
    else
        return -1;
}

static inline void set_report_rec_nofilter( report_field_descr_t* ent,
                                   int   attr_index,
                                   report_type_t  report_type,
                                   sort_order_t   sort_flag )
{
    ent->attr_index = attr_index;
    ent->report_type = report_type;
    ent->sort_flag = sort_flag;
    ent->filter = false;
    ent->filter_compar = 0;
}

static void report_usergroup_info( char *name, int flags )
{
    unsigned int   result_count;
    struct lmgr_report_t *it;
    lmgr_filter_t  filter;
    filter_value_t fv;
    int            rc;
    unsigned int   field_count = 0;
    unsigned int   shift = 0;
    bool           is_filter = false;
    bool           display_header = !NOHEADER(flags);
    unsigned long long total_size, total_count;
    total_size = total_count = 0;
    lmgr_iter_opt_t opt;

#define USERINFOCOUNT_MAX 9

    db_value_t     result[USERINFOCOUNT_MAX];
    profile_u   prof;

    /* To be retrieved for each user:
     * - username
     * - number of items of each type
     * - SUM(blocks)
     * - MIN/MAX/AVG size
     */
    report_field_descr_t user_info[USERINFOCOUNT_MAX];

    if (ISSPLITUSERGROUP(flags) && ISGROUP(flags))
    {
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_gr_name,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_owner,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
        shift++ ;
    }
    else if (ISSPLITUSERGROUP(flags) && !ISGROUP(flags))
    {
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_owner,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_gr_name,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
        shift++;
    }
    else if (ISGROUP(flags))
    {
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_gr_name,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
    }
    else
    {
        set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_owner,
                                REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
        field_count++;
    }

    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_type,
                            REPORT_GROUP_BY, REVERSE(flags)?SORT_DESC:SORT_ASC );
    field_count++;
    shift++;

    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_COUNT,
                            REPORT_COUNT, SORT_NONE);
    if (count_min) {
        user_info[field_count].filter = true;
        user_info[field_count].filter_compar = MORETHAN;
        user_info[field_count].filter_value.value.val_biguint = count_min;
    }

    field_count++;
#if defined(HAVE_SHOOK) || defined(_LUSTRE_HSM)
    /* for 'release'-capable systems, count sum(size) instead of sum(blocks) that might be zero */
    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_size, REPORT_SUM, SORT_NONE );
//    field_count++;
//    /* split by file status (@TODO split by all policy statuses) */
//    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_status, REPORT_GROUP_BY, SORT_NONE);
#else
    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_blocks, REPORT_SUM, SORT_NONE );
#endif
    field_count++;
    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_size, REPORT_MIN, SORT_NONE );
    field_count++;
    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_size, REPORT_MAX, SORT_NONE );
    field_count++;
    set_report_rec_nofilter(&user_info[field_count], ATTR_INDEX_size, REPORT_AVG, SORT_NONE );
    field_count++;

    opt.force_no_acct = FORCE_NO_ACCT(flags);

    /* no limit */
    opt.list_count_max = 0;
    /* skip missing entries */
    opt.allow_no_attr = false;

    if ( name )
    {
        lmgr_simple_filter_init( &filter );
        is_filter = true;

        fv.value.val_str = name;

        if ( WILDCARDS_IN( name ) )
            lmgr_simple_filter_add( &filter, (ISGROUP(flags)?ATTR_INDEX_gr_name:ATTR_INDEX_owner), LIKE, fv, 0 );
        else
            lmgr_simple_filter_add( &filter, (ISGROUP(flags)?ATTR_INDEX_gr_name:ATTR_INDEX_owner), EQUAL, fv, 0 );
    }

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), &is_filter );

    it = ListMgr_Report( &lmgr, user_info, field_count,
                         SPROF(flags)?&size_profile:NULL,
                         is_filter ? &filter : NULL, &opt );

    if ( is_filter )
        lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG, "ERROR: Could not retrieve user stats from database." );
        return;
    }

    result_count = field_count;

    while ((rc = ListMgr_GetNextReportItem(it, result, &result_count,
                                           SPROF(flags)?&prof:NULL)) == DB_SUCCESS)
    {
        result_count = field_count;
        display_report(user_info, result_count, result, result_count,
                       SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                       CSV(flags), display_header, 0);
        display_header = false; /* just display it once */

        total_count += result[1+shift].value_u.val_biguint;
#if defined(HAVE_SHOOK) || defined(_LUSTRE_HSM)
        /* this is a sum(size) => keep it as is */
        total_size += result[2+shift].value_u.val_biguint;
#else
        /* this is a block count => multiply by 512 to get the space in bytes */
        total_size += (result[2+shift].value_u.val_biguint * DEV_BSIZE);
#endif
    }

    ListMgr_CloseReport( it );

    /* display summary */
    if ( !NOHEADER(flags) )
    {
        char strsz[128];
        FormatFileSize( strsz, 128, total_size );
        printf("\nTotal: %llu entries, %llu bytes used (%s)\n",
               total_count, total_size, strsz);
    }

}

static void report_topdirs( unsigned int count, int flags )
{
    /* To be retrieved for dirs:
     * fullpath, owner, dircount, last_mod
     * => sorted by dircount DESC
     */
    int            rc, index;
    uint64_t       mask_sav;
    lmgr_sort_type_t sorttype;
    lmgr_filter_t  filter;
    filter_value_t fv;
    lmgr_iter_opt_t opt;
    struct lmgr_iterator_t *it;
    attr_set_t     attrs;
    entry_id_t     id;

    int list[] = { ATTR_INDEX_fullpath,
                   ATTR_INDEX_dircount,
                   ATTR_INDEX_avgsize,
                   ATTR_INDEX_owner,
                   ATTR_INDEX_gr_name,
                   ATTR_INDEX_last_mod };
    int list_cnt = sizeof(list)/sizeof(int);

    /* select only directories */
    lmgr_simple_filter_init( &filter );

    /* This filter is implicit when sorting dirs by count */
//    fv.value.val_str = STR_TYPE_DIR;
//    lmgr_simple_filter_add( &filter, ATTR_INDEX_type, EQUAL, fv, 0 );

    if (count_min) {
        /* @TODO Not supported by ListMgr yet */
        fv.value.val_biguint = count_min;
        lmgr_simple_filter_add( &filter, ATTR_INDEX_dircount, MORETHAN, fv, 0 );
    }

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), NULL );

    if (SORT_BY_AVGSIZE(flags))
        sorttype.attr_index = ATTR_INDEX_avgsize;
    else if (!SORT_BY_SZRATIO(flags)) /* sort by count (default) */
        /* default: order by dircount */
        sorttype.attr_index = ATTR_INDEX_dircount;
    else {
        /* SORT_BY_SZRATIO? */
        DisplayLog( LVL_MAJOR, REPORT_TAG,
                   "WARNING: sorting directories by size-ratio is not supported" );
        sorttype.attr_index = ATTR_INDEX_dircount; /* keep the default */
    }

    sorttype.order = REVERSE(flags)?SORT_ASC:SORT_DESC;

    /* select only the top dirs */
    opt.list_count_max = count;
    opt.force_no_acct = 0;
    /* allow missing entries */
    opt.allow_no_attr = 1;

    ATTR_MASK_INIT( &attrs );
    mask_sav = attrs.attr_mask = list2mask(list, list_cnt);

    it = ListMgr_Iterator( &lmgr, &filter, &sorttype, &opt );

    lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve top directories from database." );
        return;
    }

    if (!(NOHEADER(flags)))
        print_attr_list(1, list, list_cnt, NULL, CSV(flags));

    index = 0;
    while ( ( rc = ListMgr_GetNext( it, &id, &attrs ) ) == DB_SUCCESS )
    {
        index++;
        /* resolv id for dir requests */
        print_attr_values(index, list, list_cnt, &attrs, &id,
                          CSV(flags), ResolvName);

        ListMgr_FreeAttrs( &attrs );

        /* prepare next call */
        attrs.attr_mask = mask_sav;
    }
    ListMgr_CloseIterator( it );
}

static void report_topsize( unsigned int count, int flags )
{
    /* To be retrieved for files
     * fullpath, owner, size, stripe_info, last_access, last_mod
     * => sorted by size DESC
     */
    int            rc, index;
    uint64_t       mask_sav;
    lmgr_sort_type_t sorttype;
    lmgr_filter_t  filter;
    filter_value_t fv;
    lmgr_iter_opt_t opt;
    struct lmgr_iterator_t *it;
    attr_set_t     attrs;
    entry_id_t     id;

    int list[] = { ATTR_INDEX_fullpath,
#ifdef ATTR_INDEX_status
                   ATTR_INDEX_status,
#endif
                   ATTR_INDEX_size,
                   ATTR_INDEX_owner,
                   ATTR_INDEX_gr_name,
                   ATTR_INDEX_last_access,
                   ATTR_INDEX_last_mod,
                   ATTR_INDEX_fileclass,
                   ATTR_INDEX_stripe_info,
                   ATTR_INDEX_stripe_items
                };
    int list_cnt = sizeof(list)/sizeof(int);

    /* select only files */
    fv.value.val_str = STR_TYPE_FILE;
    lmgr_simple_filter_init( &filter );
    lmgr_simple_filter_add( &filter, ATTR_INDEX_type, EQUAL, fv, 0 );

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), NULL );

    /* order by size desc */
    sorttype.attr_index = ATTR_INDEX_size;
    sorttype.order = REVERSE(flags)?SORT_ASC:SORT_DESC;

    /* select only the top size */
    opt.list_count_max = count;
    opt.force_no_acct = 0;
    /* skip missing entries */
    opt.allow_no_attr = 0;

    ATTR_MASK_INIT( &attrs );
    mask_sav = attrs.attr_mask = list2mask(list, list_cnt);

    it = ListMgr_Iterator( &lmgr, &filter, &sorttype, &opt );

    lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve top file size from database." );
        return;
    }

    if (!(NOHEADER(flags)))
        print_attr_list(1, list, list_cnt, NULL, CSV(flags));

    index = 0;
    while ( ( rc = ListMgr_GetNext( it, &id, &attrs ) ) == DB_SUCCESS )
    {
        index++;
        print_attr_values(index, list, list_cnt, &attrs, &id,
                          CSV(flags), NULL);

        ListMgr_FreeAttrs( &attrs );
        /* prepare next call */
        attrs.attr_mask = mask_sav;
    }

    ListMgr_CloseIterator( it );
}

/** @TODO a revoir pour rbh V3 */
static void report_toppurge( unsigned int count, int flags )
{
    /* To be retrieved: non whitelisted, non directories, non invalid
     * fullpath, type, last_access, last_mod, size, stripe_info
     * => sorted by last_access ASC
     */
    int            rc, index;
    uint64_t       mask_sav;
    lmgr_sort_type_t sorttype;
    lmgr_filter_t  filter;
    filter_value_t fv;
    lmgr_iter_opt_t opt;
    struct lmgr_iterator_t *it;
    attr_set_t     attrs;
    entry_id_t     id;

    int list[] = {
                ATTR_INDEX_fullpath,
                ATTR_INDEX_type,
                ATTR_INDEX_last_access,
                ATTR_INDEX_last_mod,
                ATTR_INDEX_size,
                ATTR_INDEX_blocks,
                ATTR_INDEX_stripe_info,
                ATTR_INDEX_stripe_items
                };
    int list_cnt = sizeof(list)/sizeof(int);

    lmgr_simple_filter_init( &filter );

    /* select only non directories */
    fv.value.val_str = STR_TYPE_DIR;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_type, NOTEQUAL, fv, 0 );

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), NULL );

    /* select only non whitelisted */
#ifdef ATTR_INDEX_release_class
    fv.value.val_str = CLASS_IGNORED;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_release_class, NOTEQUAL, fv,
                            FILTER_FLAG_ALLOW_NULL);
#endif
#if defined(ATTR_INDEX_no_release)
    fv.value.val_bool = true;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_no_release, NOTEQUAL, fv,
                            FILTER_FLAG_ALLOW_NULL);
#endif

#ifdef ATTR_INDEX_status
    fv.value.val_int = STATUS_SYNCHRO;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_status, EQUAL, fv, 0);
#endif

#ifdef ATTR_INDEX_invalid
    /* select only non invalid */
    fv.value.val_bool = true;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_invalid, NOTEQUAL, fv, 0);
#endif

    sorttype.attr_index = ATTR_INDEX_last_access;
    sorttype.order = REVERSE(flags)?SORT_DESC:SORT_ASC;

    /* select only the top size */
    opt.list_count_max = count;
    opt.force_no_acct = 0;
    /* skip missing entries */
    opt.allow_no_attr = 0;

    ATTR_MASK_INIT( &attrs );
    mask_sav = attrs.attr_mask = list2mask(list, list_cnt);

    it = ListMgr_Iterator( &lmgr, &filter, &sorttype, &opt );

    lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve top purge list from database." );
        return;
    }

    if (!(NOHEADER(flags)))
        print_attr_list(1, list, list_cnt, NULL, CSV(flags));

    index = 0;
    while ( ( rc = ListMgr_GetNext( it, &id, &attrs ) ) == DB_SUCCESS )
    {
        index++;

        print_attr_values(index, list, list_cnt, &attrs, &id,
                          CSV(flags), NULL);
        ListMgr_FreeAttrs( &attrs );

        /* prepare next call */
        attrs.attr_mask = mask_sav;
    }

    ListMgr_CloseIterator( it );

}

#ifdef HAVE_RMDIR_POLICY
static void report_toprmdir( unsigned int count, int flags )
{
    /* To be retrieved for dirs:
     * fullpath, owner, last_mod
     * filter: type=dir, not invalid, not whitelisted, empty
     * => sorted by last_mod ASC
     */
    int            rc, index;
    uint64_t       mask_sav;
    lmgr_sort_type_t sorttype;
    lmgr_filter_t  filter;
    filter_value_t fv;
    lmgr_iter_opt_t opt;
    struct lmgr_iterator_t *it;
    attr_set_t     attrs;
    entry_id_t     id;
    char           date[128];
    char           dur[128];
    struct tm      t;

    int list[] = {
                ATTR_INDEX_fullpath,
                ATTR_INDEX_owner,
                ATTR_INDEX_gr_name,
                ATTR_INDEX_last_mod
                };
    int list_cnt = sizeof(list)/sizeof(int);


    lmgr_simple_filter_init( &filter );

    /* select only directories */
    fv.value.val_str = STR_TYPE_DIR;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_type, EQUAL, fv, 0 );

    /* select only non ignored */
    fv.value.val_str = CLASS_IGNORED;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_release_class, NOTEQUAL, fv,
                            FILTER_FLAG_ALLOW_NULL);

    /* select only non invalid */
    fv.value.val_bool = true;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_invalid, NOTEQUAL, fv, FILTER_FLAG_ALLOW_NULL);

    /* only consider empty directories */
    fv.value.val_uint = 0;
    rc = lmgr_simple_filter_add( &filter, ATTR_INDEX_dircount, EQUAL, fv, 0);

    mk_global_filters( &filter, !NOHEADER(flags), NULL );

    /* order by last_mod asc */
    sorttype.attr_index = ATTR_INDEX_last_mod;
    sorttype.order = REVERSE(flags)?SORT_DESC:SORT_ASC;

    /* select only the top dirs */
    opt.list_count_max = count;
    opt.force_no_acct = 0;
    /* allow missing entries */
    opt.allow_no_attr = 1;

    ATTR_MASK_INIT( &attrs );
    ATTR_MASK_SET( &attrs, fullpath );
    ATTR_MASK_SET( &attrs, owner );
    ATTR_MASK_SET( &attrs, gr_name );
    ATTR_MASK_SET( &attrs, last_mod );

    mask_sav = attrs.attr_mask;

    it = ListMgr_Iterator( &lmgr, &filter, &sorttype, &opt );

    lmgr_simple_filter_free( &filter );

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve top directories from database." );
        return;
    }

    if (!(NOHEADER(flags)))
        print_attr_list_custom(1, list, list_cnt, NULL, CSV(flags), "rmdir_deadline", 20);

    index = 0;
    while ( ( rc = ListMgr_GetNext( it, &id, &attrs ) ) == DB_SUCCESS )
    {
        time_t         mod = ATTR( &attrs, last_mod );

        index++;
        /* format last mod */
        strftime( date, 128, "%Y/%m/%d %T", localtime_r( &mod, &t ) );

        if ( policies.rmdir_policy.age_rm_empty_dirs == 0 )
            strcpy( dur, "disabled" );
        else if ( ATTR( &attrs, last_mod ) <
                  time( NULL ) - policies.rmdir_policy.age_rm_empty_dirs )
            strcpy( dur, "expired" );
        else if ( CSV(flags) )
            sprintf( dur, "%u",
                     ( unsigned int ) ( ATTR( &attrs, last_mod ) - time( NULL ) +
                                        policies.rmdir_policy.age_rm_empty_dirs ) );
        else
            FormatDurationFloat( dur, 128,
                                 ATTR( &attrs,
                                       last_mod ) - time( NULL ) +
                                 policies.rmdir_policy.age_rm_empty_dirs );


        print_attr_values_custom(index, list, list_cnt, &attrs, &id,
                                 CSV(flags), NULL, dur, 20);

        ListMgr_FreeAttrs( &attrs );

        /* prepare next call */
        attrs.attr_mask = mask_sav;
    }

    ListMgr_CloseIterator( it );

}
#endif

static void report_topuser( unsigned int count, int flags )
{
    unsigned int   result_count;
    struct lmgr_report_t *it;
    lmgr_iter_opt_t opt;
    int            rc;
    unsigned int   rank = 1;
    lmgr_filter_t  filter;
    filter_value_t fv;
    bool is_filter = false;
    profile_u   prof;

#define TOPUSERCOUNT 6

    db_value_t     result[TOPUSERCOUNT];

    /* To be retrieved for each user:
     * - username
     * - SUM(blocks)
     * - NB entries
     * - MIN/MAX/AVG size
     */
    report_field_descr_t user_info[TOPUSERCOUNT] = {
        {ATTR_INDEX_owner, REPORT_GROUP_BY, SORT_NONE, false, 0, FV_NULL},
#if defined(HAVE_SHOOK) || defined(_LUSTRE_HSM)
        /* display the total size in HSM (not only the disk level) */
        {ATTR_INDEX_size, REPORT_SUM, SORT_DESC, false, 0, FV_NULL},
#else
        {ATTR_INDEX_blocks, REPORT_SUM, SORT_DESC, false, 0, FV_NULL},
#endif
        {ATTR_INDEX_COUNT, REPORT_COUNT, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MIN, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MAX, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_AVG, SORT_NONE, false, 0, FV_NULL},
    };

    if (REVERSE(flags))
        user_info[1].sort_flag = SORT_ASC;

    if (SORT_BY_COUNT(flags)) {
        /* replace sort on blocks by sort on count */
        user_info[1].sort_flag = SORT_NONE;
        user_info[2].sort_flag = REVERSE(flags)?SORT_ASC:SORT_DESC;
    } else if (SORT_BY_AVGSIZE(flags)) {
        /* sort (big files first) */
        user_info[1].sort_flag = SORT_NONE;
        user_info[5].sort_flag = REVERSE(flags)?SORT_ASC:SORT_DESC;
    }

    if (count_min) {
        user_info[2].filter = true;
        user_info[2].filter_compar = MORETHAN;
        user_info[2].filter_value.value.val_biguint = count_min;
    }

    /* select only the top users */
    opt.list_count_max = count;
    opt.force_no_acct = 0;
    /* skip missing entries */
    opt.allow_no_attr = 0;
    opt.force_no_acct = FORCE_NO_ACCT(flags);

    /* select only files */
    lmgr_simple_filter_init( &filter );

    fv.value.val_str = STR_TYPE_FILE;
    lmgr_simple_filter_add( &filter, ATTR_INDEX_type, EQUAL, fv, 0 );
    is_filter = true;

    mk_global_filters( &filter, !NOHEADER(flags), &is_filter );

    /* is a filter specified? */
    it = ListMgr_Report( &lmgr, user_info, TOPUSERCOUNT,
                         SPROF(flags)?&size_profile:NULL, &filter, &opt );
    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve top space consumers from database." );
        return;
    }

    result_count = TOPUSERCOUNT;
    while ((rc = ListMgr_GetNextReportItem(it, result, &result_count,
                                           SPROF(flags)?&prof:NULL)) == DB_SUCCESS)
    {
        display_report(user_info, result_count, result, result_count,
                       SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                       CSV(flags), (rank == 1) && !NOHEADER(flags), rank); /* display header once */

        rank++;

        /* prepare next call */
        result_count = TOPUSERCOUNT;

    }

    ListMgr_CloseReport( it );


}

#ifdef HAVE_RM_POLICY
static void report_deferred_rm( int flags )
{
    int            rc;
    struct lmgr_rm_list_t * rmlist;
    entry_id_t     id;
    attr_set_t     attrs;

    unsigned long long total_count = 0;
    unsigned long long total_size = 0;

    lmgr_filter_t  filter;
    bool is_filter = false;

    lmgr_sort_type_t sort;

    static int list[] = {
                   ATTR_INDEX_rm_time,
                   ATTR_INDEX_ID, /* id */
                   ATTR_INDEX_type,
                   ATTR_INDEX_owner,
                   ATTR_INDEX_gr_name,
                   ATTR_INDEX_size,
                   ATTR_INDEX_last_mod,
                   ATTR_INDEX_fullpath
                };
    int list_cnt = sizeof(list)/sizeof(int);

    lmgr_simple_filter_init( &filter );

    /* append global filters */
    mk_global_filters( &filter, !NOHEADER(flags), &is_filter );

    /* order by rmtime asc */
    sort.attr_index = ATTR_INDEX_rm_time;
    sort.order = REVERSE(flags)?SORT_DESC:SORT_ASC;

    /* list all deferred rm, even if non expired */
    rmlist = ListMgr_RmList(&lmgr, is_filter? &filter : NULL, &sort);

    if (rmlist == NULL)
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve removed entries from database." );
        return;
    }

    if (!NOHEADER(flags))
        print_attr_list(0, list, list_cnt, NULL, CSV(flags));

    while ((rc = ListMgr_GetNextRmEntry(rmlist, &id, &attrs)) == DB_SUCCESS)
    {
        total_count++;
        if (ATTR_MASK_TEST(&attrs, size))
            total_size += ATTR(&attrs, size);

        print_attr_values(0, list, list_cnt, &attrs, &id, CSV(flags), ResolvName);

        /* prepare next call */
        memset(&attrs, 0, sizeof(attrs));
    }

    ListMgr_CloseRmList(rmlist);

    /* display summary */
    if (!NOHEADER(flags))
    {
        char strsz[128];

        FormatFileSize(strsz, sizeof(strsz), total_size);

        printf("\nTotal: %llu entries, %llu bytes (%s)\n",
               total_count, total_size, strsz);
    }


}
#endif


static void report_class_info( int flags )
{
    #define CLASSINFO_FIELDS 7
    db_value_t     result[CLASSINFO_FIELDS];

    struct lmgr_report_t *it;
    lmgr_filter_t  filter;
    int            rc;
    bool header;
    unsigned int   result_count;
    profile_u prof;
    bool is_filter = false;

    unsigned long long total_size, total_count;
    total_size = total_count = 0;

    /* To be retrieved for each group:
     * - class names and status
     * - NB entries
     * - SUM(blocks)
     * - MIN/MAX/AVG file size
     */
    report_field_descr_t class_info[CLASSINFO_FIELDS] = {
        {ATTR_INDEX_fileclass, REPORT_GROUP_BY, SORT_ASC, false, 0, FV_NULL},
        {ATTR_INDEX_COUNT, REPORT_COUNT, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_blocks, REPORT_SUM, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_SUM, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MIN, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MAX, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_AVG, SORT_NONE, false, 0, FV_NULL},
    };

    lmgr_simple_filter_init(&filter);
    mk_global_filters(&filter, !NOHEADER(flags), &is_filter);
    result_count = CLASSINFO_FIELDS;

    it = ListMgr_Report(&lmgr, class_info, CLASSINFO_FIELDS,
                        SPROF(flags)? &size_profile : NULL,
                        is_filter? &filter : NULL, NULL);

    if ( it == NULL )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR: Could not retrieve class information from database." );
        return;
    }

    /* a single class column (release), can print as is */
    header = !NOHEADER(flags);

    result_count = CLASSINFO_FIELDS;
    while ((rc = ListMgr_GetNextReportItem(it, result, &result_count, SPROF(flags)?&prof:NULL))
            == DB_SUCCESS)
    {
        display_report(class_info, result_count, result, result_count,
                       SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                       CSV(flags), header, 0);
        header = false; /* display header once */

        total_count += result[1].value_u.val_biguint;
        total_size += result[3].value_u.val_biguint;
        result_count = CLASSINFO_FIELDS;
    }

    ListMgr_CloseReport(it);
    lmgr_simple_filter_free( &filter );

    /* display summary */
    if ( !NOHEADER(flags) )
    {
        char strsz[128];
        FormatFileSize( strsz, 128, total_size );
        printf("\nTotal: %llu entries, %llu bytes (%s)\n",
               total_count, total_size, strsz);
    }
}

static void report_status_info(int smi_index, const char* val, int flags)
{
    #define STATUSINFO_FIELDS 8
    db_value_t     result[STATUSINFO_FIELDS];

    struct lmgr_report_t *it;
    lmgr_filter_t  filter;
    int            rc;
    bool header;
    unsigned int   result_count;
    profile_u prof;
    bool is_filter = false;

    unsigned long long total_size, total_count;
    total_size = total_count = 0;

    /* To be retrieved for each group:
     * - status names and status
     * - NB entries
     * - SUM(blocks)
     * - MIN/MAX/AVG file size
     */
    report_field_descr_t status_info[STATUSINFO_FIELDS] = {
        {ATTR_COUNT + smi_index, REPORT_GROUP_BY, SORT_ASC, false, 0, FV_NULL},
        {ATTR_INDEX_type, REPORT_GROUP_BY, SORT_ASC, false, 0, FV_NULL},
        {ATTR_INDEX_COUNT, REPORT_COUNT, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_blocks, REPORT_SUM, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_SUM, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MIN, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_MAX, SORT_NONE, false, 0, FV_NULL},
        {ATTR_INDEX_size, REPORT_AVG, SORT_NONE, false, 0, FV_NULL},
    };


    if (count_min) {
        status_info[2].filter = true;
        status_info[2].filter_compar = MORETHAN;
        status_info[2].filter_value.value.val_biguint = count_min;
    }

    lmgr_simple_filter_init(&filter);

    if (val != NULL)
    {
        filter_value_t fv;

        fv.value.val_str = val;
        lmgr_simple_filter_add(&filter, ATTR_COUNT + smi_index, EQUAL, fv,
                               EMPTY_STRING(val)? FILTER_FLAG_ALLOW_NULL : 0);
        is_filter = true;
    }

    mk_global_filters(&filter, !NOHEADER(flags), &is_filter);
    result_count = STATUSINFO_FIELDS;

    /* @TODO add filter on status, if a value is specified */

    it = ListMgr_Report(&lmgr, status_info, STATUSINFO_FIELDS,
                        SPROF(flags)? &size_profile : NULL,
                        is_filter? &filter : NULL, NULL);
    if (it == NULL)
    {
        DisplayLog(LVL_CRIT, REPORT_TAG,
                   "ERROR: Could not retrieve status information from database.");
        return;
    }

    /* a single status column (release), can print as is */
    header = !NOHEADER(flags);

    result_count = STATUSINFO_FIELDS;
    while ((rc = ListMgr_GetNextReportItem(it, result, &result_count, SPROF(flags)?&prof:NULL))
           == DB_SUCCESS)
    {
        display_report(status_info, result_count, result, result_count,
                       SPROF(flags)?&size_profile:NULL, SPROF(flags)?&prof:NULL,
                       CSV(flags), header, 0);
        header = false; /* display header once */

        total_count += result[2].value_u.val_biguint;
        total_size += result[4].value_u.val_biguint;
        result_count = STATUSINFO_FIELDS;
    }

    ListMgr_CloseReport(it);
    lmgr_simple_filter_free(&filter);

    /* display summary */
    if (!NOHEADER(flags))
    {
        char strsz[128];
        FormatFileSize(strsz, 128, total_size);
        printf("\nTotal: %llu entries, %llu bytes (%s)\n",
               total_count, total_size, strsz);
    }
}


static void maintenance_get( int flags )
{
    char           value[1024];
    time_t         timestamp;
    char           date[128];
    struct tm      t;
    int            rc;

    rc = ListMgr_GetVar( &lmgr, NEXT_MAINT_VAR, value, sizeof(value));
    if ( rc == DB_SUCCESS )
    {
        timestamp = atoi( value );
        strftime( date, 128, "%Y/%m/%d %T", localtime_r( &timestamp, &t ) );
        if ( time(NULL) >= timestamp )
        {
            if ( CSV(flags) )
                printf( "next_maintenance, %s (in the past: no effect)\n", date );
            else
                printf( "Next maintenance: %s (in the past: no effect)\n", date );
        }
        else
        {
            if ( CSV(flags) )
                printf( "next_maintenance, %s\n", date );
            else
                printf( "Next maintenance: %s\n", date );
        }
    }
    else if ( rc == DB_NOT_EXISTS )
    {
        if ( CSV(flags) )
            printf( "next_maintenance, none\n" );
        else
            printf( "No maintenance is planned\n" );
    }
    else
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR retrieving variable " NEXT_MAINT_VAR " from database" );
    }
}

static void maintenance_set( int flags, time_t when )
{
    char           value[1024];
    int            rc;

    if (when == 0)
    {
        rc = ListMgr_SetVar( &lmgr, NEXT_MAINT_VAR, NULL );

        if (rc)
            DisplayLog( LVL_CRIT, REPORT_TAG,
                        "ERROR deleting variable " NEXT_MAINT_VAR " in database" );
        else
            DisplayLog( LVL_EVENT, REPORT_TAG, "Next maintenance time has been cleared successfully" );

        return;
    }

    sprintf( value, "%u", (unsigned int)when );

    rc = ListMgr_SetVar( &lmgr, NEXT_MAINT_VAR, value );
    if ( rc == DB_SUCCESS )
    {
        DisplayLog( LVL_EVENT, REPORT_TAG, "Next maintenance time has been set successfully" );
    }
    else
    {
        DisplayLog( LVL_CRIT, REPORT_TAG,
                    "ERROR setting variable " NEXT_MAINT_VAR " in database" );
    }
}

#define MAX_OPT_LEN 1024

/**
 * Main daemon routine
 */
int main( int argc, char **argv )
{
    int            c, option_index = 0;
    char          *bin = basename( argv[0] );

    char           config_file[MAX_OPT_LEN] = "";

    bool           force_log_level = false;
    int            log_level = 0;

    bool           activity = false;
    bool           fs_info = false;

    bool           entry_info = false;
    char           entry_path[RBH_PATH_MAX] = "";

    bool           user_info = false;
    char           user_name[256] = "";

    bool           group_info = false;
    char           group_name[256] = "";

    bool           class_info = false;

    int            topdirs = 0;
    int            topsize = 0;
    int            toppurge = 0;
    int            toprmdir = 0;
    int            topuser = 0;
#ifdef HAVE_RM_POLICY
    int            deferred_rm = 0;
#endif

    bool           dump_all = false;
    bool           dump_user = false;
    char           dump_user_name[256];
    bool           dump_group = false;
    char           dump_group_name[256];
#ifdef _LUSTRE
    bool           dump_ost = false;
    value_list_t   dump_ost_set = { 0, NULL };
    char           ost_set_str[256] = "";
#endif
    char          *status_name = NULL;
    char          *status_value = NULL;

    char          *status_info_name = NULL;
    char          *status_info_value = NULL;

    time_t         next_maint = 0;
    bool           get_next_maint = false;
    bool           cancel_next_maint = false;

    int            flags = 0;
    int            rc;
    char           err_msg[4096];
    bool chgd = false;
    char badcfg[RBH_PATH_MAX];

    /* parse command line options */
    while ( ( c = getopt_long( argc, argv, SHORT_OPT_STRING, option_tab, &option_index ) ) != -1 )
    {
        switch ( c )
        {
        case 'a':
            activity = true;
            break;

        case 'P':
            if ( !optarg )
            {
                fprintf(stderr, "Missing mandatory argument <path> for --filter-path\n");
                exit(1);
            }
            rh_strncpy(path_filter, optarg, RBH_PATH_MAX);
            break;

        case 'C':
            if ( !optarg )
            {
                fprintf(stderr, "Missing mandatory argument <class> for --filter-class\n");
                exit(1);
            }
            if (class_info && !EMPTY_STRING(class_filter))
                fprintf(stderr, "WARNING: --filter-class conflicts with --class-info parameter. ignored.\n");
            else
                rh_strncpy(class_filter, optarg, 1024);
            break;

        case OPT_CLASS_INFO:

            if ( class_info )
                fprintf(stderr, "WARNING: --class-info parameter already specified on command line.\n");

            class_info = true;
            if (optarg)
            {
                if (!EMPTY_STRING(class_filter))
                    fprintf(stderr, "WARNING: --class-info conflicts with --filter-class parameter. overriding filter.\n");
                rh_strncpy(class_filter, optarg, 1024);
            }
            break;

        case OPT_STATUS_INFO:
            if (status_info_name)
                fprintf(stderr, "WARNING: --status-info parameter already specified on command line.\n");
            rc = parse_status_arg("--status-info", optarg, &status_info_name, &status_info_value, false);
            if (rc)
                exit(rc);
            break;

        case 'i':
            fs_info = true;
            break;

        case 'e':
            entry_info = true;
            rh_strncpy(entry_path, optarg, RBH_PATH_MAX);
            break;

        case 'u':
            user_info = true;
            if ( optarg )
                rh_strncpy(user_name, optarg, 256);
            break;

        case 'g':
            group_info = true;
            if ( optarg )
                rh_strncpy(group_name, optarg, 256);
            break;

        case 'D':
            dump_all = true;
            break;

        case OPT_DUMP_USER:
            dump_user = true;
            if ( !optarg )
            {
                fprintf(stderr, "Missing mandatory argument <username> for --dump-user\n");
                exit(1);
            }
            rh_strncpy(dump_user_name, optarg, 256);
            break;

        case OPT_DUMP_GROUP:
            dump_group = true;
            if ( !optarg )
            {
                fprintf(stderr, "Missing mandatory argument <groupname> for --dump-group\n");
                exit(1);
            }
            rh_strncpy(dump_group_name, optarg, 256);
            break;

#ifdef _LUSTRE
        case OPT_DUMP_OST:
            dump_ost = true;
            if ( !optarg )
            {
                fprintf(stderr, "Missing mandatory argument <ost_index|ost_set> for --dump-ost\n");
                exit(1);
            }
            /* parse it as a set */
            if (lmgr_range2list(optarg, DB_UINT, &dump_ost_set))
            {
                fprintf( stderr,
                         "Invalid value '%s' for --dump-ost option: integer or set expected (e.g. 2 or 3,5-8,10-12).\n",
                         optarg );
                exit( 1 );
            }
            /* copy arg to display it */
            rh_strncpy(ost_set_str, optarg, sizeof(ost_set_str));
            break;
#endif

        case OPT_DUMP_STATUS:
            rc = parse_status_arg("--dump-status", optarg, &status_name, &status_value, true);
            if (rc)
                exit(rc);
            break;

        case 'd':
            if ( optarg )
            {
                topdirs = str2int( optarg );
                if ( topdirs == -1 )
                {
                    fprintf( stderr,
                             "Invalid parameter '%s' for --topdirs option: positive integer expected\n",
                             optarg );
                    exit( 1 );
                }
            }
            else
                topdirs = DEFAULT_TOP_SIZE;
            break;

        case 's':
            if ( optarg )
            {
                topsize = str2int( optarg );
                if ( topsize == -1 )
                {
                    fprintf( stderr,
                             "Invalid parameter '%s' for --topsize option: positive integer expected\n",
                             optarg );
                    exit( 1 );
                }
            }
            else
                topsize = DEFAULT_TOP_SIZE;
            break;

        case 'p':
            if ( optarg )
            {
                toppurge = str2int( optarg );
                if ( toppurge == -1 )
                {
                    fprintf( stderr,
                             "Invalid parameter '%s' for --toppurge option: positive integer expected\n",
                             optarg );
                    exit( 1 );
                }
            }
            else
                toppurge = DEFAULT_TOP_SIZE;
            break;

        case OPT_TOPRMDIR:
            if ( optarg )
            {
                toprmdir = str2int( optarg );
                if ( toprmdir == -1 )
                {
                    fprintf( stderr,
                             "Invalid parameter '%s' for --toprmdir option: positive integer expected\n",
                             optarg );
                    exit( 1 );
                }
            }
            else
                toprmdir = DEFAULT_TOP_SIZE;
            break;

        case 'U':
            if ( optarg )
            {
                topuser = str2int( optarg );
                if ( topuser == -1 )
                {
                    fprintf( stderr,
                             "Invalid parameter '%s' for --topusers option: positive integer expected\n",
                             optarg );
                    exit( 1 );
                }
            }
            else
                topuser = DEFAULT_TOP_SIZE;
            break;

#ifdef HAVE_RM_POLICY
        case 'R':
            deferred_rm = true;
            break;
#endif

#ifdef HAVE_MIGR_POLICY
        case CLEAR_NEXT_MAINT:
            cancel_next_maint = true;
            get_next_maint = true;
            break;
        case SET_NEXT_MAINT:
            if ( optarg )       /* optional argument */
            {
                /* parse date/time yyyymmddHHMM[SS] */
                next_maint = str2date(optarg);
                if (next_maint == (time_t)-1) {
                    fprintf( stderr,
                             "Invalid date format: yyyymmdd[HH[MM[SS]]] expected\n" );
                    exit(1);
                }
            }
            /* in all cases, display next maintenance time */
            get_next_maint = true;
            break;
#endif

        case 'f':
            rh_strncpy(config_file, optarg, MAX_OPT_LEN);
            break;
        case 'l':
            force_log_level = true;
            log_level = str2debuglevel( optarg );
            if ( log_level == -1 )
            {
                fprintf( stderr,
                         "Unsupported log level '%s'. CRIT, MAJOR, EVENT, VERB, DEBUG or FULL expected.\n",
                         optarg );
                exit( 1 );
            }
            break;

        case 'r':
            flags |= OPT_FLAG_REVERSE;
            break;
        case 'c':
            flags |= OPT_FLAG_CSV;
            break;
        case 'q':
            flags |= OPT_FLAG_NOHEADER;
            break;
        case 'h':
            display_help( bin );
            exit( 0 );
            break;
        case 'V':
            display_version( bin );
            exit( 0 );
            break;
        case 'F':
            flags |= OPT_FLAG_NO_ACCT;
            break;
        case 'S':
            flags |= OPT_FLAG_SPLITUSERGROUP;
            break;
        case OPT_BY_COUNT:
            flags |= OPT_FLAG_BY_COUNT;
            break;
        case OPT_BY_AVGSIZE:
            flags |= OPT_FLAG_BY_AVGSIZE;
            break;
        case OPT_BY_SZ_RATIO:
            flags |= OPT_FLAG_BY_SZRATIO;
            /* auto-enable size profiling */
            flags |= OPT_FLAG_SPROF;
            /* parse range */
            if (parse_size_range(optarg, &size_profile))
                exit(1);
            break;
        case OPT_COUNT_MIN:
            count_min = atoi(optarg);
            break;

        case OPT_SIZE_PROFILE:
            flags |= OPT_FLAG_SPROF;
            break;

        case ':':
        case '?':
        default:
            display_help( bin );
            exit( 1 );
            break;
        }
    }

    /* check there is no extra arguments */
    if ( optind != argc )
    {
        fprintf( stderr, "Error: unexpected argument on command line: %s\n", argv[optind] );
        exit( 1 );
    }

    /* if a size range was specified, determine sort order:
        default DESC, ASC for reverse */
    if (size_profile.range_ratio_len > 0)
        size_profile.range_ratio_sort = REVERSE(flags)?SORT_ASC:SORT_DESC;

    if ( !activity && !fs_info && !user_info && !group_info
         && !topsize && !toppurge && !topuser && !dump_all
         && !dump_user && !dump_group && !class_info && !entry_info
         && (status_name == NULL) && (status_info_name == NULL)
         && !topdirs
#ifdef HAVE_RM_POLICY
        && !deferred_rm
#endif
#ifdef HAVE_RMDIR_POLICY
         && !toprmdir
#endif
#ifdef _LUSTRE
        && !dump_ost
#endif
#ifdef HAVE_MIGR_POLICY
        && !next_maint && !get_next_maint && !cancel_next_maint
#endif
        )
    {
        display_help( bin );
        exit( 1 );
    }

    rc = rbh_init_internals();
    if (rc != 0)
        exit(rc);

    /* get default config file, if not specified */
    if (SearchConfig(config_file, config_file, &chgd, badcfg, MAX_OPT_LEN) != 0)
    {
        fprintf(stderr, "No config file (or too many) found matching %s\n", badcfg);
        exit(2);
    }
    else if (chgd)
    {
        fprintf(stderr, "Using config file '%s'.\n", config_file );
    }

    /* only read common config (listmgr, ...) (mask=0) */
    if(rbh_cfg_load(0, config_file, err_msg))
    {
        fprintf(stderr, "Error reading configuration file '%s': %s\n",
                config_file, err_msg);
        exit(1);
    }

    if (force_log_level)
        log_config.debug_level = log_level;
    else
        log_config.debug_level = LVL_MAJOR; /* no event message */

    /* Initialize logging */
    rc = InitializeLogs(bin);
    if (rc)
    {
        fprintf(stderr, "Error opening log files: rc=%d, errno=%d: %s\n",
                rc, errno, strerror(errno));
        exit(rc);
    }

    if ((rc = InitFS()) != 0)
            fprintf(stderr, "Warning: cannot access filesystem %s (%s), some reports may be incomplete or not available.\n",
                    global_config.fs_path, strerror(abs(rc)));

    /* Initialize list manager */
    rc = ListMgr_Init(true);
    if ( rc )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG, "Error %d initializing list manager", rc );
        exit( rc );
    }
    else
        DisplayLog( LVL_DEBUG, REPORT_TAG, "ListManager successfully initialized" );

    if ( CheckLastFS(  ) != 0 )
        exit( 1 );

    /* Create database access */
    rc = ListMgr_InitAccess( &lmgr );
    if ( rc )
    {
        DisplayLog( LVL_CRIT, REPORT_TAG, "Error %d: cannot connect to database", rc );
        exit( rc );
    }


    /* retrieve and display info */
    if ( activity )
        report_activity( flags );

    if ( fs_info )
        report_fs_info( flags );

    if (entry_info)
        report_entry(entry_path, flags);

    if ( user_info )
        report_usergroup_info( ( EMPTY_STRING( user_name ) ? NULL : user_name ),
                               flags );

    if ( group_info )
        report_usergroup_info( ( EMPTY_STRING( group_name ) ? NULL : group_name ),
                               flags | OPT_FLAG_GROUP );

    if ( class_info )
        report_class_info(flags);

    if ( topdirs )
        report_topdirs( topdirs, flags );

    if ( topsize )
        report_topsize( topsize, flags );

    if ( toppurge )
        report_toppurge( toppurge, flags );

#ifdef HAVE_RMDIR_POLICY
    if ( toprmdir )
        report_toprmdir( toprmdir, flags );
#endif

    if ( topuser )
        report_topuser( topuser, flags );

#ifdef HAVE_RM_POLICY
    if ( deferred_rm )
        report_deferred_rm( flags );
#endif

    if ( dump_all )
        dump_entries( DUMP_ALL, 0, NULL, NULL, flags );

    if ( dump_user )
        dump_entries( DUMP_USR, 0, dump_user_name, NULL, flags );

    if ( dump_group )
        dump_entries( DUMP_GROUP, 0, dump_group_name, NULL, flags );

#ifdef _LUSTRE
    if ( dump_ost ) {
        dump_entries( DUMP_OST, 0, ost_set_str, &dump_ost_set, flags );
        /* free the list */
        if (dump_ost_set.values)
            MemFree(dump_ost_set.values);
    }
#endif

    if (status_name != NULL)
    {
        sm_instance_t *smi;
        const char *strval;

        rc = check_status_args(status_name, status_value, &strval, &smi);
        if (rc)
            exit(rc);
        dump_entries(DUMP_STATUS, smi->smi_index, (char *)strval, NULL, flags);
    }

    if (status_info_name != NULL)
    {
        sm_instance_t *smi;
        const char *strval;

        rc = check_status_args(status_info_name, status_info_value, &strval, &smi);
        if (rc)
            exit(rc);
        if (smi->sm->flags & SM_NODB)
        {
            fprintf(stderr, "ERROR: status report is not supported for '%s' status manager\n",
                    smi->instance_name);
            exit(EINVAL);
        }
        report_status_info(smi->smi_index, strval, flags);
    }

    if (cancel_next_maint)
        maintenance_set(flags, 0);

    if (next_maint != 0)
        maintenance_set(flags, next_maint);

    if (get_next_maint)
        maintenance_get(flags);

    ListMgr_CloseAccess(&lmgr);

    return 0;                   /* for compiler */

}
