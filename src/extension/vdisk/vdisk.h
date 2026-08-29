#ifndef VDISK_EXTENSION_H
#define VDISK_EXTENSION_H

#include "extension/extension.h"

extern int vdisk_callback(Extension *extension, ExtensionEvent event,
			  intptr_t data1, intptr_t data2);
extern const char *vdisk_get_cache_dir(void);
extern void vdisk_set_persistent(bool persistent);
extern void vdisk_set_setup_paths(bool enable);
extern void vdisk_add_path_exclusion(const char *exclusion_str);
extern const char *vdisk_get_discovered_path_env(void);

#endif /* VDISK_EXTENSION_H */
