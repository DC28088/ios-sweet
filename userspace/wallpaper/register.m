/* Targeted iOS16 wallpaper registration. Runtime selectors are checked; no
 * dyld-cache addresses or private-method instruction offsets are assumed.
 * API references: NWylynko/ios-flame phone/tools/{lsreg,exq}.m (745c4341).
 */
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

@interface NSObject (SweetWallpaperSPI)
+ (id)defaultWorkspace;
+ (id)enumeratorWithOptions:(NSUInteger)options;
- (NSString *)bundleIdentifier;
- (void)registerPlugin:(NSURL *)url;
- (BOOL)registerApplication:(NSURL *)url;
- (id)initWithExtensionPointIdentifier:(NSString *)identifier;
+ (NSArray *)executeQuery:(id)query;
@end

static NSArray *queryProviders(void) {
    Class q = NSClassFromString(@"_EXQuery");
    Class c = NSClassFromString(@"_EXQueryController");
    if (![q instancesRespondToSelector:@selector(initWithExtensionPointIdentifier:)] ||
        ![c respondsToSelector:@selector(executeQuery:)]) return nil;
    id query = [[q alloc] initWithExtensionPointIdentifier:@"com.apple.posterkit.provider"];
    id result = [c executeQuery:query];
    return [result isKindOfClass:[NSArray class]] ? result : nil;
}

static BOOL registered(NSArray *results) {
    Class records = NSClassFromString(@"LSApplicationExtensionRecord");
    if (![records respondsToSelector:@selector(enumeratorWithOptions:)]) return NO;
    BOOL collections = NO, legacy = NO;
    for (id record in [records enumeratorWithOptions:0]) {
        if (![record respondsToSelector:@selector(bundleIdentifier)]) continue;
        NSString *identifier = [record bundleIdentifier];
        collections |= [identifier isEqualToString:@"com.apple.WallpaperKit.CollectionsPoster"];
        legacy |= [identifier isEqualToString:@"com.apple.PaperBoard.LegacyPoster"];
    }
    printf("SWEET WALLPAPER: query=%ld collections=%d legacy=%d\n",
           results ? (long)results.count : -1L, collections, legacy);
    return results.count > 0 && collections && legacy;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        /* Registration can block LaunchServices during early bootstrap. Never
         * mutate its database merely because this executable was launched.
         * --register is reserved for an explicitly controlled diagnostic run;
         * the installed launch job must remain Disabled after the 0121 stall.
         */
        BOOL repair = argc == 2 && strcmp(argv[1], "--register") == 0;
        if (argc > 1 && !repair) return 1;
        /* Inspect the same LS context as the UI. */
        if (getuid() != 501) return 2;
        alarm(90);
        freopen("/private/var/mobile/Library/Logs/sweet-wallpaper.log", "a", stdout);
        setvbuf(stdout, NULL, _IOLBF, 0);
        printf("SWEET WALLPAPER: start uid=%d\n", getuid());
        if (!dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices", RTLD_NOW) ||
            !dlopen("/System/Library/Frameworks/ExtensionFoundation.framework/ExtensionFoundation", RTLD_NOW)) {
            puts("SWEET WALLPAPER: required framework unavailable"); return 3;
        }
        if (!repair) {
            @try {
                BOOL ok = registered(queryProviders());
                puts("SWEET WALLPAPER: query-only; no registration requested");
                return ok ? 0 : 7;
            } @catch (NSException *error) {
                printf("SWEET WALLPAPER: query exception %s\n", error.description.UTF8String);
                return 7;
            }
        }
        NSArray *paths = [NSArray arrayWithContentsOfFile:@"/usr/local/share/sweet-wallpaper/providers.plist"];
        if (!paths.count || paths.count > 64) return 4;
        /* Bounded attempts: failed registrations are retried on the next boot,
         * without a KeepAlive crash loop or a success marker based on SPI return. */
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            @try {
                if (registered(queryProviders())) {
                    puts("SWEET WALLPAPER: VERIFIED provider records and ExtensionFoundation query");
                    return 0;
                }
                Class cls = NSClassFromString(@"LSApplicationWorkspace");
                if (![cls respondsToSelector:@selector(defaultWorkspace)]) return 5;
                id ws = [cls defaultWorkspace];
                if (![ws respondsToSelector:@selector(registerPlugin:)] ||
                    ![ws respondsToSelector:@selector(registerApplication:)]) return 6;
                NSString *host = @"/Applications/PaperBoard.app";
                if ([[NSFileManager defaultManager] fileExistsAtPath:host])
                    printf("SWEET WALLPAPER: PaperBoard registration=%d\n",
                           [ws registerApplication:[NSURL fileURLWithPath:host]]);
                for (NSString *path in paths) {
                    if (![path isKindOfClass:[NSString class]] ||
                        (![path hasPrefix:@"/System/Library/"] && ![path hasPrefix:@"/Applications/"]) ||
                        ![path hasSuffix:@".appex"] || [path containsString:@".."] ||
                        ![[NSFileManager defaultManager] fileExistsAtPath:path]) continue;
                    [ws registerPlugin:[NSURL fileURLWithPath:path]];
                    printf("SWEET WALLPAPER: requested %s\n", path.UTF8String);
                }
            } @catch (NSException *error) {
                printf("SWEET WALLPAPER: exception %s\n", error.description.UTF8String);
            }
            sleep(5);
        }
        @try {
            if (registered(queryProviders())) {
                puts("SWEET WALLPAPER: VERIFIED provider records and ExtensionFoundation query");
                return 0;
            }
        } @catch (NSException *error) {
            printf("SWEET WALLPAPER: final query exception %s\n", error.description.UTF8String);
        }
        puts("SWEET WALLPAPER: registration remains unverified; no success recorded");
        return 7;
    }
}
