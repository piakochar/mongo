/*-
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Initialize a static WT_ASYNC_OP structure.
 */
#define	WT_ASYNC_OP_STATIC_INIT(n,					\
	get_key,							\
	get_value,							\
	set_key,							\
	set_value,							\
	search,								\
	insert,								\
	update,								\
	remove,								\
	get_id)								\
	static const WT_ASYNC_OP n = {					\
	NULL,					/* connection */	\
	NULL,					/* uri */		\
	NULL,					/* config */		\
	0,					/* uri_cfg_hash */	\
	NULL,					/* key_format */	\
	NULL,					/* value_format */	\
	(int (*)(WT_ASYNC_OP *, ...))(get_key),				\
	(int (*)(WT_ASYNC_OP *, ...))(get_value),			\
	(void (*)(WT_ASYNC_OP *, ...))(set_key),			\
	(void (*)(WT_ASYNC_OP *, ...))(set_value),			\
	search,								\
	insert,								\
	update,								\
	remove,								\
	get_id,								\
	{ NULL, NULL },			/* TAILQ_ENTRY q */		\
	0,				/* recno key */			\
	{ 0 },				/* recno raw buffer */		\
	NULL,				/* lang_private */		\
	{ NULL, 0, 0, NULL, 0 }		/* WT_ITEM key */		\
	{ NULL, 0, 0, NULL, 0 }		/* WT_ITEM value */		\
	NULL,				/* WT_ASYNC_CALLBACK cb */	\
	0,				/* int saved_err */		\
	0,				/* uint32_t internal_id */	\
	0,				/* uint64_t unique_id */	\
	0,				/* uint32_t state */		\
	0,				/* WT_ASYNC_OPTYPE optype */	\
	0				/* uint32_t flags */		\
}

struct __wt_async_callback;
		typedef struct __wt_async_callback WT_ASYNC_CALLBACK;

/*! Asynchronous operation types. */
typedef enum {
	WT_AOP_FLUSH,	/*!< Flush the operation queue */
	WT_AOP_INSERT,	/*!< Insert if key is not in the data source */
	WT_AOP_PUT,	/*!< Set the value for a key (unconditional) */
	WT_AOP_REMOVE,	/*!< Remove a key from the data source */
	WT_AOP_SEARCH,	/*!< Search and return key/value pair */
	WT_AOP_UPDATE	/*!< Set the value of an existing key */
} WT_ASYNC_OPTYPE;

#define	O2C(op)	((WT_CONNECTION_IMPL *)(op)->iface.connection)
#define	O2S(op)								\
    (((WT_CONNECTION_IMPL *)(op)->iface.connection)->default_session)
/*
 * WT_ASYNC_OP_IMPL --
 *	Implementation of the WT_ASYNC_OP.
 */
struct __wt_async_op_impl {
	WT_ASYNC_OP	iface;

	const char *uri;
	const char *config;
	uint64_t uri_hash;
	uint64_t cfg_hash;

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(wt_async_op) q;
	 */
	struct {
		WT_ASYNC_OP_IMPL *tqe_next;
		WT_ASYNC_OP_IMPL **tqe_prev;
	} q;				/* Linked list of WT_ASYNC_OPS. */

	uint64_t recno;			/* Record number, normal and raw mode */
	uint8_t raw_recno_buf[WT_INTPACK64_MAXSIZE];

	void	*lang_private;		/* Language specific private storage */

	WT_ITEM key, value;
	int saved_err;			/* Saved error in set_{key,value}. */
	WT_ASYNC_CALLBACK	*cb;

	uint32_t	internal_id;	/* Array position id. */
	uint64_t	unique_id;	/* Unique identifier. */

#define	WT_ASYNCOP_ENQUEUED	0x0001
#define	WT_ASYNCOP_FREE		0x0002
#define	WT_ASYNCOP_READY	0x0004
#define	WT_ASYNCOP_WORKING	0x0008
	uint32_t	state;		/* Op state */
	WT_ASYNC_OPTYPE	optype;		/* Operation type */

#define	WT_ASYNCOP_DATA_SOURCE	0x0001
#define	WT_ASYNCOP_DUMP_HEX	0x0002
#define	WT_ASYNCOP_DUMP_PRINT	0x0004
#define	WT_ASYNCOP_KEY_EXT	0x0008	/* Key points out of the tree. */
#define	WT_ASYNCOP_KEY_INT	0x0010	/* Key points into the tree. */
#define	WT_ASYNCOP_KEY_SET	(WT_CURSTD_KEY_EXT | WT_CURSTD_KEY_INT)
#define	WT_ASYNCOP_OVERWRITE	0x0020
#define	WT_ASYNCOP_RAW		0x0040
#define	WT_ASYNCOP_VALUE_EXT	0x0080	/* Value points out of the tree. */
#define	WT_ASYNCOP_VALUE_INT	0x0100	/* Value points into the tree. */
#define	WT_ASYNCOP_VALUE_SET	(WT_CURSTD_VALUE_EXT | WT_CURSTD_VALUE_INT)
	uint32_t flags;
#endif
};
/*
 * Definition of the async subsystem.
 */
typedef struct {
#define	WT_ASYNC_MAX_OPS	4096
	WT_ASYNC_OP_IMPL	 async_ops[WT_ASYNC_MAX_OPS];
					/* Async ops */
#define	OPS_INVALID_INDEX	0xffffffff
	uint32_t		 ops_index;	/* Active slot index */
	uint64_t		 op_id;
	/*
	 * Synchronization resources
	 */
	WT_SPINLOCK		 ops_lock;      /* Locked: ops array */
	WT_SPINLOCK		 opsq_lock;	/* Locked: work queue */
	SLIST_HEAD(__wt_async_lh, __wt_async_op) oplh;

	/* Notify any waiting threads when work is enqueued. */
	WT_CONDVAR		*ops_cond;
	WT_CONDVAR		*flush_cond;
	uint32_t		 flush_count;
	uint32_t		 flush_id;

#define	WT_ASYNC_MAX_WORKERS	20
	WT_SESSION_IMPL		*worker_sessions[WT_ASYNC_MAX_WORKERS];
					/* Async worker threads */
	pthread_t		 worker_tids[WT_ASYNC_MAX_WORKERS];

#define	WT_ASYNC_FLUSHING	0x0001
	uint32_t		 flags;
} WT_ASYNC;

/*
 * WT_ASYNC_WORKER_ARGS --
 *	State for an async worker thread.
 */
struct __wt_async_worker_args {
	u_int id;
};
