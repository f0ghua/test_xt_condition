// SPDX-License-Identifier: GPL-2.0
/*
 * nftables condition expression support
 */
#include <nftables/libnftables.h>
#include <erec.h>
#include <expression.h>
#include <rule.h>
#include <statement.h>
#include <string.h>
#include <netlink.h>
#include <utils.h>

/* Structure holding our condition expression data */
struct nft_expr_condition {
    const char *name;
};

static const struct nftnl_expr_ops *condition_get_ops(void)
{
    return &nft_expr_condition_ops;
}

/* Expression parsing */
static int condition_parse(struct nft_ctx *ctx, struct expr **expr)
{
    struct nft_expr_condition *cond;
    struct expr *expr_tmp;
    const char *name;

    /* Parse condition name */
    if (ctx->scanner->token != TOKEN_STRING) {
        return expr_error(ctx, "Expected string for condition name");
    }
    name = ctx->scanner->buffer;

    /* Validate name length */
    if (strlen(name) >= NAME_MAX) {
        return expr_error(ctx, "Condition name too long");
    }

    /* Create expression */
    expr_tmp = expr_alloc(ctx, EXPR_CONDITION, sizeof(*cond));
    if (!expr_tmp) {
        return expr_error(ctx, "Out of memory");
    }

    /* Initialize condition data */
    cond = expr_data(expr_tmp);
    cond->name = xstrdup(name);

    *expr = expr_tmp;
    return 0;
}

/* Expression printing */
static int condition_print(struct nft_expr_condition *cond, struct output_ctx *octx)
{
    return nft_print(octx, "condition \"%s\"", cond->name);
}

/* Expression serialization to netlink */
static int condition_nlmsg_build(struct netlink_linearize_ctx *ctx,
                               struct nftnl_expr *nle,
                               struct expr *expr)
{
    struct nft_expr_condition *cond = expr_data(expr);

    if (nftnl_expr_set_str(nle, NFTA_CONDITION_NAME, cond->name)) {
        return -1;
    }

    return 0;
}

/* Expression deserialization from netlink */
static int condition_nlmsg_parse(struct nftnl_expr *nle, struct expr *expr)
{
    struct nft_expr_condition *cond = expr_data(expr);
    const char *name;

    name = nftnl_expr_get_str(nle, NFTA_CONDITION_NAME);
    if (!name) {
        return -1;
    }

    cond->name = xstrdup(name);
    return 0;
}

/* Expression type definition */
const struct expr_ops expr_ops_condition = {
    .type        = EXPR_CONDITION,
    .name        = "condition",
    .parse       = condition_parse,
    .print       = condition_print,
    .json        = condition_json,
    .nlmsg_build = condition_nlmsg_build,
    .nlmsg_parse = condition_nlmsg_parse,
    .get_ops     = condition_get_ops,
};