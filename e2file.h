#ifndef E2FILE_H
#define E2FILE_H
/*
 * - Pre-size everything; allocate an array of control structures
 *   with initialisation. Note that, with 8K buffers, 200 buffers is
 *   1.6 MBytes! It also exceeds the initial size of a Supernat database.
 * - Do not worry about locking at the moment.
 * - Requires exclusive access to files, really, because no guarantee when
 *   a file will get closed.
 *
 * Particular Functions:
 * - E2_CON * e2init(char * logfile, int no_of_files, int no_of_buffers)
 *   -  returns a pointer to a control block
 *   -  keeps a current control block, so that multiple blocks are
 *      possible (although cannot see the point of this at the moment)
 *   -  E2_FILE will point to applicable control block, just in case.
 *   -  The control blocks are chained together for the benefit of e2finish()
 *   -  e2init() registers e2finish() with atexit()
 * - void e2finish(void)
 *   -  calls e2fclose() on all open files
 * - void e2switch(E2_CON * ncon)
 *   -  switches current block
 * - void e2commit(E2_CON * ncon)
 *   -  Writes a commitment marker to a log of transactions.
 * Stdio Analogues:
 * - E2_FILE * e2fopen(char * file_name, char * mode)
 * - E2_FILE * e2fdopen(int fd, char * mode)
 * - char * e2fgets(char * buffer, size_t size, E2_FILE * fp)
 * - size_t e2fread(char * buffer, size_t size, size_t n_els, E2_FILE * fp)
 * - size_t e2fwrite(char * buffer, size_t size, size_t n_els, E2_FILE * fp)
 * - int e2fileno(E2_FILE *fp) 
 * - int e2ferror(E2_FILE *fp) 
 * - int e2feof(E2_FILE *fp) 
 * - int e2clearerr(E2_FILE *fp) 
 * - int e2fseek(E2_FILE * fp, long offset, int ptrname)
 * - void e2rewind(E2_FILE * fp)
 * - long e2ftell(E2_FILE * fp)
 * - int e2fflush(E2_FILE * fp)
 * - int e2fclose(E2_FILE * fp)
 * Notes:
 * - We are not doing anything about locking at the moment.
 * - getc(), ungetc(),  fprintf() are ignored for the moment
 * - We do not allow the file to be anything other than a regular file
 *   at present
 */
#include <stdarg.h>
/*
 * Data structures:
 */
typedef struct _e2_con {
    char * logfile;   /* Name of file to use for logging */
    int f_cnt;        /* Number of files */
    int buf_cnt;      /* Number of buffers    */
    struct _e2_file * fp;    /* Array of file headers */
    struct _e2_ino * ip;     /* Array of different filenames in use */
    struct _e2_buf_hdr * bhp; /* Array of buffer headers */
    char * bp;        /* Array of 8K buffers */
    int log_fd;       /* Log File Descriptor */
    char * log_where; /* Whereabouts in the log buffer we are */
    char log_buffer[65536]; /* Log buffer */
    struct _e2_buf_hdr * whp;     /* Write list head pointer */
    struct _e2_buf_hdr * wtp;     /* Write list tail pointer */
    struct _e2_buf_hdr * chp;     /* Clean list head pointer  */
    struct _e2_buf_hdr * ctp;     /* Clean list tail pointer  */
    HASH_CON * fil_hash;          /* Buffer/Ino hash table   */
    HASH_CON * buf_hash;          /* Buffer/Ino hash table   */
    struct _e2_con * next_e2_con; /* Chain pointer           */
} E2_CON;
/*
 * Each E2_FILE has its own position and error marker, but all
 * f2open()s of a single file share the same file descriptor
 */
typedef struct _e2_file {
    struct _e2_con * conp;
    struct _e2_ino * ip;    /* The applicable E2_INO structure */
    long fpos;              /* Seek position   */
    int cur_err;            /* Error indicator */
    int eof;                /* EOF indicator */
} E2_FILE;
/*
 * We do not chain blocks for a given file together; rather, they are
 * hashed by the ino+block number, and point back to the file.
 * We have no need to de-allocate blocks file by file.
 * For journal purposes, we need to log:
 * - allocations of _e2_ino (which map file names to handles )
 * - writes (_e2_ino, position, length, data)
 * - transaction markers.
 */
typedef struct _e2_ino {
    struct _e2_con * conp;
    char * fname;           /* Full path name of file */
    int fd;                 /* File descriptor */
    int flags;              /* Open flags value */
    int f_cnt;              /* Number of file references */
    int w_cnt;              /* Number of blocks with impending writes;
                             * cannot close the file until these have
                             * been done */
} E2_INO;
/*
 * Structure that administers a buffer. These should be self-administering.
 * All buffers are the same size (BUFSIZ).
 */
typedef struct _e2_buf_hdr {
    struct _e2_con * conp;
    struct _e2_ino * ip;          /* File that owns this, if there is one */
    struct _e2_buf_hdr * prev;    /* Prev list pointer  */
    struct _e2_buf_hdr * next;    /* Next list pointer  */
    long buf_no;                  /* Position of buffer in file */
    char * bp;                    /* Pointer to buffer base */
    char * ep;                    /* Pointer to first free (or beyond) */
    char * vp;                    /* Pointer to beyond the last valid item */
    char * wbp;                   /* Pointer to write base */
    char * wep;                   /* Pointer to write free (or beyond) */
} E2_BUF_HDR;
E2_CON * e2init ANSIARGS((char * logfile, int no_of_files, int no_of_buffers));
E2_FILE * e2fopen ANSIARGS((char * fname,char * mode));
E2_FILE * e2fdopen ANSIARGS((int fd, char * mode));
size_t e2fread ANSIARGS((char * buffer,size_t size,size_t n_els,E2_FILE * fp));
char * e2fgets ANSIARGS((char * buffer, size_t size, E2_FILE * fp));
size_t e2fwrite ANSIARGS((char * buffer,size_t size,size_t n_els,E2_FILE * fp));
size_t e2vfprintf(E2_FILE * fp, char *fmt, va_list arglist);
#define e2fileno(fp) ((fp)->ip->fd)
#define e2feof(fp) ((fp)->eof)
#define e2ferror(fp) ((fp)->cur_err)
#define e2clearerr(fp) ((fp)->cur_err = 0)
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
int e2fseek ANSIARGS((E2_FILE * fp, long offset, int ptrname));
int e2rewind ANSIARGS(( E2_FILE * fp));
#define  e2ftell(fp) (fp->fpos)
int e2fflush ANSIARGS(( E2_FILE * fp));
int e2fclose ANSIARGS(( E2_FILE * fp));
void e2switch ANSIARGS((E2_CON * ncon));
void e2commit ANSIARGS((void));
void e2restore ANSIARGS((FILE * logfp));
void e2finish ANSIARGS((void));
#endif
