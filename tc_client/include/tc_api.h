/**
 * Client API of NFS Transactional Compounds (TC).
 *
 * Functions with "tc_" are general API, whereas functions with "tx_" are API
 * with transaction support.
 */
#ifndef __TC_API_H__
#define __TC_API_H__

#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __cplusplus
#define CONST const
extern "C" {
#else
#define CONST
#endif

enum TC_FILETYPE {
	TC_FILE_DESCRIPTOR = 1,
	TC_FILE_PATH,
	TC_FILE_HANDLE,
};

#define TC_FD_NULL -1
#define TC_FD_CWD -2
#define TC_FD_ABS -3

/**
 * "type" is one of the three file types; "fd" and "path_or_handle" depend on
 * the file type:
 *
 *	1. When "type" is TC_FILE_DESCRIPTOR, "fd" identifies the file we are
 *	operating on.
 *
 *	2. When "type" is TC_FILE_PATH, "fd" is the base file descriptor, and
 *	"path_or_handle" is the file path.  The file is identified by resolving
 *	the path relative to "fd".  In this case, "fd" has two special values:
 *	(a) TC_FDCWD which means the current working directory, and
 *	(b) TC_FDABS which means the "path_or_handle" is an absolute path.
 *
 *	3. When "type" is TC_FILE_HANDLE, "fd" is "mount_fd", and
 *	"path_or_handle" points to "struct file_handle".
 *
 * See http://man7.org/linux/man-pages/man2/open_by_handle_at.2.html
 */
typedef struct _tc_file
{
	int type;

	int fd;

	union
	{
		const char *path;
		const struct file_handle *handle;
	}; /* path_or_handle */
} tc_file;

tc_file tc_file_from_path(const char *pathname);

/**
 * Open a tc_file using path.  Similar to "openat(2)".
 *
 * NOTE: It is not necessary that a tc_file have to be open before reading
 * from/writing to it.  We recommend using tc_readv() and tc_writev() to
 * implicitly open a file when necessary.
 */
tc_file tc_open_by_path(int dirfd, const char *pathname, int flags,
			mode_t mode);

static inline tc_file tc_open(const char *pathname, int flags, mode_t mode)
{
	return tc_open_by_path(AT_FDCWD, pathname, flags, mode);
}

/**
 * Open a tc_file using file handle.  Similar to "open_by_handle_at(2)".
 */
tc_file tc_open_by_handle(int mount_fd, struct file_handle *fh, int flags);

/**
 * Close a tc_file if necessary.
 */
int tc_close(tc_file file);

/**
 * Represents an I/O vector of a file.
 *
 * The fields have different meaning depending the operation is read or write.
 * Most often, clients allocate an array of this struct.
 */
struct tc_iovec
{
	tc_file file;
	size_t offset; /* IN: read/write offset */

	/**
	 * IN:  # of bytes of requested read/write
	 * OUT: # of bytes successfully read/written
	 */
	size_t length;

	/**
	 * This data buffer should always be allocated by caller for either
	 * read or write, and the length of the buffer should be indicated by
	 * the "length" field above.
	 *
	 * IN:  data requested to be written
	 * OUT: data successfully read
	 */
	void *data;

	unsigned int is_creation : 1; /* IN: create file if not exist? */
	unsigned int is_failure : 1;  /* OUT: is this I/O a failure? */
	unsigned int is_eof : 1;      /* OUT: does this I/O reach EOF? */
};

/**
 * Result of a TC operation.
 *
 * When transaction is not enabled, compound processing stops upon the first
 * failure.
 */
typedef struct _tc_res
{
	bool okay;  /* no error */
	int index;  /* index of the first failed operation */
	int err_no; /* error number of the failed operation */
} tc_res;

/**
 * Read from one or more files.
 *
 * @reads: the tc_iovec array of read operations.  "path" of the first array
 * element must not be NULL; a NULL "path" of any other array element means
 * using the same "path" of the preceding array element.
 * @count: the count of reads in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_readv(struct tc_iovec *reads, int count, bool is_transaction);

static inline bool tx_readv(struct tc_iovec *reads, int count)
{
	tc_res res = tc_readv(reads, count, true);
	return res.okay;
}

/**
 * Write to one or more files.
 *
 * @writes: the tc_iovec array of write operations.  "path" of the first array
 * element must not be NULL; a NULL "path" of any other array element means
 * using the same "path"
 * @count: the count of writes in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_writev(struct tc_iovec *writes, int count, bool is_transaction);

static inline bool tx_writev(struct tc_iovec *writes, int count)
{
	tc_res res = tc_writev(writes, count, true);
	return res.okay;
}

/**
 * The bitmap indicating the presence of file attributes.
 */
struct tc_attrs_masks
{
	unsigned int has_mode : 1;  /* protection flags */
	unsigned int has_size : 1;  /* file size, in bytes */
	unsigned int has_nlink : 1; /* number of hard links */
	unsigned int has_uid : 1;   /* user ID of owner */
	unsigned int has_gid : 1;   /* group ID of owner */
	unsigned int has_rdev : 1;  /* device ID of block or char special
				   files */
	unsigned int has_atime : 1; /* time of last access */
	unsigned int has_mtime : 1; /* time of last modification */
	unsigned int has_ctime : 1; /* time of last status change */
};

/**
 * File attributes.  See stat(2).
 */
struct tc_attrs
{
	tc_file file;
	struct tc_attrs_masks masks;
	mode_t mode;   /* protection */
	size_t size;   /* file size, in bytes */
	nlink_t nlink; /* number of hard links */
	uid_t uid;
	gid_t gid;
	dev_t rdev;
	time_t atime;
	time_t mtime;
	time_t ctime;
};

/**
 * Get attributes of file objects.
 *
 * @attrs: array of attributes to get
 * @count: the count of tc_attrs in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_getattrsv(struct tc_attrs *attrs, int count, bool is_transaction);

static inline bool tx_getattrsv(struct tc_attrs *attrs, int count)
{
	tc_res res = tc_getattrsv(attrs, count, true);
	return res.okay;
}

/**
 * Set attributes of file objects.
 *
 * @attrs: array of attributes to set
 * @count: the count of tc_attrs in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_setattrsv(struct tc_attrs *attrs, int count, bool is_transaction);

static inline bool tx_setattrsv(struct tc_attrs *attrs, int count)
{
	tc_res res = tc_setattrsv(attrs, count, true);
	return res.okay;
}

/**
 * List the content of a directory.
 *
 * @dir [IN]: the path of the directory to list
 * @masks [IN]: masks of attributes to get for listed objects
 * @max_count [IN]: the maximum number of count to list
 * @contents [OUT]: the pointer to the array of files/directories in the
 * directory.  The array and the paths in the array will be allocated
 * internally by this function; the caller is responsible for releasing the
 * memory, probably by using tc_free_attrs().
 */
tc_res tc_listdir(const char *dir, struct tc_attrs_masks masks, int max_count,
		  struct tc_attrs **contents, int *count);

/**
 * Free an array of "tc_attrs".
 *
 * @attrs [IN]: the array to be freed
 * @count [IN]: the length of the array
 * @free_path [IN]: whether to free the paths in "tc_attrs" as well.
 */
void tc_free_attrs(struct tc_attrs *attrs, int count, bool free_path);

typedef struct tc_file_pair
{
	tc_file src_file;
	tc_file dst_file;
} tc_file_pair;

/**
 * Rename the file from "src_path" to "dst_path" for each of "pairs".
 *
 * @pairs: the array of file pairs to be renamed
 * @count: the count of the preceding "tc_file_pair" array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_renamev(struct tc_file_pair *pairs, int count, bool is_transaction);

static inline bool tx_renamev(tc_file_pair *pairs, int count)
{
	tc_res res = tc_renamev(pairs, count, true);
	return res.okay;
}

tc_res tc_removev(tc_file *files, int count, bool is_transaction);

static inline bool tx_removev(tc_file *files, int count)
{
	tc_res res = tc_removev(files, count, true);
	return res.okay;
}

tc_res tc_mkdirv(tc_file *dir, mode_t *mode, int count, bool is_transaction);

static inline bool tx_mkdirv(tc_file *dir, mode_t *mode, int count,
			     bool is_transaction)
{
	tc_res res = tc_mkdirv(dir, mode, count, is_transaction);
	return res.okay;
}

struct tc_extent_pair
{
	const char *src_path;
	const char *dst_path;
	size_t src_offset;
	size_t dst_offset;
	size_t length;
};

/**
 * Copy the file from "src_path" to "dst_path" for each of "pairs".
 *
 * @pairs: the array of file extent pairs to copy
 * @count: the count of the preceding "tc_extent_pair" array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_copyv(struct tc_extent_pair *pairs, int count, bool is_transaction);

static inline bool tx_copyv(struct tc_extent_pair *pairs, int count)
{
	tc_res res = tc_copyv(pairs, count, true);
	return res.okay;
}

/**
 * Application data blocks (ADB).
 *
 * See https://tools.ietf.org/html/draft-ietf-nfsv4-minorversion2-39#page-60
 */
struct tc_adb
{
	const char *path;

	/**
	 * The offset within the file the ADB blocks should start.
	 */
	size_t adb_offset;

	/**
	 * size (in bytes) of an ADB block
	 */
	size_t adb_block_size;

	/**
	 * IN: requested number of ADB blocks to write
	 * OUT: number of ADB blocks successfully written.
	 */
	size_t adb_block_count;

	/**
	 * Relative offset within an ADB block to write then Application Data
	 * Block Number (ADBN).
	 *
	 * A value of UINT64_MAX means no ADBN to write.
	 */
	size_t adb_reloff_blocknum;

	/**
	 * The Application Data Block Number (ADBN) of the first ADB.
	 */
	size_t adb_block_num;

	/**
	 * Relative offset of the pattern within an ADB block.
	 *
	 * A value of UINT64_MAX means no pattern to write.
	 */
	size_t adb_reloff_pattern;

	/**
	 * Size and value of the ADB pattern.
	 */
	size_t adb_pattern_size;
	void *adb_pattern_data;
};

/**
 * Write Application Data Blocks (ADB) to one or more files.
 *
 * @patterns: the array of ADB patterns to write
 * @count: the count of the preceding pattern array
 * @is_transaction: whether to execute the compound as a transaction
 */
tc_res tc_write_adb(struct tc_adb *patterns, int count, bool is_transaction);

static inline bool tx_write_adb(struct tc_adb *patterns, int count)
{
	tc_res res = tc_write_adb(patterns, count, true);
	return res.okay;
};

#ifdef __cplusplus
#undef CONST
}
#else
#undef CONST
#endif

#endif // __TC_API_H__