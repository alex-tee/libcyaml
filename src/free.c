/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (C) 2017 Michael Drake <tlsa@netsurf-browser.org>
 */

/**
 * \file
 * \brief Free data structures created by the CYAML load functions.
 *
 * As described in the public API for \ref cyaml_free(), it is preferable for
 * clients to write their own free routines, tailored for their data structure.
 *
 * Recursion and stack usage
 * -------------------------
 *
 * This generic CYAML free routine is implemented using recursion, rather
 * than iteration with a heap-allocated stack.  This is because recursion
 * seems less bad than allocating within the free code, and the stack-cost
 * of these functions isn't huge.  The maximum recursion depth is of course
 * bound by the schema, however schemas for recursively nesting data structures
 * are unbound, e.g. for a data tree structure.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "data.h"
#include "util.h"

/**
 * Internal function for freeing a CYAML-parsed data structure.
 *
 * \param[in]  cfg     The client's CYAML library config.
 * \param[in]  schema  The schema describing how to free `data`.
 * \param[in]  data    The data structure to be freed.
 */
static void cyaml__free(
		const cyaml_config_t *cfg,
		const cyaml_schema_type_t *schema,
		uint8_t * const data);

/**
 * Internal function for freeing a CYAML-parsed sequence.
 *
 * \param[in]  cfg              The client's CYAML library config.
 * \param[in]  sequence_schema  The schema describing how to free `data`.
 * \param[in]  data             The data structure to be freed.
 */
static void cyaml__free_sequence(
		const cyaml_config_t *cfg,
		const cyaml_schema_type_t *sequence_schema,
		uint8_t *data)
{
	const cyaml_schema_type_t *schema = sequence_schema->sequence.schema;
	uint32_t data_size = schema->data_size;
	cyaml_err_t err;
	uint64_t count;

	if (sequence_schema->type == CYAML_SEQUENCE) {
		count = cyaml_data_read(sequence_schema->sequence.count_size,
				data + sequence_schema->sequence.count_offset,
				&err);
		if (err != CYAML_OK) {
			return;
		}
	} else {
		assert(sequence_schema->type == CYAML_SEQUENCE_FIXED);
		count = sequence_schema->sequence.max;
	}

	if (sequence_schema->flags & CYAML_FLAG_POINTER) {
		data = (void *)cyaml_data_read(sizeof(void *), data, &err);
		if ((data == NULL) || (err != CYAML_OK)) {
			return;
		}
	}
	if (schema->flags & CYAML_FLAG_POINTER) {
		data_size = sizeof(data);
	}

	for (uint64_t i = 0; i < count; i++) {
		cyaml__free(cfg, schema, data + data_size * i);
	}
}

/**
 * Internal function for freeing a CYAML-parsed mapping.
 *
 * \param[in]  cfg             The client's CYAML library config.
 * \param[in]  mapping_schema  The schema describing how to free `data`.
 * \param[in]  data            The data structure to be freed.
 */
static void cyaml__free_mapping(
		const cyaml_config_t *cfg,
		const cyaml_schema_type_t *mapping_schema,
		uint8_t * const data)
{
	const cyaml_schema_mapping_t *schema = mapping_schema->mapping.schema;

	while (schema->key != NULL) {
		uint8_t *entry_data = data + schema->data_offset;
		cyaml__free(cfg, &schema->value, entry_data);
		schema++;
	}
}

/* This function is documented at the forward declaration above. */
static void cyaml__free(
		const cyaml_config_t *cfg,
		const cyaml_schema_type_t *schema,
		uint8_t * const data)
{
	if (data == NULL) {
		return;
	}
	if (schema->type == CYAML_MAPPING) {
		cyaml__free_mapping(cfg, schema, data);
	} else if (schema->type == CYAML_SEQUENCE ||
	           schema->type == CYAML_SEQUENCE_FIXED) {
		cyaml__free_sequence(cfg, schema, data);
	}

	if (schema->flags & CYAML_FLAG_POINTER) {
		cyaml_err_t err;
		char *alloacted_data = (void *)cyaml_data_read(
				sizeof(char *), data, &err);
		if (err != CYAML_OK) {
			return;
		}
		cyaml__log(cfg, CYAML_LOG_DEBUG,
				"Freeing allocation: %p\n", alloacted_data);
		free(alloacted_data);
	}
}

/* Exported function, documented in include/cyaml/cyaml.h */
cyaml_err_t cyaml_free(
		const cyaml_config_t *config,
		const cyaml_schema_type_t *schema,
		cyaml_data_t *data)
{
	if (config == NULL) {
		return CYAML_ERR_BAD_PARAM_NULL_CONFIG;
	}
	if (schema == NULL) {
		return CYAML_ERR_BAD_PARAM_NULL_SCHEMA;
	}
	cyaml__free(config, schema, data);
	free(data);
	return CYAML_OK;
}