/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 * Copyright (C) 2004-2009 CEA/DAM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the CeCILL License.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license (http://www.cecill.info) and that you
 * accept its terms.
 */

/**
 *  \file rbh_cfg.h
 *  \brief Module for configuration management and parsing.
 *
 * \addtogroup CONFIG_PARSING
 * @{
 */

#ifndef _ROBINHOOD_CONFIG_H
#define _ROBINHOOD_CONFIG_H

#include "config_parsing.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/**
 * Generic definitions for config management
 * \addtogroup MODULE_CONFIG_FUNCTIONS
 * @{
 */
typedef int    (*write_config_func_t) (FILE *output);

/** configuration functions for modules */
typedef struct mod_cfg_funcs {
    const char  *module_name;
    void *      (*new)(void); /**< allocate a new config structure */
    void        (*free)(void *); /**< free a config structure */

    void        (*set_default)(void *); /**< fill config structure with default parameters */
    int         (*read)(config_file_t config, void *cfg, char *msg_out); /**< read parameters from config file */
    int         (*set_config)(void *cfg,  bool reload); /**< set the module config */

    void        (*write_default)(FILE *output); /**< write defaults */
    void        (*write_template)(FILE *output); /**< write a template */
} mod_cfg_funcs_t;

/** @} */

/* get config file for the current process (can be used to replace '{cfg}' in external commands) */
const char *config_file_path(void);

/* behavior flags for all modules */
#define FLAG_DRY_RUN    0x00000001
#define FLAG_IGNORE_POL 0x00000002
#define FLAG_ONCE       0x00000004
#define FLAG_NO_LIMIT   0x00000008
#define FLAG_CHECK_ONLY 0x00000010 /* only check triggers, don't purge */
#define FLAG_NO_GC      0x00000020 /* don't clean orphan entries after scan */
#define FLAG_FORCE_RUN  0x00000040 /* force running policy even if no scan was complete */

/* Config module masks:
 * Global, Log, and List Manager are always initialized.
 * Entry processor, Info Collector, policy runs are optional
 * are optionnal.
 */
#define MODULE_MASK_ENTRY_PROCESSOR 0x00000001
#define MODULE_MASK_FS_SCAN         0x00000002
#define MODULE_MASK_EVENT_HDLR      0x00000004
#define MODULE_MASK_POLICY_RUN      0x00000008

#define MODULE_MASK_ALWAYS          0x10000000


/**
 * Read robinhood's configuration file and set modules configuration.
 * if everything is OK, returns 0 and fills the structure
 * else, returns an error code and sets a contextual error message in err_msg_out.
 */
int rbh_cfg_load(int module_mask, char *file_path, char *msg_out);

/**
 * Reload robinhood's configuration file (the one used for last call to rbh_cfg_load())
 * and change only parameters that can be modified on the fly.
 */
int rbh_cfg_reload(int curr_module_mask);

/**
 * Write a documented template of configuration file,
 * to the given file path.
 * returns 0 on success, else it returns a posix error code.
 */
int rbh_cfg_write_template(FILE *stream);

/**
 * Write all default configuration values,
 * to the given file path.
 * returns 0 on success, else it returns a posix error code.
 */
int rbh_cfg_write_default(FILE *stream);

/* ==== Tools for writing config templates ==== */

void print_begin_block(FILE *output, unsigned int indent, const char *blockname,
                       const char *id );
void print_end_block(FILE *output, unsigned int indent);
void print_line(FILE *output, unsigned int indent, const char *format, ...);

#endif

/** @} */
