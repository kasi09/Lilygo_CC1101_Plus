#pragma once
#include <Preferences.h>

namespace RemoteStorage {
constexpr const char* Partition = "remotes";

// Keep remote blobs separate from the small Wi-Fi/settings NVS partition.
// Copy legacy records once; never erase or modify their original namespace.
template <typename Record, unsigned Slots, typename Validator>
bool begin(Preferences& destination, const char* name,
           Record (&records)[Slots], Validator valid) {
  if (!destination.begin(name, false, Partition)) return false;
  if (!destination.getBool("migrated", false)) {
    Preferences legacy;
    if (legacy.begin(name, true)) {
      for (unsigned slot = 0; slot < Slots; ++slot) {
        char key[8]; snprintf(key, sizeof(key), "s%u", slot);
        // A partially completed migration may already contain this record.
        if (destination.isKey(key)) continue;
        Record candidate;
        if (legacy.getBytesLength(key) != sizeof(candidate) ||
            legacy.getBytes(key, &candidate, sizeof(candidate)) != sizeof(candidate) ||
            !valid(candidate)) continue;
        if (destination.putBytes(key, &candidate, sizeof(candidate)) != sizeof(candidate))
          return false;
      }
      legacy.end();
    }
    // Only mark a completed copy. A failed write is retried on the next boot.
    if (destination.putBool("migrated", true) != 1) return false;
  }
  for (unsigned slot = 0; slot < Slots; ++slot) {
    char key[8]; snprintf(key, sizeof(key), "s%u", slot);
    Record candidate;
    if (destination.getBytesLength(key) == sizeof(candidate) &&
        destination.getBytes(key, &candidate, sizeof(candidate)) == sizeof(candidate) &&
        valid(candidate)) records[slot] = candidate;
  }
  return true;
}
}
