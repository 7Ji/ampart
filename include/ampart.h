#pragma once
#include <stdio.h>

#define AMPART_LIB_MAJOR 0
#define AMPART_LIB_MINOR 0
#define AMPART_LIB_PATCH 1

#define AMPART_OK       0
#define AMPART_BAD      1

/**
 * Return type used in ampart, the only possible values that would be returned
 * by ampart are `AMPART_OK (0)` and `AMPART_BAD (1)`
 */
typedef unsigned char ampart_status;

/**
 * Initialize ampart's inner routine, users are expected to call this before
 * calling any other `ampart_` functions, and call `ampart_free()` after they
 * finish using ampart
 * @param log_out The file poiner to write log into
 * @return The initialization status
 */
ampart_status ampart_init(FILE *log_out);

/**
 * Free all resources possibly allocaed in `ampart_init`, currently this does
 * nothing, but you should already check for its return, in case of future
 * changes
 * @return The freeing status
 */
ampart_status ampart_free();

/**
 * Partition argument mini-language opaque type, initialized and parsed from
 * string via `ampart_parg_from_str()`, users are expected to free them later
 * via `ampart_parg_free()`
 */
typedef struct ampart_parg *ampart_parg;


/**
 * Parse and initialize parg from a NULL-terminiated string, users are expected
 * to free the parsed parg later using `ampart_parg_free()`
 * @param parg Pointer to an opaque `ampart_parg`
 * @param str The source string
 * @param len_str Length of the given string, optional, if it is `0` then ampart
 * would calculates it internally
 * @return The parsing status
 */
ampart_status ampart_parg_from_str(ampart_parg *parg, char const *str, size_t len_str);

/**
 * Generate the corresponding NULL-terminated string of a parg
 * @param parg An opaque `ampart_parg`
 * @param str The target string buffer, the string is guaranteed to be `NULL` -
 * terminated, even if it would be truncated (see below)
 * @param maxlen_str The maximum lenth of string (excluding `NULL`) that `str`
 * could hold, if `len_str + 1` would >= `maxlen_str`, then `len_str` is limited
 * to `maxlen_str - 1`, and `str[maxlen_str - 1]` would be set to `NULL`
 * @param len_str Pointer to where the result length (excluding `NULL`) shall be
 * stored, optional, if it is `NULL` then no result would be saved.
 * @return The generation status
 */
ampart_status ampart_parg_to_str(ampart_parg const parg, char *str, size_t maxlen_str, size_t *len_str);

ampart_status ampart_parg_free(ampart_parg parg);

#define AMPART_ACTION_NONE              0b00000000

#define AMPART_READ_NONE                0b00000000
#define AMPART_READ_CONTENT_NONE        0b00000000
#define AMPART_READ_CONTENT_DTB         0b00000001
#define AMPART_READ_CONTENT_RESERVED    0b00000010
#define AMPART_READ_CONTENT_EMMC        0b00000011
#define AMPART_READ_CONTENT_MASK        0b00000011
#define AMPART_READ_FTYPE_NONE          0b00000000
#define AMPART_READ_FTYPE_REGUAR        0b00000100
#define AMPART_READ_FTYPE_DEVNODE       0b00001000
#define AMPART_READ_FTYPE_MASK          0b00001100

#define AMPART_MOD_NONE                 0b00000000
#define AMPART_MOD_UPDATE_NONE          0b00000000
#define AMPART_MOD_UPDATE_DTB           0b00000001
#define AMPART_MOD_UPDATE_EPT           0b00000010
#define AMPART_MOD_UPDATE_MASK          0b00000011

#define AMPART_REPORT_NONE              0b00000000
#define AMPART_REPORT_SNAPSHOT_NONE     0b00000000
#define AMPART_REPORT_SNAPSHOT_DECIMAL  0b00000001
#define AMPART_REPORT_SNAPSHOT_HEX      0b00000010
#define AMPART_REPORT_SNAPSHOT_HUMAN    0b00000100
#define AMPART_REPORT_SNAPSHOT_ALL      0b00000111
#define AMPART_REPORT_SNAPSHOT_MASK     0b00000111
#define AMPART_REPORT_WEB_NONE          0b00000000
#define AMPART_REPORT_WEB_DTB           0b00001000
#define AMPART_REPORT_WEB_EPT           0b00010000
#define AMPART_REPORT_WEB_BOTH          0b00011000
#define AMPART_REPORT_WEB_MASK          0b00011000
#define AMPART_REPORT_ALL               0b00011111

#define AMPART_WRITE_NONE               0b00000000
#define AMPART_WRITE_DTB                0b00000001
#define AMPART_WRITE_EPT                0b00000010
#define AMPART_WRITE_PARTS              0b00000011
#define AMPART_MIGRATE_NONE             0b00000000
#define AMPART_MIGRATE_ESSENTIAL        0b00000100
#define AMPART_MIGRATE_ALL              0b00001100
#define AMPART_MIGRATE_MASK             0b00001100

/**
 * What actions ampart routine should take during a certain `plan`
 */
typedef struct ampart_actions {
    /**
     * What actions ampart reading routine should take: an `OR` -ed byte of
     * `AMPART_READ_` flags
     */
    unsigned char read;
    /**
     * What actions ampart modifying routine should take: an `OR` -ed byte of
     * `AMPART_MOD_` flags
     */
    unsigned char mod;
    /**
     * What actions ampart reporting routine should take: an `OR` -ed byte of
     * `AMPART_REPORT_` flags
     */
    unsigned char report;
    /**
     * What actions ampart writing routine should take: an `OR` -ed byte of
     * `AMPART_WRITE_` flags
     */
    unsigned char write;
} ampart_actions;

/**
 * Plan opaque type, initialized from `ampart_plan_create()` (with `ampart_parg`
 * ) or `ampart_plan_create_raw()` (with string pargs)
 */
typedef struct ampart_plan *ampart_plan;

/**
 * Create an ampart plan with certain actions and earlier parsed `ampart_parg`s.
 * Users shall call `ampart_plan_free` to free the allocated plan later.
 * @param plan Pointer to an opaque `ampart_plan`
 * @param actions What actions ampart shall do
 * @param pargs Pointer to a list of pargs
 * @param len_pargs Count of pargs
 * @return The creation status
 */
ampart_status ampart_plan_create(ampart_plan *plan, ampart_actions actions, ampart_parg const *const *pargs, size_t len_pargs);

/**
 * Create an ampart plan with certain actions and string pargs that could be
 * parsed into `ampart_parg`s.
 * Users shall call `ampart_plan_free` to free the allocated plan later.
 * @param plan Pointer to an opaque `ampart_plan`
 * @param actions What actions ampart shall do
 * @param pargs Pointer to a list of strings that each can be parsed into a
 * `ampart_parg`
 * @param len_pargs Count of pargs
 * @return The creation status
 */
ampart_status ampart_plan_create_raw(ampart_plan *plan, ampart_actions actions, char const *const *pargs, size_t len_pargs);

/**
 * Free an opaque `ampart_plan`
 * @param plan The plan to free
 * @return The freeing status
 */
ampart_status ampart_plan_free(ampart_plan plan);

/**
 * An opaque type pointing to a target, with certain meta info associated with it
 */
typedef struct ampart_target *ampart_target;

/**
 * Create an opaque target to work on, with explicit offset, gap, etc. If target
 * is a common, non-Xiaomi target, users could use `ampart_target_create_common`
 * instead
 * @param path The path to target
 * @param len_path The length of path, optional, if it is `0` then it's figured
 * out internally
 * @param snapshot_out The file poiner to which snapshots shall be printed, only
 * used with a plan that has snapshot output
 * @param offset_reserved The offset of reserved partition in disk, in bytes
 * @param offset_dtb The offset of DTB in reserved partition, in bytes
 * @param gap_partition The gap between partitions, in bytes
 * @return The creation status
 */
ampart_status ampart_target_create(char const *path, size_t len_path, FILE *snapshot_out, size_t offset_reserved, size_t offset_dtb, size_t gap_partition);

#define AMPART_COMMON_OFFSET_RESERVED   0x2400000U // 36M
#define AMPART_COMMON_OFFSET_DTB        0x400000U  // 4M
#define AMPART_COMMON_GAP_PARTITION     0x800000U  // 8M

#define ampart_target_create_common(path, len_path, snapshot_out) \
    ampart_target_create(path, len_path, snapshot_out, AMPART_COMMON_OFFSET_RESERVED, AMPART_COMMON_OFFSET_DTB, AMPART_COMMON_GAP_PARTITION)

/**
 * Free an opaque `ampart_target`
 * @param target The target to free
 * @return The freeing status
 */
ampart_status ampart_target_free(ampart_target target);

/**
 * Work on an `ampart_target` according to an `ampart_plan`
 * @param plan The plan to use
 * @param target The target to work on
 * @return The work status
 */
ampart_status ampart_work(ampart_plan plan, ampart_target target);

/**
 * A shortcut to call `ampart_init`, create an `ampart_plan`, an `ampart_target`
 * then do `ampart_work`, and finally call `ampart_free` after freeing both the
 * `plan` and `target`; If target is a common, non-Xiaomi target, then users
 * can call `ampat_oneshot_common` instead
 */
ampart_status ampart_oneshot(FILE *log_out, ampart_actions actions, char const *restrict *parg, size_t len_pargs, char const *path, size_t len_path, FILE *snapshot_out, size_t offset_reserved, size_t offset_dtb, size_t gap_partition);

#define ampart_oneshot_common(log_out, actions, parg, len_pargs, path, len_path, snapshot_out) \
    ampart_oneshot(log_out, actions, parg, len_pargs, path, len_path, snapshot_out, AMPART_COMMON_OFFSET_RESERVED, AMPART_COMMON_OFFSET_DTB, AMPART_COMMON_GAP_PARTITION)
