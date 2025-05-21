/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Your Name <your.email@example.com>
 *
 * NFTables condition expression state management
 */
#ifndef _NFT_CONDITION_STATE_H_
#define _NFT_CONDITION_STATE_H_

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/list.h>

/* Condition state structure */
struct condition_state {
    atomic_t enabled;              /* Atomic state value (0/1) */
    atomic_t refcount;            /* Reference counter */
    char name[NAME_MAX];          /* Condition name */
    struct mutex lock;            /* Protects state updates */
    struct list_head list;        /* Global state list entry */
    struct proc_dir_entry *proc;  /* Associated procfs entry */
};

/* State management functions */
int condition_state_init(void);
void condition_state_cleanup(void);
struct condition_state *condition_state_get(const char *name);
void condition_state_put(struct condition_state *state);
bool condition_state_enabled(const struct condition_state *state);

#endif /* _NFT_CONDITION_STATE_H_ */