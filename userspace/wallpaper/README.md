# iOS16 wallpaper registration

`register.m` repairs only PosterKit provider registration, using checked Objective-C
selectors from the iOS16 LaunchServices/ExtensionFoundation interfaces. The API
reference is [ios-flame's phone tools](https://github.com/NWylynko/ios-flame/tree/745c4341af86089efb0df851fa8a27fb8face624/phone/tools).
It does not use that reference's hardcoded instruction-offset call.

The mobile-context launch job runs once per boot, with a 90-second alarm and
at most eight registration attempts. It registers PaperBoard and the installed
`com.apple.posterkit.provider` bundles supplied in
`/usr/local/share/sweet-wallpaper/providers.plist`. It checks that both
CollectionsPoster and LegacyPoster exist in LaunchServices and that the
ExtensionFoundation provider query is nonempty. SPI return values alone never
count as success. Diagnostics append to
`/private/var/mobile/Library/Logs/sweet-wallpaper.log`.

Build with the `sweet-wallpaper` GitHub workflow. Deploy the signed executable
as `/usr/local/libexec/sweet-wallpaper-register` (root:wheel,0755). The launch job
uses `UserName=mobile`, `GroupName=mobile`, `RunAtLoad=true`, `ProcessType=Background`,
and no `KeepAlive` or launchd stdout/stderr paths. Add the same job to the merged
`/System/Library/xpc/launchd.plist` LaunchDaemons dictionary as well as its standalone
plist. iOS16 uses the merged dictionary during bootstrap.

The root also needs Sailfish's ExtensionKit
`enforce_xpc_cache_code_signing.Enabled=false` override because these ports lack
trust-cache coverage for extension-point definitions. Preserve any other feature
flags. Do not remove existing migration preferences or trigger full migration.

Prepare wallpaper images separately with `tools/prepare-sweet-wallpapers.py`.
Keep the HEIC originals, install the validated PNG twins and updated CAML files,
and verify all original source hashes against the current root first. Registration
and PNG conversion address separate failures. The resulting wallpaper rendering
still needs observation on the device; compilation and registration are not proof
that wallpaper is visible.

For raw userdata installation, retain every changed original block, verify a
local filesystem overlay, freshly compare device blocks, obtain the approval
required by AGENTS.md, and verify every written chunk by reading it back.
