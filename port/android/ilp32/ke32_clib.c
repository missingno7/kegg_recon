/* ke32_clib.c - C library functions compiled INTO the ILP32 game world (Android).
 *
 * Variadic functions cannot cross the boundary between the 32-bit-pointer game world and
 * the 64-bit host: the arm64_32 convention passes variadic arguments on the stack in
 * 4-byte slots, the host's AAPCS64 in registers. printf and spawnlp are therefore
 * implemented here, in the game world's own calling convention, and hand only a finished
 * buffer (or a single pointer) to the host through scalar ke32_* entry points.
 *
 * Built by port/android/tools/ilp32_world.py --kind clib; see stdio.h next to it.
 */
#include <stdarg.h>
#include <stddef.h>

void ke32_host_write(const char *text, int length);
void ke32_spawn_refused(const char *path);

static int put(char *out, int cap, int n, char c)
{
    if (n < cap - 1)
        out[n] = c;
    return n + 1;
}

/* The formats the game uses (%s %u %x %X, widths and flags for safety): Watcom semantics,
 * 32-bit int and long. */
static int format(char *out, int cap, const char *fmt, va_list ap)
{
    int n = 0;
    while (*fmt) {
        char digits[36];
        int left = 0, zero = 0, plus = 0, space = 0, alt = 0, width = 0, precision = -1, nd = 0;
        int negative = 0, base = 10, upper = 0, pad, i;
        unsigned int value;
        const char *s;
        char c = *fmt++;
        if (c != '%') {
            n = put(out, cap, n, c);
            continue;
        }
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '#') alt = 1;
            else break;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    precision = precision * 10 + (*fmt++ - '0');
            }
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'N' || *fmt == 'F')
            fmt++;
        c = *fmt ? *fmt++ : 0;
        switch (c) {
        case 's':
            s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            for (nd = 0; s[nd] && (precision < 0 || nd < precision); nd++)
                ;
            pad = width > nd ? width - nd : 0;
            if (!left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
            for (i = 0; i < nd; i++)
                n = put(out, cap, n, s[i]);
            if (left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
            continue;
        case 'c':
            pad = width > 1 ? width - 1 : 0;
            if (!left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
            n = put(out, cap, n, (char)va_arg(ap, int));
            if (left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
            continue;
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            negative = v < 0;
            value = negative ? 0u - (unsigned int)v : (unsigned int)v;
            break;
        }
        case 'u': value = va_arg(ap, unsigned int); break;
        case 'x': value = va_arg(ap, unsigned int); base = 16; break;
        case 'X': value = va_arg(ap, unsigned int); base = 16; upper = 1; break;
        case 'o': value = va_arg(ap, unsigned int); base = 8; break;
        case 'p': value = (unsigned int)(size_t)va_arg(ap, void *); base = 16; upper = 1;
                  if (precision < 0) precision = 8; break;
        case '%': n = put(out, cap, n, '%'); continue;
        default:
            n = put(out, cap, n, '%');
            if (c)
                n = put(out, cap, n, c);
            continue;
        }
        do {
            unsigned d = value % (unsigned)base;
            digits[nd++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
            value /= (unsigned)base;
        } while (value);
        if (precision == 0 && nd == 1 && digits[0] == '0')
            nd = 0;
        {
            int zeros = precision > nd ? precision - nd : 0;
            int sign = negative || plus || space;
            int prefix = alt && base == 16 && nd ? 2 : 0;
            int total = nd + zeros + sign + prefix;
            if (zero && !left && precision < 0 && width > total) {
                zeros += width - total;
                total = width;
            }
            pad = width > total ? width - total : 0;
            if (!left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
            if (sign)
                n = put(out, cap, n, negative ? '-' : plus ? '+' : ' ');
            if (prefix) {
                n = put(out, cap, n, '0');
                n = put(out, cap, n, upper ? 'X' : 'x');
            }
            while (zeros-- > 0)
                n = put(out, cap, n, '0');
            while (nd > 0)
                n = put(out, cap, n, digits[--nd]);
            if (left)
                while (pad-- > 0)
                    n = put(out, cap, n, ' ');
        }
    }
    if (cap > 0)
        out[n < cap - 1 ? n : cap - 1] = 0;
    return n;
}

int ke32_printf(const char *fmt, ...)
{
    char buffer[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = format(buffer, (int)sizeof buffer, fmt, ap);
    va_end(ap);
    ke32_host_write(buffer, n < (int)sizeof buffer - 1 ? n : (int)sizeof buffer - 1);
    return n;
}

/* launch_print_order_form() spawns the DOS print utility: never start host programs. */
int ke32_spawnlp(int mode, const char *path, const char *arg0, ...)
{
    (void)mode;
    (void)arg0;
    ke32_spawn_refused(path);
    return -1;
}
