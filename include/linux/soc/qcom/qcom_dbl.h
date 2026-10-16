/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DBL_DRIVER_H_
#define _DBL_DRIVER_H_

struct dbl_client;

struct dbl_client *dbl_register_client(const char *name);
void dbl_unregister_client(struct dbl_client *client);

int lpi_dbl_vote(struct dbl_client *client_handle, void *async_cb, long timeout_ms);
int lpi_dbl_unvote(struct dbl_client *handle);

#endif
