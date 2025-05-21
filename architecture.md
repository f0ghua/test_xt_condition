# NFTables Condition Module Architecture

## 1. System Architecture Overview

### 1.1 Overall Architecture
```mermaid
graph TB
    subgraph User Space
        CLI[NFTables CLI]
        subgraph Parser Layer
            LP[Lexical Parser]
            SP[Syntax Parser]
            AST[AST Builder]
        end
        subgraph Expression Layer
            EXP[Expression Builder]
            NET[Netlink Message Generator]
        end
        Tool[Management Tools]
    end
    
    subgraph Kernel Space
        NF[NFTables Core]
        subgraph Condition Module
            EH[Expression Handler]
            SM[State Manager]
            PM[Procfs Manager]
        end
        PF[procfs]
    end
    
    CLI --> LP
    LP --> SP
    SP --> AST
    AST --> EXP
    EXP --> NET
    NET --> |Netlink| NF
    Tool --> |procfs| PF
    NF --> |invoke| EH
    EH --> |query| SM
    SM --> |manage| PM
    PM --> |file ops| PF
```

### 1.2 User Space Processing Flow
```mermaid
sequenceDiagram
    participant U as User
    participant L as Lexical Parser
    participant S as Syntax Parser
    participant A as AST Builder
    participant E as Expression Builder
    participant N as Netlink Generator
    participant K as Kernel

    U->>L: Input "condition foo"
    L->>L: Lexical analysis
    L->>S: Token stream
    S->>S: Syntax analysis
    S->>A: Parse tree nodes
    A->>A: Build AST
    A->>E: AST nodes
    E->>E: Build expression
    E->>N: Expression object
    N->>N: Generate Netlink msg
    N->>K: Send to kernel
```

## 2. Core Components Design

### 2.1 User Space Components

#### 2.1.1 Parser Components
1. Lexical Parser
   ```c
   // Token types
   enum token_type {
       TOKEN_CONDITION,
       TOKEN_STRING,
       TOKEN_IDENTIFIER,
       // ...
   };

   struct token {
       enum token_type type;
       char *value;
       size_t line;
       size_t column;
   };
   ```

2. Syntax Parser
   ```c
   // AST node types
   enum node_type {
       NODE_CONDITION,
       NODE_EXPRESSION,
       NODE_IDENTIFIER,
       // ...
   };

   struct ast_node {
       enum node_type type;
       struct ast_node *left;
       struct ast_node *right;
       void *data;
   };
   ```

3. Expression Builder
   ```c
   struct nft_expr_info {
       const char *type;
       void *data;
       unsigned int size;
   };

   struct nft_condition_expr {
       char name[NFT_NAME_MAX];
       uint32_t flags;
   };
   ```

### 2.2 Kernel Space Components

#### 2.2.1 Expression Handler
```mermaid
classDiagram
    class ExpressionHandler {
        +register_type()
        +init_rule()
        +evaluate()
        +cleanup()
    }
    class NFTablesCore {
        +process_rule()
        +evaluate_expr()
    }
    NFTablesCore --> ExpressionHandler
```

#### 2.2.2 State Manager
```mermaid
stateDiagram-v2
    [*] --> Init
    Init --> Active: Create condition
    Active --> InUse: Rule reference
    InUse --> InUse: State toggle
    InUse --> Cleanup: RefCount zero
    Cleanup --> [*]
```

## 3. Key Workflows

### 3.1 Rule Creation Flow
```mermaid
sequenceDiagram
    participant C as CLI
    participant P as Parser
    participant N as Netlink
    participant K as Kernel
    participant S as State Manager

    C->>P: Input command
    P->>P: Parse & validate
    P->>N: Build message
    N->>K: Send request
    K->>S: Create state
    S-->>K: State ready
    K-->>N: Success
    N-->>C: Rule created
```

### 3.2 State Management
```mermaid
flowchart TB
    A[State Request] --> B{Check Permission}
    B -->|Success| C[Acquire Lock]
    B -->|Failure| D[Error Return]
    C --> E[Update State]
    E --> F[Release Lock]
    F --> G[Notify Listeners]
```

## 4. Resource Management

### 4.1 Memory Management Strategy
```mermaid
graph TD
    A[Allocation Request] --> B{Memory Pool}
    B -->|Available| C[Allocate Block]
    B -->|Full| D[System Alloc]
    C --> E[Track Reference]
    D --> E
    E --> F{Zero Refs?}
    F -->|Yes| G[Release Memory]
    F -->|No| H[Keep Active]
```

### 4.2 Lock Hierarchy
1. Global State Lock
   - Protects condition list
   - Coarse-grained synchronization

2. Per-Condition Lock
   - Protects individual state
   - Fine-grained operations

3. Atomic Operations
   - State toggles
   - Reference counting

## 5. Error Handling

### 5.1 Error Flow
```mermaid
flowchart TD
    A[Error Detection] --> B{Error Type}
    B -->|Resource| C[ENOMEM]
    B -->|Permission| D[EPERM]
    B -->|Invalid| E[EINVAL]
    B -->|Not Found| F[ENOENT]
    C & D & E & F --> G[Log Error]
    G --> H[Recovery]
```

### 5.2 Recovery Strategy
1. Transaction Rollback
   - Revert partial changes
   - Clean up resources
   - Maintain consistency

2. Error Propagation
   - Clear error codes
   - Detailed logging
   - User feedback

## 6. Performance Considerations

### 6.1 Critical Paths
1. Rule Evaluation
   - Optimize state lookup
   - Minimize lock contention
   - Cache hot conditions

2. State Updates
   - Batch processing
   - Efficient synchronization
   - Minimize copying

### 6.2 Performance Metrics
```mermaid
graph LR
    A[Evaluation] -->|< 1μs| B[Latency Target]
    C[Concurrency] -->|10k/s| D[Throughput]
    E[Memory] -->|< 4KB/condition| F[Resource Limit]
```

## 7. Security Design

### 7.1 Access Control
1. Rule Creation
   - Root only
   - Validation checks
   - Resource limits

2. State Modification
   - Configurable permissions
   - Audit logging
   - Rate limiting

### 7.2 Resource Protection
1. Input Validation
   - Size limits
   - Format checks
   - Sanitization

2. Resource Limits
   - Memory caps
   - File descriptor limits
   - CPU usage bounds

This architecture document provides a comprehensive view of both user space and kernel space components, their interactions, and key design decisions for the NFTables condition module. It serves as the foundation for detailed specification development and implementation planning.