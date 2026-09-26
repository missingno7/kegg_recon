extern int g_e200;
extern int g_e204;
extern unsigned char g_e208;
extern int g_e209;
extern int g_e20d;
extern int g_e219;
extern int g_e21d;
extern int g_e221;
extern int g_e225;

int f_b17e(int *index, char **cursor)
{
    --*index;
    if (*index <= 0) {
        *cursor += 8;
        if ((*index = *(int *)(*cursor + 4)) < 0) {
            *cursor += *index * 8;
            *index = *(int *)(*cursor + 4);
        }
    }
    return *(int *)*cursor;
}
