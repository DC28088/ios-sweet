// sweet-sysctl: read or write integer sysctls (the phone has no sysctl(8)).
// Usage: sweet-sysctl name [name=value ...]   value: decimal or 0x hex (int or uint32)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

static int show(const char *name) {
    int64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0) { perror(name); return 1; }
    if (len == 4) {
        int32_t i = (int32_t)v;
        printf("%s: %d (%#x)\n", name, i, (uint32_t)i);
    } else {
        printf("%s: %lld (%#llx)\n", name, (long long)v, (unsigned long long)v);
    }
    return 0;
}

int main(int argc, char **argv) {
    int rc = 0;
    if (argc < 2) { fprintf(stderr, "usage: %s name[=value] ...\n", argv[0]); return 2; }
    for (int i = 1; i < argc; i++) {
        char name[256];
        const char *eq = strchr(argv[i], '=');
        size_t n = eq ? (size_t)(eq - argv[i]) : strlen(argv[i]);
        if (n == 0 || n >= sizeof(name)) { fprintf(stderr, "bad name: %s\n", argv[i]); rc = 1; continue; }
        memcpy(name, argv[i], n);
        name[n] = 0;
        if (eq) {
            char *end = NULL;
            long long parsed = strtoll(eq + 1, &end, 0);
            if (end == eq + 1 || *end) { fprintf(stderr, "bad value: %s\n", eq + 1); rc = 1; continue; }
            int32_t v = (int32_t)parsed;
            if (sysctlbyname(name, NULL, NULL, &v, sizeof(v)) != 0) { perror(name); rc = 1; continue; }
        }
        rc |= show(name);
    }
    return rc;
}
