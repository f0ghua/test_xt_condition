
# nftables 自定义模块需求文档 (修订版)

本项目基于以下软件版本开发：
- **nftables**: v1.1.1
- **libnftnl**: v1.2.8
- **Linux kernel**: v5.15

---
## 一、模块名称：`condition`

### 1.1 功能概述
`condition` 模块是一个 nftables 表达式 (expression)，用于根据 `/proc/condition/<name>` 条目的值动态控制某条防火墙规则是否生效。

-   规则中通过 `condition "name_string"` 语法指定一个字符串参数 `<name>`。
-   内核模块在加载包含此表达式的规则时，会自动管理 `/proc/condition/` 目录及其中对应的 `<name>` 文件。
-   每次规则匹配评估到 `condition` 表达式时，内核会检查与 `<name>` 关联的内部状态值：
    -   状态值为 `1`：`condition` 表达式评估为真，规则匹配继续。
    -   状态值为 `0`：`condition` 表达式评估为假，规则视为不匹配，跳过该规则。

### 1.2 使用示例
```nft
table ip filter {
    chain input {
        type filter hook input priority filter; policy accept;

        # 示例1: 仅当 /proc/condition/wifi_enabled 内容为 "1" 时，才允许来自 wlan0 的 SSH 流量
        iifname "wlan0" tcp dport 22 condition "wifi_enabled" accept

        # 示例2: 仅当 /proc/condition/maintenance_mode 内容为 "1" 时，拒绝所有新的 TCP 连接
        meta nfproto ipv4 tcp flags syn condition "maintenance_mode" reject with tcp reset
    }
}
```
此规则仅在对应的 `/proc/condition/<name>` 文件所代表的状态为 `1` 时生效。

### 1.3 用户空间接口说明 (`/proc/condition/`)

1.  **目录管理**:
    *   `/proc/condition/` 目录由 `condition` 内核模块在加载第一条使用 `condition` 表达式的规则时自动创建（如果尚不存在）。
    *   该目录在内核模块卸载时自动移除。

2.  **文件管理**:
    *   每个在规则中使用的唯一字符串参数 `<name>` 都会在 `/proc/condition/` 目录下对应一个名为 `<name>` 的文件。
    *   **创建**: 当第一条使用特定 `<name>` 的规则被加载时，对应的 `/proc/condition/<name>` 文件被自动创建。
    *   **初始内容**: 新创建的 `/proc/condition/<name>` 文件，其代表的内核状态初始设置为 `0` (禁用)。用户读取该 proc 文件时，应看到 `"0\n"`。
    *   **权限**: `/proc/condition/<name>` 文件的权限设置为 `0644` (owner: read-write, group: read, other: read)。
    *   **生命周期**: 当最后一条使用特定 `<name>` 的规则从内核中被删除时，对应的 `/proc/condition/<name>` 文件将被自动删除。
    *   **共享**: 若多条规则使用相同的 `<name>` 参数，它们将共享同一个 `/proc/condition/<name>` 文件及其状态。

3.  **状态控制**:
    *   用户空间通过向对应的 proc 文件写入字符串来控制状态：
        ```bash
        echo 1 > /proc/condition/wifi_enabled  # 启用相关规则
        echo 0 > /proc/condition/wifi_enabled  # 禁用相关规则
        ```
    *   **内容解析**:
        *   写入时，模块会解析写入的字符串。首尾的空白字符（如空格、换行符）将被忽略。
        *   有效内容为 `"1"` (启用) 或 `"0"` (禁用)。
        *   若写入其他无效内容 (如 `"2"`, `"abc"`, 空字符串等)，操作将被拒绝，内核状态不变，并可能返回写入错误给用户空间。内核应记录一条警告日志。
    *   **读取**: 读取该文件将返回当前状态，格式为 `"1\n"` 或 `"0\n"`。

4.  **`<name>` 参数约束**:
    *   允许字符：字母 (a-z, A-Z)、数字 (0-9)、下划线 (`_`)、中划线 (`-`)。
    *   禁止字符：不允许包含斜杠 (`/`)、空格及其他 procfs 文件名通常不支持的特殊字符。
    *   长度限制：最大长度为 `254` 字节 (考虑到 `NAME_MAX` 通常为 255，并为末尾空字符留出空间)。

### 1.4 技术实现要点

#### 1.4.1 内核空间实现 (Netfilter Expression)

1.  **模块与表达式注册**:
    *   实现为一个 Netfilter `nft_expr_type`。
    *   关键回调函数：`init` (规则对象初始化), `destroy` (规则对象销毁), `eval` (匹配时评估), `dump` (规则信息输出)。

2.  **Procfs 管理与状态存储**:
    *   **`init` 回调**:
        *   解析 Netlink 消息获取 `<name>` 字符串。
        *   验证 `<name>` 的合法性 (字符、长度)。
        *   管理 `/proc/condition/` 目录的创建。
        *   为每个唯一的 `<name>`：
            *   维护一个引用计数。
            *   如果首次使用，创建 `/proc/condition/<name>` procfs 条目，并关联 `file_operations`。
            *   在表达式私有数据 (`priv`) 中存储指向该 `<name>` 关联的内部状态数据结构的指针或标识。
            *   内部状态数据结构应包含一个 `atomic_t` (或类似受保护的 `bool`) 变量用于存储 `0` 或 `1` 状态，以及 `<name>` 字符串本身、引用计数等。初始状态设为 `0`。
    *   **`destroy` 回调**:
        *   减少对应 `<name>` 的引用计数。
        *   若引用计数归零，则移除对应的 `/proc/condition/<name>` procfs 条目并释放相关资源。
    *   **`file_operations` (for procfs files)**:
        *   `read`: 返回内部状态变量的值 (格式化为 `"0\n"` 或 `"1\n"`)。
        *   `write`: 解析输入，验证后更新内部状态变量 (`atomic_set`)。确保并发安全。对无效输入返回错误并记录内核日志。
    *   **状态缓存与并发**:
        *   `eval` 回调直接读取内部 `atomic_t` 状态变量，**不进行文件 I/O 操作**，以保证高性能。
        *   对内部状态变量的访问（主要由 procfs `write` 修改，`eval` 读取）必须是并发安全的。`atomic_t` 提供了这种保证。引用计数的修改也需原子化。

3.  **匹配逻辑 (`eval` 回调)**:
    *   根据表达式私有数据中获取的内部状态变量的值 (`0` 或 `1`)，决定表达式的评估结果 (true/false)。

4.  **错误处理与日志**:
    *   在 procfs 文件创建失败、`<name>` 无效、内存分配失败等情况下，应通过 Netlink 向用户空间返回错误。
    *   内核应使用 `pr_warn` 或 `pr_err` 记录关键错误和警告（如无效的 procfs 写入）。

#### 1.4.2 用户空间实现 (nftables CLI & libnftnl 相关)

1.  **语法解析 (nftables CLI)**:
    *   **词法分析 (`scanner.l`)**: 新增关键字 `T_CONDITION` (或类似) 对应字符串 `"condition"`。
    *   **语法分析 (`parser_y.y` 或 `parser_bison.y`)**:
        *   新增表达式语法规则，如 `condition_expr: T_CONDITION T_STRING;`
        *   解析后生成内部表达式结构体 (`struct nft_expr`)，类型设置为自定义的内部枚举值 (如 `NFT_EXPR_MY_CONDITION`)。
        *   将解析到的 `<name>` 字符串 (来自 `T_STRING`) 存储到表达式对象的私有数据中。

2.  **表达式操作定义 (`src/expr.c` 或新建 `src/expr_condition.c`)**:
    *   为 `condition` 表达式实现 `struct nft_expr_ops`，包括：
        *   `parse`: 将词法/语法分析结果转换为内部结构。
        *   `print`: 将表达式格式化为用户可读字符串 (如 `condition "wifi_enabled"`)，用于 `nft list ruleset`。
        *   `eval` (用户空间侧，可选，主要用于模拟或静态分析，核心在内核)。
        *   `clone`, `free_priv` 等辅助函数。

3.  **Netlink 消息处理**:
    *   **序列化 (nftables CLI -> Kernel)**:
        *   定义一个新的 Netlink 属性类型，例如 `NFTA_CONDITION_NAME` (自定义枚举值)。
        *   在表达式的 `ops->dump` (或等效的序列化函数) 中，将 `<name>` 字符串使用 `mnl_attr_put_strz` (或 libnftnl 对应函数 `nftnl_expr_set_str`) 打包为 `NFTA_CONDITION_NAME` 属性，添加到 Netlink 消息中。
        *   **libnftnl 考虑**: 优先尝试在 nftables CLI 中直接使用 libnftnl 提供的通用 Netlink 属性构建函数（如 `nftnl_expr_set_data` 或创建自定义属性的底层接口），而不是立即修改 libnftnl 库本身。这对于 OpenWrt 环境更易于集成。
    *   **反序列化 (Kernel -> nftables CLI, for `list ruleset`)**:
        *   在表达式的 `ops->parse_payload` (或等效的解析函数) 中，从 Netlink 消息中提取 `NFTA_CONDITION_NAME` 属性，并将 `<name>` 字符串存储回用户空间的表达式结构体。

4.  **错误处理与反馈 (nftables CLI)**:
    *   nftables CLI 需要能够处理从内核返回的与 `condition` 表达式相关的错误。
    *   利用 Netlink 消息中包含的位置信息 (line, col)，将错误准确定位到用户输入的 `condition "<name>"` 部分。
    *   在用户空间对 `<name>` 进行初步校验（如长度、非法字符），尽早发现问题。

---

请检查这份修订后的文档是否准确反映了我们的讨论结果，并且是否清晰、完整。如果你发现任何错误或有进一步的修改意见，请随时提出。