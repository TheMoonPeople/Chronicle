#pragma once

/**
 * One buffer a component sound library module reads or writes.
 */
struct sceCslBuffCtx {
    int sema;   /**< Semaphore guarding the buffer, or zero. */
    void *buff; /**< The buffer. */
};

/**
 * A group of buffers a module reads from or writes to.
 */
struct sceCslBuffGrp {
    int buffNum;            /**< Number of buffers in the group. */
    sceCslBuffCtx *buffCtx; /**< The buffers. */
};

/**
 * The context one component sound library module runs in.
 */
struct sceCslCtx {
    int buffGrpNum;         /**< Number of buffer groups. */
    sceCslBuffGrp *buffGrp; /**< The buffer groups, input first. */
    void *conf;             /**< Module configuration, or zero. */
    void *callBack;         /**< Callback table, or zero. */
    char **extmod;          /**< Extension modules, or zero. */
};
