// SPDX-License-Identifier: GPL-2.0
/*
 * nftables condition expression module
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <net/netfilter/nf_tables.h>
#include "condition_state.h"

/* Condition expression private data */
struct nft_condition {
    struct condition_state *state;  /* Points to shared state */
    char name[NAME_MAX];           /* Cached name for dumping */
};

/* Expression type info */
static const struct nla_policy nft_condition_policy[NFTA_CONDITION_MAX + 1] = {
    [NFTA_CONDITION_NAME] = { .type = NLA_STRING,
                             .len = NAME_MAX - 1 },
};

/* Initialize condition expression from netlink attributes */
static int nft_condition_init(const struct nft_ctx *ctx,
                            const struct nft_expr *expr,
                            const struct nlattr * const tb[])
{
    struct nft_condition *cond = nft_expr_priv(expr);
    const char *name;
    int ret;

    if (!tb[NFTA_CONDITION_NAME])
        return -EINVAL;

    name = nla_data(tb[NFTA_CONDITION_NAME]);
    if (strlen(name) >= NAME_MAX)
        return -ENAMETOOLONG;

    /* Get or create condition state (handles atomic refcounting) */
    cond->state = condition_state_get(name);
    if (IS_ERR(cond->state))
        return PTR_ERR(cond->state);

    /* Cache name for dumping */
    strncpy(cond->name, name, NAME_MAX - 1);
    cond->name[NAME_MAX - 1] = '\0';

    return 0;
}

/* Clean up condition expression */
static void nft_condition_destroy(const struct nft_ctx *ctx,
                                const struct nft_expr *expr)
{
    struct nft_condition *cond = nft_expr_priv(expr);

    if (cond->state)
        condition_state_put(cond->state); /* Decrements refcount atomically */
}

/* Evaluate condition - directly reads atomic state for performance */
static void nft_condition_eval(const struct nft_expr *expr,
                             struct nft_regs *regs,
                             const struct nft_pktinfo *pkt)
{
    const struct nft_condition *cond = nft_expr_priv(expr);

    /* Fast path: atomic read of cached state without any I/O */
    regs->verdict.code = condition_state_enabled(cond->state) ? 
                        NFT_CONTINUE : NFT_BREAK;
}

/* Dump condition expression info */
static int nft_condition_dump(struct sk_buff *skb,
                            const struct nft_expr *expr)
{
    const struct nft_condition *cond = nft_expr_priv(expr);

    if (nla_put_string(skb, NFTA_CONDITION_NAME, cond->name))
        goto nla_put_failure;

    return 0;

nla_put_failure:
    return -1;
}

static struct nft_expr_type nft_condition_type;
static const struct nft_expr_ops nft_condition_ops = {
    .type       = &nft_condition_type,
    .size       = NFT_EXPR_SIZE(sizeof(struct nft_condition)),
    .eval       = nft_condition_eval,
    .init       = nft_condition_init,
    .destroy    = nft_condition_destroy,
    .dump       = nft_condition_dump,
};

static struct nft_expr_type nft_condition_type __read_mostly = {
    .name       = "condition",
    .ops        = &nft_condition_ops,
    .policy     = nft_condition_policy,
    .maxattr    = NFTA_CONDITION_MAX,
    .owner      = THIS_MODULE,
};

/* Module initialization */
static int __init nft_condition_module_init(void)
{
    int ret;

    /* Initialize global state management */
    ret = condition_state_init();
    if (ret < 0)
        return ret;

    ret = nft_register_expr(&nft_condition_type);
    if (ret < 0)
        goto err_state;

    return 0;

err_state:
    condition_state_cleanup();
    return ret;
}

/* Module cleanup */
static void __exit nft_condition_module_exit(void)
{
    nft_unregister_expr(&nft_condition_type);
    condition_state_cleanup();
}

module_init(nft_condition_module_init);
module_exit(nft_condition_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name <your.email@example.com>");
MODULE_DESCRIPTION("nftables condition expression support");
MODULE_ALIAS_NFT_EXPR("condition");