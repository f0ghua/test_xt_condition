// SPDX-License-Identifier: GPL-2.0
/*
 * nftables condition expression state management
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "condition_state.h"

/* Global state management */
static struct condition_mgr {
    struct list_head conditions;   /* List of all conditions */
    struct mutex lock;            /* Protects list modifications */
    struct proc_dir_entry *proc_dir; /* /proc/condition directory */
} condition_mgr;

/* procfs file operations */
static ssize_t condition_proc_write(struct file *file, const char __user *buf,
                                  size_t count, loff_t *ppos)
{
    struct condition_state *state = PDE_DATA(file_inode(file));
    char kbuf[2]; /* 1 character + null terminator */
    int val;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(kbuf, buf, 1))
        return -EFAULT;

    /* Parse input value */
    if (kbuf[0] == '1')
        val = 1;
    else if (kbuf[0] == '0')
        val = 0;
    else {
        pr_warn("condition: invalid value written to %s\n", state->name);
        return -EINVAL;
    }

    /* Update state atomically */
    atomic_set(&state->enabled, val);
    
    return count;
}

static int condition_proc_show(struct seq_file *m, void *v)
{
    struct condition_state *state = m->private;
    int val = atomic_read(&state->enabled);
    
    seq_printf(m, "%d\n", val);
    return 0;
}

static int condition_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, condition_proc_show, PDE_DATA(inode));
}

static const struct proc_ops condition_proc_ops = {
    .proc_open    = condition_proc_open,
    .proc_read    = seq_read,
    .proc_write   = condition_proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Create new condition state */
static struct condition_state *condition_state_create(const char *name)
{
    struct condition_state *state;
    struct proc_dir_entry *entry;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return ERR_PTR(-ENOMEM);

    /* Initialize state */
    atomic_set(&state->enabled, 0);
    atomic_set(&state->refcount, 1);
    mutex_init(&state->lock);
    INIT_LIST_HEAD(&state->list);
    strncpy(state->name, name, NAME_MAX - 1);
    state->name[NAME_MAX - 1] = '\0';

    /* Create procfs entry */
    entry = proc_create_data(name, 0644, condition_mgr.proc_dir,
                            &condition_proc_ops, state);
    if (!entry) {
        kfree(state);
        return ERR_PTR(-ENOMEM);
    }
    state->proc = entry;

    /* Add to global list */
    mutex_lock(&condition_mgr.lock);
    list_add(&state->list, &condition_mgr.conditions);
    mutex_unlock(&condition_mgr.lock);

    return state;
}

/* Find existing condition state by name */
static struct condition_state *condition_state_find(const char *name)
{
    struct condition_state *state;

    mutex_lock(&condition_mgr.lock);
    list_for_each_entry(state, &condition_mgr.conditions, list) {
        if (strcmp(state->name, name) == 0) {
            atomic_inc(&state->refcount);
            mutex_unlock(&condition_mgr.lock);
            return state;
        }
    }
    mutex_unlock(&condition_mgr.lock);

    return NULL;
}

/* Get or create condition state */
struct condition_state *condition_state_get(const char *name)
{
    struct condition_state *state;

    /* Try to find existing state */
    state = condition_state_find(name);
    if (state)
        return state;

    /* Create new state if not found */
    return condition_state_create(name);
}

/* Release condition state */
void condition_state_put(struct condition_state *state)
{
    if (!atomic_dec_and_test(&state->refcount))
        return;

    /* Last reference dropped, clean up */
    mutex_lock(&condition_mgr.lock);
    list_del(&state->list);
    mutex_unlock(&condition_mgr.lock);

    if (state->proc)
        remove_proc_entry(state->name, condition_mgr.proc_dir);

    kfree(state);
}

/* Get condition state value */
bool condition_state_enabled(const struct condition_state *state)
{
    return atomic_read(&state->enabled) != 0;
}

/* Initialize condition state management */
int condition_state_init(void)
{
    mutex_init(&condition_mgr.lock);
    INIT_LIST_HEAD(&condition_mgr.conditions);

    /* Create /proc/condition directory */
    condition_mgr.proc_dir = proc_mkdir("condition", NULL);
    if (!condition_mgr.proc_dir)
        return -ENOMEM;

    return 0;
}

/* Clean up condition state management */
void condition_state_cleanup(void)
{
    struct condition_state *state, *tmp;

    /* Remove all procfs entries and free states */
    mutex_lock(&condition_mgr.lock);
    list_for_each_entry_safe(state, tmp, &condition_mgr.conditions, list) {
        if (state->proc)
            remove_proc_entry(state->name, condition_mgr.proc_dir);
        list_del(&state->list);
        kfree(state);
    }
    mutex_unlock(&condition_mgr.lock);

    /* Remove /proc/condition directory */
    if (condition_mgr.proc_dir)
        remove_proc_entry("condition", NULL);
}

EXPORT_SYMBOL_GPL(condition_state_get);
EXPORT_SYMBOL_GPL(condition_state_put);
EXPORT_SYMBOL_GPL(condition_state_enabled);