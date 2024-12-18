/*
 * config.h -- structures, constants, macros
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#pragma once

#define MODULE_NAME 	"shofer"

#define AUTHOR		"Marko Vlaic"
#define LICENSE		"Dual BSD/GPL"

struct shofer_dev {
  dev_t dev_no;
  struct cdev cdev;
};
