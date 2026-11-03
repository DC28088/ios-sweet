# Privacy and diagnostic data

Never commit or upload raw device/SIM identifiers (IMEI, IMSI, ICCID, MSISDN,
serials), phone numbers, contacts, call/SMS history, carrier/account details,
cell IDs, LAC/TAC, GPS coordinates, personal SSIDs/BSSIDs, credentials, private
keys, or raw cellular/device dumps. Keep sensitive captures local, outside git,
and use placeholders or aggregate technical results in public notes.

Build artifacts and Actions logs must not include device dumps or private data.
Review staged diffs before pushing, including documentation and commit messages.

On 2026-11-03 the public history was rewritten to remove device-specific network,
location and call-test observations. Existing clones must not merge or push the
old history back. Reclone, or transplant reviewed code changes onto a freshly
fetched branch. Do not publish the old commit map or unredacted diagnostic files.
