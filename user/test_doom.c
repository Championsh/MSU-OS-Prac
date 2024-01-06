#include <inc/lib.h>

void
umain(int argc, char **argv) {
    printf("Testing string funcs:\n");
    int i = -3;
    char c = 'a';
    char a[9] = "abcdzbcd", b[5] = "zbcd";
    printf("%d\n", abs(i));
    printf("%c\n", toupper(c));
    printf("%c\n", tolower(toupper(c)));
    printf("%s\n", strstr(a, b));
    puts("abcd");
    printf("strdup: (empty string) %s (normal string) %s\n\n", strdup(""), strdup(a));

    printf("Testing file funcs (tell, seek-to-end):\n");
    struct Stat st;
    stat("/doom1.wad", &st);
    printf("size: %d\n", st.st_size);
    int fd = open("/doom1.wad", O_RDONLY);
    seek(fd, 3);
    printf("seek 3 tell: %d\n", tell(fd));
    seek(fd, 5);
    printf("seek 5 tell: %d\n", tell(fd));
    seekfromcur(fd, 5);
    printf("seekfromcur 5 tell: %d\n", tell(fd));
    seektoend(fd);
    printf("seektoend tell: %d\n", tell(fd));
    void *buf = malloc(100000);
    seek(fd, 0);
    size_t rd = read(fd, buf, 9234);
    printf("read: %lu\n\n", rd);
    close(fd);
    free(buf);

    printf("Testing malloc/free:\n");
    char *ptr = malloc(1024000), *oldptr = NULL;
    printf("ptr: %p\n", ptr);
    ptr[0] = 'a', ptr[1] = 'b', ptr[2] = 0;
    printf("%s\n", ptr);
    oldptr = ptr;
    ptr = malloc(123);
    printf("ptr: %p\n", ptr);
    ptr[0] = 1;
    free(ptr);
    ptr = malloc(8);
    printf("ptr: %p\n\n", ptr);
    ptr[0] = 1;
    free(oldptr);
    free(ptr);

    printf("Testing calloc and realloc:\n");
    ptr = calloc(3, 2);
    printf("ptr: %p\n", ptr);
    ptr[0] = 1;
    free(ptr);
    ptr = malloc(123);
    ptr[0] = 1;
    ptr[122] = 101;
    printf("ptr: %p\n", ptr);
    oldptr = ptr;
    ptr = realloc(ptr, 600 * 400 * 4);
    ptr[600 * 400 * 4 - 1] = 123;
    printf("new ptr: %p, old ptr: %p, checksum1: %d, checksum2: %d\n\n", ptr, oldptr, ptr[600 * 400 * 4 - 1], ptr[122]);
    free(ptr);

    printf("Tick test.\n");
    printf("Get ticks1: %lu\n", get_ticks());
    printf("Wait 1000ms\n");
    sleep(1000);
    printf("Get ticks2: %lu\n\n", get_ticks());

    printf("Tesing poll keys (press q to exit):\n");
    char sym;
    while((sym = poll_kbd()) != 'q')
        if(sym != -1)
            printf("Read key: %u\n", sym & 0x80 ? (uint8_t)(sym) ^ 0x80 : (uint8_t)sym);
    printf("\n");
}
