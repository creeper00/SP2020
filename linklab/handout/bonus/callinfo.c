#define UNW_LOCAL_ONLY
#include <stdlib.h>
#include <libunwind.h>

int get_callinfo(char* fname, size_t fnlen, unsigned long long* ofs)
{
    unw_cursor_t cursor;
    unw_context_t context;
    unw_word_t off, ip, sp;

    if (unw_getcontext(&context)) return -1;
    if (unw_init_local(&cursor, &context)) return -1;

    unw_get_proc_name(&cursor, fname, fnlen, &off);
    while (!(fname[0] == 'm' && fname[1] == 'a' && fname[2] == 'i' && fname[3] == 'n' && fname[4] == '\0')) {
        unw_step(&cursor);
        unw_get_proc_name(&cursor, fname, fnlen, &off);
    }
    *ofs = (unsigned long long) off - 5;
    return 0;
}
