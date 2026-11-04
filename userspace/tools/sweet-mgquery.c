// sweet-mgquery: print MobileGestalt answers for capability keys.
// Read-only. Usage: sweet-mgquery [key ...]  (no args = cellular/telephony set)
// Do not pass identifier keys (serials, IMEI, ICCID...): output may end up in logs.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef const void *CFTypeRef;
typedef const void *CFStringRef;
typedef long CFIndex;

static const char *defaults[] = {
    "cellular-data", "telephony", "sms", "mms", "voice-control",
    "CellularTelephonyCapability", "CellularDataCapability", "HasBaseband",
    "BasebandChipset", "BasebandClass", "BasebandPostponementStatus",
    "BasebandStatus", "SIMStatus", "SIMTrayStatus", "ContainsCellularRadio",
    "contains-cellular-radio", "DeviceSupportsCellularData", "RegionalBehaviorNoVOIP",
    "DeviceClass", "ProductType", "HWModelStr", "DeviceName", "wifi", "gps",
    "DeviceSupportsDualSIM", "DeviceSupportseSIM", "SupportsDualSIM", "eSIM",
    "SIMCapability", "DeviceSupportsSingleSIM", "multi-sim", "dual-sim",
    "RequiresBasebandActivation", "DeviceRequiresPetForSigning", "cellular-ims",
    NULL};

int main(int argc, char **argv) {
    void *cf = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_NOW);
    void *mg = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_NOW);
    if (!cf || !mg) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }
    CFStringRef (*mkstr)(CFTypeRef, const char *, unsigned) = dlsym(cf, "CFStringCreateWithCString");
    CFStringRef (*desc)(CFTypeRef) = dlsym(cf, "CFCopyDescription");
    int (*getcs)(CFStringRef, char *, CFIndex, unsigned) = dlsym(cf, "CFStringGetCString");
    void (*rel)(CFTypeRef) = dlsym(cf, "CFRelease");
    CFTypeRef (*copy)(CFStringRef, void *) = dlsym(mg, "MGCopyAnswer");
    if (!mkstr || !desc || !getcs || !rel || !copy) { fprintf(stderr, "dlsym failed\n"); return 1; }
    const char **keys = argc > 1 ? (const char **)(argv + 1) : defaults;
    for (int i = 0; keys[i]; i++) {
        CFStringRef k = mkstr(NULL, keys[i], 0x08000100);
        CFTypeRef v = copy(k, NULL);
        char buf[512] = "(null)";
        if (v) {
            CFStringRef d = desc(v);
            if (!d || !getcs(d, buf, sizeof buf, 0x08000100)) strcpy(buf, "(undescribable)");
            if (d) rel(d);
            rel(v);
        }
        // Keep one line per key; drop newlines from descriptions.
        for (char *p = buf; *p; p++) if (*p == '\n') *p = ' ';
        printf("%s = %s\n", keys[i], buf);
        rel(k);
    }
    return 0;
}
